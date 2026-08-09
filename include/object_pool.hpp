#pragma once

#include "fixed_block_pool.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

// Type-safe wrapper over FixedBlockPool.
//
// Example:
//   ObjectPool<Order> orders(1024);
//   Order* order = orders.create(1, 101.25, 50, 'B');
//   orders.destroy(order);
//
// create() constructs T inside a pool block. destroy() calls T's destructor and
// returns that block to the pool.
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t capacity)
        : pool_(sizeof(T), capacity) {}

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template <typename... Args>
    T* create(Args&&... args) {
        void* memory = pool_.allocate();

        try {
            return new (memory) T(std::forward<Args>(args)...);
        } catch (...) {
            pool_.deallocate(memory);
            throw;
        }
    }

    void destroy(T* object) noexcept {
        if (object == nullptr) {
            return;
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            object->~T();
        }

        pool_.deallocate(object);
    }

    std::size_t capacity() const noexcept { return pool_.capacity(); }
    std::size_t used() const noexcept { return pool_.used(); }
    std::size_t available() const noexcept { return pool_.available(); }

private:
    FixedBlockPool pool_;
};
