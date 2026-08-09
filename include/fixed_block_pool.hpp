#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <new>

// FixedBlockPool owns one raw memory buffer and splits it into equal-sized blocks.
//
// Example:
//   FixedBlockPool pool(sizeof(Order), 1024);
//   void* p = pool.allocate();
//   pool.deallocate(p);
//
// This class only manages raw memory. It does not construct or destroy C++ objects.
class FixedBlockPool {
private:
    struct FreeBlock {
        FreeBlock* next;
    };

public:
    FixedBlockPool(std::size_t block_size, std::size_t block_count)
        : memory_(nullptr),
          free_head_(nullptr),
          block_size_(block_size),
          block_count_(block_count),
          used_(0),
          alignment_(alignof(std::max_align_t)) {
        assert(block_size_ > 0 && "block size must be greater than zero");
        assert(block_count_ > 0 && "pool must contain at least one block");

        block_size_ = std::max(block_size_, sizeof(FreeBlock));
        block_size_ = (block_size_ + alignment_ - 1) & ~(alignment_ - 1);

        memory_ = ::operator new(
            block_size_ * block_count_,
            static_cast<std::align_val_t>(alignment_)
        );

        link_blocks();
    }

    ~FixedBlockPool() {
        ::operator delete(memory_, static_cast<std::align_val_t>(alignment_));
    }

    FixedBlockPool(const FixedBlockPool&) = delete;
    FixedBlockPool& operator=(const FixedBlockPool&) = delete;

    void* allocate() {
        if (free_head_ == nullptr) {
            throw std::bad_alloc{};
        }

        FreeBlock* block = free_head_;
        free_head_ = free_head_->next;
        ++used_;
        return block;
    }

    void deallocate(void* ptr) noexcept {
        if (ptr == nullptr) {
            return;
        }

        assert(used_ > 0 && "deallocating from an empty pool");

        auto* block = static_cast<FreeBlock*>(ptr);
        block->next = free_head_;
        free_head_ = block;
        --used_;
    }

    std::size_t block_size() const noexcept { return block_size_; }
    std::size_t capacity() const noexcept { return block_count_; }
    std::size_t used() const noexcept { return used_; }
    std::size_t available() const noexcept { return block_count_ - used_; }

private:
    void link_blocks() noexcept {
        auto* start = static_cast<unsigned char*>(memory_);
        free_head_ = reinterpret_cast<FreeBlock*>(start);

        for (std::size_t i = 0; i + 1 < block_count_; ++i) {
            auto* current = reinterpret_cast<FreeBlock*>(start + i * block_size_);
            auto* next = reinterpret_cast<FreeBlock*>(start + (i + 1) * block_size_);
            current->next = next;
        }

        auto* last = reinterpret_cast<FreeBlock*>(start + (block_count_ - 1) * block_size_);
        last->next = nullptr;
    }

    void* memory_;
    FreeBlock* free_head_;
    std::size_t block_size_;
    std::size_t block_count_;
    std::size_t used_;
    std::size_t alignment_;
};
