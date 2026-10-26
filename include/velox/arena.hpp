#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velox {

/**
 * RAII POSIX memory-mapped file reader with zero-copy string_view slicing.
 * Falls back gracefully to heap buffer for streams or small synthetic inputs.
 */
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    bool open(const std::string& path, std::string* err = nullptr);
    void loadFromMemory(std::string data);
    void close();

    [[nodiscard]] std::string_view view() const noexcept {
        return {data_, size_};
    }
    [[nodiscard]] const uint8_t* bytes() const noexcept {
        return reinterpret_cast<const uint8_t*>(data_);
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool isMapped() const noexcept { return is_mmap_; }

private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
    int fd_ = -1;
    bool is_mmap_ = false;
    std::string fallback_buf_;
#if defined(_WIN32) || defined(PLATFORM_WINDOWS)
    void* win_file_ = nullptr;
    void* win_map_ = nullptr;
#endif
};

/**
 * Cache-line aligned (64-byte) monotonic bump arena allocator.
 * Eliminates per-row malloc/free overhead during high-speed parsing.
 */
class alignas(64) MonotonicArena {
public:
    explicit MonotonicArena(std::size_t block_size = 1024 * 1024);
    ~MonotonicArena() = default;

    MonotonicArena(const MonotonicArena&) = delete;
    MonotonicArena& operator=(const MonotonicArena&) = delete;

    void* allocate(std::size_t bytes, std::size_t alignment = 8);
    void reset() noexcept;

    [[nodiscard]] std::size_t totalAllocatedBytes() const noexcept {
        return total_allocated_;
    }

private:
    struct Block {
        std::unique_ptr<uint8_t[]> memory;
        std::size_t capacity = 0;
        std::size_t offset = 0;
    };
    std::size_t default_block_size_;
    std::size_t total_allocated_ = 0;
    std::vector<Block> blocks_;
};

} // namespace velox
