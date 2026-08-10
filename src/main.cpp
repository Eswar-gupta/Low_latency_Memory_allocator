#include "object_pool.hpp"

#include <cstdint>
#include <iostream>

struct alignas(64) Order {
    std::uint64_t id;
    std::uint64_t timestamp_ns;
    std::uint32_t instrument_id;
    std::uint32_t quantity;
    double price;
    char side;
    std::uint8_t padding[23];

    Order(std::uint64_t id, std::uint64_t timestamp_ns, std::uint32_t instrument_id,
          std::uint32_t quantity, double price, char side)
        : id(id), timestamp_ns(timestamp_ns), instrument_id(instrument_id),
          quantity(quantity), price(price), side(side) {}
};

static void print_state(const ObjectPool<Order>& pool) {
    std::cout << "used=" << pool.used()
              << ", available=" << pool.available() << '\n';
}

int main() {
    ObjectPool<Order> orders(4);

    std::cout << "ObjectPool<Order> demo\n";
    std::cout << "capacity=" << orders.capacity() << '\n';

    print_state(orders);

    Order* a = orders.create(1, 1690000000000ULL, 1, 50, 101.25, 'B');
    Order* b = orders.create(2, 1690000000001ULL, 1, 25, 101.30, 'S');
    print_state(orders);

    std::cout << "first_order=" << a->id
              << ", price=" << a->price
              << ", side=" << a->side << '\n';

    orders.destroy(a);
    print_state(orders);

    Order* c = orders.create(3, 1690000000002ULL, 1, 10, 101.20, 'B');
    std::cout << "reused_first_slot=" << (c == a ? "yes" : "no") << '\n';

    orders.destroy(b);
    orders.destroy(c);
    print_state(orders);

    return 0;
}
