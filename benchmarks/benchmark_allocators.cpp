#include "object_pool.hpp"
#include "pool_allocator.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

struct Order {
    std::uint64_t id;
    std::uint64_t timestamp_ns;
    double price;
    std::uint32_t quantity;
    char side;

    Order(std::uint64_t id, std::uint64_t timestamp_ns, double price,
          std::uint32_t quantity, char side)
        : id(id), timestamp_ns(timestamp_ns), price(price), quantity(quantity), side(side) {}
};

struct Result {
    std::string workload;
    std::string allocator;
    std::size_t operations;
    double time_ms;
    double ops_per_sec;
    double speedup_vs_baseline;
    std::uint64_t checksum;
};

static constexpr int kRepeats = 3;
static constexpr std::size_t kSummarySize = 1'000'000;
static volatile std::uint64_t g_sink = 0;

static Order make_order(std::size_t i) {
    return Order(
        static_cast<std::uint64_t>(i + 1),
        1'700'000'000'000'000'000ULL + static_cast<std::uint64_t>(i),
        100.0 + static_cast<double>(i % 100) * 0.01,
        static_cast<std::uint32_t>((i % 1000) + 1),
        (i % 2 == 0) ? 'B' : 'S'
    );
}

static std::uint64_t consume(const Order* order) {
    return order->id
         ^ order->timestamp_ns
         ^ static_cast<std::uint64_t>(order->price * 100.0)
         ^ order->quantity
         ^ static_cast<std::uint64_t>(order->side);
}

template <typename PrepareFn, typename RunFn>
static Result measure_hot_path(const std::string& workload,
                               const std::string& allocator,
                               std::size_t operations,
                               PrepareFn&& prepare,
                               RunFn&& run) {
    double best_ms = 0.0;
    std::uint64_t best_checksum = 0;

    for (int repeat = 0; repeat < kRepeats; ++repeat) {
        auto state = prepare();

        // For ObjectPool, prepare() creates the pool before timing starts.
        // That matches the intended model: preallocate during startup, then
        // measure only the hot-path create/destroy work.
        const auto begin = std::chrono::steady_clock::now();
        const std::uint64_t checksum = run(state);
        const auto end = std::chrono::steady_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - begin).count();
        if (elapsed_ms < 0.000001) {
            elapsed_ms = 0.000001;
        }

        if (repeat == 0 || elapsed_ms < best_ms) {
            best_ms = elapsed_ms;
            best_checksum = checksum;
        }
    }

    g_sink ^= best_checksum;

    const double seconds = best_ms / 1000.0;
    return Result{
        workload,
        allocator,
        operations,
        best_ms,
        static_cast<double>(operations) / seconds,
        1.0,
        best_checksum
    };
}

static std::uint64_t new_delete_immediate(std::size_t n) {
    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = new Order(sample);
        checksum += consume(order);
        delete order;
    }

    return checksum;
}

static std::uint64_t std_allocator_immediate(std::size_t n) {
    std::allocator<Order> allocator;
    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = allocator.allocate(1);
        std::allocator_traits<std::allocator<Order>>::construct(allocator, order, sample);
        checksum += consume(order);
        std::allocator_traits<std::allocator<Order>>::destroy(allocator, order);
        allocator.deallocate(order, 1);
    }

    return checksum;
}

static std::uint64_t object_pool_immediate(ObjectPool<Order>& pool, std::size_t n) {
    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = pool.create(sample);
        checksum += consume(order);
        pool.destroy(order);
    }

    return checksum;
}

static std::uint64_t new_delete_batch(std::size_t n) {
    std::vector<Order*> orders;
    orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        orders.push_back(new Order(sample));
    }

    for (Order* order : orders) {
        checksum += consume(order);
        delete order;
    }

    return checksum;
}

static std::uint64_t std_allocator_batch(std::size_t n) {
    std::allocator<Order> allocator;
    std::vector<Order*> orders;
    orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = allocator.allocate(1);
        std::allocator_traits<std::allocator<Order>>::construct(allocator, order, sample);
        orders.push_back(order);
    }

    for (Order* order : orders) {
        checksum += consume(order);
        std::allocator_traits<std::allocator<Order>>::destroy(allocator, order);
        allocator.deallocate(order, 1);
    }

    return checksum;
}

static std::uint64_t object_pool_batch(ObjectPool<Order>& pool, std::size_t n) {
    std::vector<Order*> orders;
    orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        orders.push_back(pool.create(sample));
    }

    for (Order* order : orders) {
        checksum += consume(order);
        pool.destroy(order);
    }

    return checksum;
}

static std::uint64_t new_delete_lifecycle(std::size_t n) {
    std::vector<Order*> live_orders;
    live_orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = new Order(sample);
        checksum += consume(order);
        live_orders.push_back(order);

        if (i % 3 == 2) {
            delete live_orders.back();
            live_orders.pop_back();
        }
    }

    for (Order* order : live_orders) {
        checksum += consume(order);
        delete order;
    }

    return checksum;
}

static std::uint64_t std_allocator_lifecycle(std::size_t n) {
    std::allocator<Order> allocator;
    std::vector<Order*> live_orders;
    live_orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = allocator.allocate(1);
        std::allocator_traits<std::allocator<Order>>::construct(allocator, order, sample);
        checksum += consume(order);
        live_orders.push_back(order);

        if (i % 3 == 2) {
            Order* cancelled = live_orders.back();
            live_orders.pop_back();
            std::allocator_traits<std::allocator<Order>>::destroy(allocator, cancelled);
            allocator.deallocate(cancelled, 1);
        }
    }

    for (Order* order : live_orders) {
        checksum += consume(order);
        std::allocator_traits<std::allocator<Order>>::destroy(allocator, order);
        allocator.deallocate(order, 1);
    }

    return checksum;
}

static std::uint64_t object_pool_lifecycle(ObjectPool<Order>& pool, std::size_t n) {
    std::vector<Order*> live_orders;
    live_orders.reserve(n);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        Order sample = make_order(i);
        Order* order = pool.create(sample);
        checksum += consume(order);
        live_orders.push_back(order);

        if (i % 3 == 2) {
            pool.destroy(live_orders.back());
            live_orders.pop_back();
        }
    }

    for (Order* order : live_orders) {
        checksum += consume(order);
        pool.destroy(order);
    }

    return checksum;
}

static std::uint64_t stl_order_book_default(std::size_t n) {
    std::list<Order> orders;
    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        orders.push_back(make_order(i));

        Order& newest = orders.back();
        newest.quantity += static_cast<std::uint32_t>(i % 7);
        checksum += consume(&newest);

        if (i % 3 == 2) {
            Order& oldest = orders.front();
            oldest.price += 0.01;
            checksum += consume(&oldest);
            orders.pop_front();
        }
    }

    for (const Order& order : orders) {
        checksum += consume(&order);
    }

    return checksum;
}

static std::uint64_t stl_order_book_pool_allocator(PoolAllocator<Order>& allocator, std::size_t n) {
    using OrderList = std::list<Order, PoolAllocator<Order>>;

    OrderList orders{allocator};
    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < n; ++i) {
        orders.push_back(make_order(i));

        Order& newest = orders.back();
        newest.quantity += static_cast<std::uint32_t>(i % 7);
        checksum += consume(&newest);

        if (i % 3 == 2) {
            Order& oldest = orders.front();
            oldest.price += 0.01;
            checksum += consume(&oldest);
            orders.pop_front();
        }
    }

    for (const Order& order : orders) {
        checksum += consume(&order);
    }

    return checksum;
}

static double baseline_time(const std::vector<Result>& results,
                            const std::string& workload,
                            std::size_t operations) {
    const char* baseline_allocator = workload == "stl_order_book" ? "default_stl" : "new_delete";

    for (const Result& result : results) {
        if (result.workload == workload && result.allocator == baseline_allocator &&
            result.operations == operations) {
            return result.time_ms;
        }
    }
    return 0.0;
}

static void fill_speedups(std::vector<Result>& results) {
    for (Result& result : results) {
        const double baseline = baseline_time(results, result.workload, result.operations);
        result.speedup_vs_baseline = result.time_ms > 0.0 ? baseline / result.time_ms : 0.0;
    }
}

static void write_csv(const std::vector<Result>& results, const std::string& path) {
    std::ofstream out(path);
    out << "workload,allocator,operations,time_ms,ops_per_sec,speedup_vs_baseline,checksum\n";

    for (const Result& r : results) {
        out << r.workload << ',' << r.allocator << ',' << r.operations << ','
            << std::fixed << std::setprecision(6) << r.time_ms << ','
            << std::fixed << std::setprecision(2) << r.ops_per_sec << ','
            << std::fixed << std::setprecision(4) << r.speedup_vs_baseline << ','
            << r.checksum << '\n';
    }
}

static void write_text_report(const std::vector<Result>& results, const std::string& path) {
    std::ofstream out(path);
    out << "Custom allocator benchmark report\n";
    out << "=================================\n\n";
    out << "ObjectPool and PoolAllocator timings exclude pool construction/preallocation.\n";
    out << "This models low-latency systems where pools are warmed up before the hot path.\n";
    out << "For immediate/batch/lifecycle, speedup baseline is new/delete.\n";
    out << "For stl_order_book, speedup baseline is default STL allocation.\n\n";

    out << std::left << std::setw(20) << "workload" << std::setw(16) << "allocator"
        << std::right << std::setw(12) << "ops" << std::setw(14) << "time_ms"
        << std::setw(16) << "ops/sec" << std::setw(12) << "speedup" << '\n';
    out << std::string(90, '-') << '\n';

    for (const Result& r : results) {
        out << std::left << std::setw(20) << r.workload << std::setw(16) << r.allocator
            << std::right << std::setw(12) << r.operations
            << std::setw(14) << std::fixed << std::setprecision(3) << r.time_ms
            << std::setw(16) << std::fixed << std::setprecision(0) << r.ops_per_sec
            << std::setw(12) << std::fixed << std::setprecision(2) << r.speedup_vs_baseline
            << '\n';
    }
}

static void print_simple_summary(const std::vector<Result>& results) {
    std::cout << "Simple benchmark summary at " << kSummarySize << " operations\n";
    std::cout << "ObjectPool/PoolAllocator excludes startup preallocation from timed hot path.\n";
    std::cout << "speedup > 1.0 means faster than that workload's baseline\n\n";

    for (const std::string workload : {"immediate", "batch", "lifecycle"}) {
        std::cout << workload << '\n';
        for (const Result& r : results) {
            if (r.workload == workload && r.operations == kSummarySize &&
                (r.allocator == "new_delete" || r.allocator == "object_pool")) {
                std::cout << "  " << std::left << std::setw(12) << r.allocator
                          << std::right << std::setw(10) << std::fixed << std::setprecision(3)
                          << r.time_ms << " ms"
                          << "  speedup=" << std::fixed << std::setprecision(2)
                          << r.speedup_vs_baseline << "x\n";
            }
        }
    }

    std::cout << "stl_order_book\n";
    for (const Result& r : results) {
        if (r.workload == "stl_order_book" && r.operations == kSummarySize &&
            (r.allocator == "default_stl" || r.allocator == "pool_allocator")) {
            std::cout << "  " << std::left << std::setw(14) << r.allocator
                      << std::right << std::setw(10) << std::fixed << std::setprecision(3)
                      << r.time_ms << " ms"
                      << "  speedup=" << std::fixed << std::setprecision(2)
                      << r.speedup_vs_baseline << "x\n";
        }
    }

    std::cout << "\nResults written to:";
    std::cout << "  benchmark_results/results.csv\n";
    std::cout << "  benchmark_results/results.txt\n";
}

static void print_detailed(const std::vector<Result>& results) {
    std::cout << std::left << std::setw(20) << "workload" << std::setw(16) << "allocator"
              << std::right << std::setw(12) << "ops" << std::setw(14) << "time_ms"
              << std::setw(12) << "speedup" << '\n';
    std::cout << std::string(74, '-') << '\n';

    for (const Result& r : results) {
        std::cout << std::left << std::setw(20) << r.workload << std::setw(16) << r.allocator
                  << std::right << std::setw(12) << r.operations
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.time_ms
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.speedup_vs_baseline
                  << '\n';
    }
}

int main(int argc, char** argv) {
    const bool detailed = argc > 1 && std::string(argv[1]) == "--detailed";
    const std::vector<std::size_t> sizes = {1, 10, 100, 1'000, 10'000, 100'000, kSummarySize};

    std::vector<Result> results;

    for (std::size_t n : sizes) {
        results.push_back(measure_hot_path("immediate", "new_delete", n, [] { return 0; },
                                           [n](int&) { return new_delete_immediate(n); }));
        results.push_back(measure_hot_path("immediate", "std_allocator", n, [] { return 0; },
                                           [n](int&) { return std_allocator_immediate(n); }));
        results.push_back(measure_hot_path("immediate", "object_pool", n, [] { return ObjectPool<Order>(1); },
                                           [n](ObjectPool<Order>& pool) { return object_pool_immediate(pool, n); }));

        results.push_back(measure_hot_path("batch", "new_delete", n, [] { return 0; },
                                           [n](int&) { return new_delete_batch(n); }));
        results.push_back(measure_hot_path("batch", "std_allocator", n, [] { return 0; },
                                           [n](int&) { return std_allocator_batch(n); }));
        results.push_back(measure_hot_path("batch", "object_pool", n, [n] { return ObjectPool<Order>(n); },
                                           [n](ObjectPool<Order>& pool) { return object_pool_batch(pool, n); }));

        results.push_back(measure_hot_path("lifecycle", "new_delete", n, [] { return 0; },
                                           [n](int&) { return new_delete_lifecycle(n); }));
        results.push_back(measure_hot_path("lifecycle", "std_allocator", n, [] { return 0; },
                                           [n](int&) { return std_allocator_lifecycle(n); }));
        results.push_back(measure_hot_path("lifecycle", "object_pool", n, [n] { return ObjectPool<Order>(n); },
                                           [n](ObjectPool<Order>& pool) { return object_pool_lifecycle(pool, n); }));

        results.push_back(measure_hot_path("stl_order_book", "default_stl", n, [] { return 0; },
                                           [n](int&) { return stl_order_book_default(n); }));
        results.push_back(measure_hot_path("stl_order_book", "pool_allocator", n, [n] { return PoolAllocator<Order>(n); },
                                           [n](PoolAllocator<Order>& alloc) { return stl_order_book_pool_allocator(alloc, n); }));
    }

    fill_speedups(results);

    std::filesystem::create_directories("benchmark_results");
    write_csv(results, "benchmark_results/results.csv");
    write_text_report(results, "benchmark_results/results.txt");

    if (detailed) {
        print_detailed(results);
    } else {
        print_simple_summary(results);
    }

    return static_cast<int>(g_sink == 0xFFFF'FFFF'FFFF'FFFFULL);
}
