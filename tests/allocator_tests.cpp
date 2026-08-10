#include "fixed_block_pool.hpp"
#include "object_pool.hpp"
#include "pool_allocator.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <list>
#include <new>

struct alignas(64) Order {
    std::uint64_t id;
    std::uint64_t timestamp_ns;
    std::uint32_t instrument_id;
    std::uint32_t quantity;
    double price;
    char side;
    std::uint8_t padding[23];
};

struct LifetimeCounter {
    static int constructed;
    static int destroyed;

    int value;

    explicit LifetimeCounter(int value) : value(value) {
        ++constructed;
    }

    ~LifetimeCounter() {
        ++destroyed;
    }
};

int LifetimeCounter::constructed = 0;
int LifetimeCounter::destroyed = 0;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test failed: " << message << '\n';
        std::exit(1);
    }
}

static void test_pool_counts() {
    FixedBlockPool pool(sizeof(Order), 3);

    require(pool.capacity() == 3, "capacity should match constructor argument");
    require(pool.used() == 0, "new pool should have no used blocks");
    require(pool.available() == 3, "new pool should have all blocks available");

    void* a = pool.allocate();
    void* b = pool.allocate();

    require(a != nullptr, "first allocation should return memory");
    require(b != nullptr, "second allocation should return memory");
    require(a != b, "two live allocations should not return the same block");
    require(pool.used() == 2, "used count should increase after allocation");
    require(pool.available() == 1, "available count should decrease after allocation");

    pool.deallocate(a);
    require(pool.used() == 1, "used count should decrease after deallocation");
    require(pool.available() == 2, "available count should increase after deallocation");

    pool.deallocate(b);
    require(pool.used() == 0, "all blocks should be returned");
    require(pool.available() == 3, "all blocks should be available again");
}

static void test_pool_reuses_returned_block() {
    FixedBlockPool pool(sizeof(Order), 2);

    void* first = pool.allocate();
    void* second = pool.allocate();

    pool.deallocate(first);
    void* reused = pool.allocate();

    require(reused == first, "pool should reuse the most recently returned block");

    pool.deallocate(second);
    pool.deallocate(reused);
}

static void test_pool_exhaustion() {
    FixedBlockPool pool(sizeof(Order), 1);

    void* block = pool.allocate();

    bool threw = false;
    try {
        pool.allocate();
    } catch (const std::bad_alloc&) {
        threw = true;
    }

    require(threw, "allocating past capacity should throw std::bad_alloc");
    pool.deallocate(block);
}

static void test_object_pool_constructs_and_destroys_objects() {
    LifetimeCounter::constructed = 0;
    LifetimeCounter::destroyed = 0;

    ObjectPool<LifetimeCounter> pool(2);

    LifetimeCounter* a = pool.create(10);
    LifetimeCounter* b = pool.create(20);

    require(a->value == 10, "first object should receive constructor argument");
    require(b->value == 20, "second object should receive constructor argument");
    require(LifetimeCounter::constructed == 2, "constructors should run");
    require(LifetimeCounter::destroyed == 0, "objects should not be destroyed yet");
    require(pool.used() == 2, "object pool should track live objects");

    pool.destroy(a);
    pool.destroy(b);

    require(LifetimeCounter::constructed == 2, "destroy should not construct objects");
    require(LifetimeCounter::destroyed == 2, "destroy should call destructors");
    require(pool.used() == 0, "object pool should have no live objects left");
}

static void test_pool_allocator_with_stl_list() {
    LifetimeCounter::constructed = 0;
    LifetimeCounter::destroyed = 0;

    using CounterList = std::list<LifetimeCounter, PoolAllocator<LifetimeCounter>>;
    CounterList counters{PoolAllocator<LifetimeCounter>(4)};

    counters.emplace_back(10);
    counters.emplace_back(20);
    counters.emplace_back(30);

    require(counters.size() == 3, "list should contain inserted objects");
    require(counters.front().value == 10, "list should preserve first object value");
    require(LifetimeCounter::constructed == 3, "list should construct inserted objects");

    counters.pop_front();
    require(counters.size() == 2, "list pop should remove one object");
    require(LifetimeCounter::destroyed == 1, "list pop should destroy removed object");

    counters.clear();
    require(LifetimeCounter::destroyed == 3, "list clear should destroy remaining objects");
}

int main() {
    test_pool_counts();
    test_pool_reuses_returned_block();
    test_pool_exhaustion();
    test_object_pool_constructs_and_destroys_objects();
    test_pool_allocator_with_stl_list();

    std::cout << "allocator tests passed\n";
    return 0;
}
