#include "velox/arena.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace velox {

MappedFile::~MappedFile() {
    close();
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_),
      size_(other.size_),
      fd_(other.fd_),
      is_mmap_(other.is_mmap_),
      fallback_buf_(std::move(other.fallback_buf_))
#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
      , win_file_(other.win_file_),
      win_map_(other.win_map_)
#endif
{
    if (!is_mmap_ && !fallback_buf_.empty()) {
        data_ = fallback_buf_.data();
    }
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
    other.is_mmap_ = false;
#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
    other.win_file_ = nullptr;
    other.win_map_ = nullptr;
#endif
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        close();
        data_ = other.data_;
        size_ = other.size_;
        fd_ = other.fd_;
        is_mmap_ = other.is_mmap_;
        fallback_buf_ = std::move(other.fallback_buf_);
#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
        win_file_ = other.win_file_;
        win_map_ = other.win_map_;
        other.win_file_ = nullptr;
        other.win_map_ = nullptr;
#endif
        if (!is_mmap_ && !fallback_buf_.empty()) {
            data_ = fallback_buf_.data();
        }
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
        other.is_mmap_ = false;
    }
    return *this;
}

bool MappedFile::open(const std::string& path, std::string* err) {
    close();

#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
    DWORD attrs = ::GetFileAttributesA(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if (err) *err = "Path is a directory: " + path;
        return false;
    }

    HANDLE hFile = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (err) *err = "Failed to open file: " + path;
        return false;
    }

    LARGE_INTEGER liSize;
    if (!::GetFileSizeEx(hFile, &liSize)) {
        if (err) *err = "Failed to stat file: " + path;
        ::CloseHandle(hFile);
        return false;
    }

    constexpr uint64_t kMaxMappedBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    if (liSize.QuadPart < 0 || static_cast<uint64_t>(liSize.QuadPart) > kMaxMappedBytes) {
        if (err) *err = "File size out of safe bounds (0..2 GiB): " + path;
        ::CloseHandle(hFile);
        return false;
    }

    if (liSize.QuadPart == 0) {
        data_ = "";
        size_ = 0;
        is_mmap_ = false;
        ::CloseHandle(hFile);
        return true;
    }

    size_ = static_cast<std::size_t>(liSize.QuadPart);
    HANDLE hMap = ::CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        fallback_buf_.resize(size_);
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile, fallback_buf_.data(), static_cast<DWORD>(size_), &bytesRead, nullptr)) {
            if (err) *err = "Failed to read file: " + path;
            ::CloseHandle(hFile);
            return false;
        }
        ::CloseHandle(hFile);
        size_ = static_cast<std::size_t>(bytesRead);
        data_ = fallback_buf_.data();
        is_mmap_ = false;
        return true;
    }

    void* mapped = ::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size_);
    if (!mapped) {
        ::CloseHandle(hMap);
        ::CloseHandle(hFile);
        if (err) *err = "Failed to map view of file: " + path;
        return false;
    }

    data_ = static_cast<const char*>(mapped);
    is_mmap_ = true;
    win_file_ = hFile;
    win_map_ = hMap;
    return true;

#else
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        if (err) *err = "Failed to open file: " + path;
        return false;
    }

    struct stat st {};
    if (::fstat(fd_, &st) < 0) {
        if (err) *err = "Failed to stat file: " + path;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (!S_ISREG(st.st_mode)) {
        if (err) *err = "Path is not a regular file: " + path;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    constexpr off_t kMaxMappedBytes = static_cast<off_t>(2ULL * 1024ULL * 1024ULL * 1024ULL);
    if (st.st_size < 0 || st.st_size > kMaxMappedBytes) {
        if (err) *err = "File size out of safe bounds (0..2 GiB): " + path;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (st.st_size == 0) {
        data_ = "";
        size_ = 0;
        is_mmap_ = false;
        return true;
    }

    size_ = static_cast<std::size_t>(st.st_size);
    void* mapped = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapped == MAP_FAILED) {
        // Fallback to standard read if mmap is unsupported on this descriptor
        fallback_buf_.resize(size_);
        ssize_t n = ::read(fd_, fallback_buf_.data(), size_);
        if (n < 0) {
            if (err) *err = "Failed to read file: " + path;
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        size_ = static_cast<std::size_t>(n);
        data_ = fallback_buf_.data();
        is_mmap_ = false;
        return true;
    }

    ::madvise(mapped, size_, MADV_SEQUENTIAL);
    data_ = static_cast<const char*>(mapped);
    is_mmap_ = true;
    return true;
#endif
}

void MappedFile::loadFromMemory(std::string data) {
    close();
    fallback_buf_ = std::move(data);
    data_ = fallback_buf_.data();
    size_ = fallback_buf_.size();
    is_mmap_ = false;
}

void MappedFile::close() {
#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
    if (is_mmap_ && data_) {
        ::UnmapViewOfFile(data_);
    }
    if (win_map_) {
        ::CloseHandle(win_map_);
        win_map_ = nullptr;
    }
    if (win_file_ && win_file_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(win_file_);
        win_file_ = nullptr;
    }
#else
    if (is_mmap_ && data_ && size_ > 0) {
        ::munmap(const_cast<char*>(data_), size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
    data_ = nullptr;
    size_ = 0;
    is_mmap_ = false;
    fallback_buf_.clear();
}

MonotonicArena::MonotonicArena(std::size_t block_size)
    : default_block_size_(std::max<std::size_t>(block_size, 4096)) {
    Block b;
    b.capacity = default_block_size_;
    b.memory = std::make_unique<uint8_t[]>(b.capacity);
    b.offset = 0;
    blocks_.push_back(std::move(b));
}

void* MonotonicArena::allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) bytes = 1;
    if (alignment == 0) alignment = 1;

    for (auto& b : blocks_) {
        uintptr_t raw = reinterpret_cast<uintptr_t>(b.memory.get() + b.offset);
        uintptr_t aligned = (raw + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
        std::size_t pad = static_cast<std::size_t>(aligned - raw);
        if (b.offset + pad + bytes <= b.capacity) {
            b.offset += pad + bytes;
            total_allocated_ += bytes;
            return reinterpret_cast<void*>(aligned);
        }
    }

    std::size_t new_cap = std::max(default_block_size_, bytes + alignment + 64);
    Block nb;
    nb.capacity = new_cap;
    nb.memory = std::make_unique<uint8_t[]>(new_cap);
    uintptr_t raw = reinterpret_cast<uintptr_t>(nb.memory.get());
    uintptr_t aligned = (raw + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
    std::size_t pad = static_cast<std::size_t>(aligned - raw);
    nb.offset = pad + bytes;
    void* ptr = reinterpret_cast<void*>(aligned);
    blocks_.push_back(std::move(nb));
    total_allocated_ += bytes;
    return ptr;
}

void MonotonicArena::reset() noexcept {
    for (auto& b : blocks_) {
        b.offset = 0;
    }
    total_allocated_ = 0;
}

} // namespace velox
