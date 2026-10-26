#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace velox {

// ─────────────────────────────────────────────────────────────────────────────
// C++20 Concepts & Span Utilities (CPPDEV.md)
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
concept QuantitativeScalar = std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>);

template <QuantitativeScalar T>
[[nodiscard]] constexpr double computeSpanMean(std::span<const T> values) noexcept {
    if (values.empty()) [[unlikely]] {
        return 0.0;
    }
    double acc = 0.0;
    for (T v : values) {
        acc += static_cast<double>(v);
    }
    return acc / static_cast<double>(values.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// RAII FileHandle Wrapper (memcpp.md — Resource Acquisition Is Initialization)
// ─────────────────────────────────────────────────────────────────────────────
class FileHandle {
    std::FILE* handle_{nullptr};

public:
    explicit FileHandle(const char* path, const char* mode)
        : handle_(std::fopen(path, mode)) {
        if (!handle_) {
            throw std::runtime_error(std::string("Failed to open file: ") + path);
        }
    }

    ~FileHandle() noexcept {
        if (handle_) {
            std::fclose(handle_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FileHandle(FileHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                std::fclose(handle_);
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] std::FILE* get() const noexcept { return handle_; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Fixed-Block PoolAllocator (memcpp.md — Custom Pool Allocator)
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t BlockSize = 4096>
class PoolAllocator {
    union Slot {
        alignas(T) std::array<std::byte, sizeof(T)> storage;
        Slot* next;
    };

    Slot* free_list_{nullptr};
    std::vector<std::unique_ptr<Slot[]>> blocks_;
    std::size_t active_allocations_{0};

public:
    PoolAllocator() = default;
    ~PoolAllocator() = default;

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        if (!free_list_) [[unlikely]] {
            allocateBlock();
        }
        Slot* slot = free_list_;
        free_list_ = slot->next;
        ++active_allocations_;
        return std::construct_at(reinterpret_cast<T*>(&slot->storage), std::forward<Args>(args)...);
    }

    void destroy(T* ptr) noexcept {
        if (!ptr) return;
        std::destroy_at(ptr);
        Slot* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = free_list_;
        free_list_ = slot;
        --active_allocations_;
    }

    [[nodiscard]] std::size_t activeCount() const noexcept { return active_allocations_; }
    [[nodiscard]] std::size_t blockCount() const noexcept { return blocks_.size(); }

private:
    void allocateBlock() {
        constexpr std::size_t count = std::max<std::size_t>(16, BlockSize / sizeof(Slot));
        auto new_block = std::make_unique<Slot[]>(count);
        for (std::size_t i = 0; i + 1 < count; ++i) {
            new_block[i].next = &new_block[i + 1];
        }
        new_block[count - 1].next = free_list_;
        free_list_ = &new_block[0];
        blocks_.push_back(std::move(new_block));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Cache-Line Aligned Counter & Structure-of-Arrays (optimcpop.md)
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(64) CacheAlignedCounter {
    std::atomic<uint64_t> count{0};
    uint8_t padding[64 - sizeof(std::atomic<uint64_t>)]{};
};

struct TelemetrySeriesSoA {
    std::vector<double> timestamps;
    std::vector<double> latencies_ms;
    std::vector<double> revenues_usd;
    std::vector<uint32_t> region_ids;

    void reserve(std::size_t n) {
        timestamps.reserve(n);
        latencies_ms.reserve(n);
        revenues_usd.reserve(n);
        region_ids.reserve(n);
    }

    [[nodiscard]] std::size_t size() const noexcept { return latencies_ms.size(); }

    // Vectorized scale transform over contiguous SoA array
    void scaleRevenues(double factor) noexcept {
        const std::size_t n = revenues_usd.size();
        double* __restrict ptr = revenues_usd.data();
        for (std::size_t i = 0; i < n; ++i) {
            ptr[i] *= factor;
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Work-Stealing / Condition-Variable ThreadPool (optimcpop.md)
// ─────────────────────────────────────────────────────────────────────────────
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};

public:
    explicit ThreadPool(std::size_t threads = std::max<std::size_t>(1, std::thread::hardware_concurrency())) {
        workers_.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] { return stop_.load() || !tasks_.empty(); });
                        if (stop_.load() && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() noexcept {
        stop_.store(true);
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

    [[nodiscard]] std::size_t workerCount() const noexcept { return workers_.size(); }
};


// ─────────────────────────────────────────────────────────────────────────────
// Thread-Safe Determinate Progress & CancellationToken (Workflow 2 & Workflow 9)
// Keeps long-running file imports/scans off the UI thread with safe cancellation.
// ─────────────────────────────────────────────────────────────────────────────
class CancellationToken {
    std::atomic<bool> cancelled_{false};
    std::atomic<int> progress_pct_{0};

public:
    CancellationToken() = default;

    void requestCancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }
    void reset() noexcept {
        cancelled_.store(false, std::memory_order_relaxed);
        progress_pct_.store(0, std::memory_order_relaxed);
    }
    void setProgressPercent(int pct) noexcept {
        progress_pct_.store(std::clamp(pct, 0, 100), std::memory_order_relaxed);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] int progressPercent() const noexcept {
        return progress_pct_.load(std::memory_order_relaxed);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Reversible CommandHistory Stack for Undo / Redo (Workflow 7 Step 3)
// ─────────────────────────────────────────────────────────────────────────────
class CommandHistory {
    struct Entry {
        std::string description;
        std::function<void()> apply_fn;
        std::function<void()> revert_fn;
    };
    std::vector<Entry> history_;
    std::size_t cursor_{0};

public:
    void execute(std::string desc, std::function<void()> apply_fn, std::function<void()> revert_fn) {
        if (cursor_ < history_.size()) {
            history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(cursor_), history_.end());
        }
        apply_fn();
        history_.push_back({std::move(desc), std::move(apply_fn), std::move(revert_fn)});
        cursor_ = history_.size();
    }

    bool undo() {
        if (!canUndo()) return false;
        --cursor_;
        history_[cursor_].revert_fn();
        return true;
    }

    bool redo() {
        if (!canRedo()) return false;
        history_[cursor_].apply_fn();
        ++cursor_;
        return true;
    }

    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < history_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return history_.size(); }
    [[nodiscard]] std::size_t undoSize() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t redoSize() const noexcept { return history_.size() - cursor_; }
};

} // namespace velox

