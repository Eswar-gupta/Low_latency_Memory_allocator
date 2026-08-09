#pragma once

#include "fixed_block_pool.hpp"

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <unordered_map>

class PoolMemoryResource {
public:
    explicit PoolMemoryResource(std::size_t blocks_per_size)
        : blocks_per_size_(blocks_per_size) {}

    PoolMemoryResource(const PoolMemoryResource&) = delete;
    PoolMemoryResource& operator=(const PoolMemoryResource&) = delete;

    std::shared_ptr<FixedBlockPool> pool_for(std::size_t block_size) {
        auto [it, inserted] = pools_.try_emplace(block_size, nullptr);
        if (inserted) {
            it->second = std::make_shared<FixedBlockPool>(block_size, blocks_per_size_);
        }

        return it->second;
    }

    std::size_t blocks_per_size() const noexcept { return blocks_per_size_; }

private:
    std::size_t blocks_per_size_;
    std::unordered_map<std::size_t, std::shared_ptr<FixedBlockPool>> pools_;
};

template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    template <typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    explicit PoolAllocator(std::size_t capacity = 1024)
        : resource_(std::make_shared<PoolMemoryResource>(capacity)),
          pool_(resource_->pool_for(sizeof(T))) {}

    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other)
        : resource_(other.resource_),
          pool_(resource_->pool_for(sizeof(T))) {}

    T* allocate(std::size_t n) {
        if (n == 0) {
            return nullptr;
        }

        if (n == 1) {
            return static_cast<T*>(pool_->allocate());
        }

        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }

        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* ptr, std::size_t n) noexcept {
        if (ptr == nullptr) {
            return;
        }

        if (n == 1) {
            pool_->deallocate(ptr);
            return;
        }

        ::operator delete(ptr);
    }

    std::size_t capacity() const noexcept {
        return resource_->blocks_per_size();
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>& other) const noexcept {
        return resource_ == other.resource_;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    template <typename U>
    friend class PoolAllocator;

    std::shared_ptr<PoolMemoryResource> resource_;
    std::shared_ptr<FixedBlockPool> pool_;
};
