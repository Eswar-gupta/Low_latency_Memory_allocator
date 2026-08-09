#include "object_pool.hpp"

#include <cstdint>
#include <iostream>

struct Order {
    std::uint64_t id;
    double price;
    std::uint32_t quantity;
    char side;

    Order(std::uint64_t id, double price, std::uint32_t quantity, char side)
        : id(id), price(price), quantity(quantity), side(side) {}
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

    Order* a = orders.create(1, 101.25, 50, 'B');
    Order* b = orders.create(2, 101.30, 25, 'S');
    print_state(orders);

    std::cout << "first_order=" << a->id
              << ", price=" << a->price
              << ", side=" << a->side << '\n';

    orders.destroy(a);
    print_state(orders);

    Order* c = orders.create(3, 101.20, 10, 'B');
    std::cout << "reused_first_slot=" << (c == a ? "yes" : "no") << '\n';

    orders.destroy(b);
    orders.destroy(c);
    print_state(orders);

    return 0;
}
