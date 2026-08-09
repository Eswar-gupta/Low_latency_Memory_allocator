# Custom Memory Allocator for HFT

This project implements a high-performance **Custom Memory Allocator** in C++, designed specifically for low-latency / high-frequency trading (HFT) systems where the same types of objects (like orders or market data events) are repeatedly created and destroyed.

By pre-allocating a large contiguous block of memory and managing it with a free-list, we avoid the heavy latency spikes associated with frequent `new/delete` heap allocations in the hot path. 

The project includes:
1. **`FixedBlockPool`**: A raw memory manager using $O(1)$ free-list logic.
2. **`ObjectPool<T>`**: A type-safe wrapper using placement new.
3. **`PoolAllocator<T>`**: An STL-compatible allocator adapter to accelerate node-based STL containers.

---

## 📊 Benchmark Results

### 1. Full Object Lifecycle (Create-Edit-Destroy)
This benchmark simulates creating objects, modifying them, and destroying them repeatedly. `ObjectPool` completely bypasses heap allocation in the hot path.

![Full Lifecycle Comparison](/Users/eswargupta/Desktop/1Acads/HFT_Projects/Custom_Memory_allocator/results2/lifecycle_combined.png)

**At 1,000,000 operations:**
- **Standard Heap (`new/delete`)**: `269.551 ms`
- **Standard STL Allocator**: `160.813 ms` *(from previous run)*
- **Custom `ObjectPool`**: `16.053 ms`
- **Result:** `16.79x` faster than standard heap allocation.

### 2. STL Data Structure Lifecycle (std::list)
This benchmark simulates an order book maintaining a `std::list<Order>`. Standard STL lists require a new heap allocation for every node inserted. By injecting our `PoolAllocator`, node allocation drops to $O(1)$.

![STL Order Book Comparison](/Users/eswargupta/Desktop/1Acads/HFT_Projects/Custom_Memory_allocator/results2/stl_order_book_combined.png)

**At 1,000,000 operations:**
- **Default STL Allocator**: `179.446 ms`
- **Custom `PoolAllocator`**: `88.628 ms`
- **Result:** `2.02x` faster than the default STL allocator.

---

## 💼 Resume Bullet Points

If you are using this project for a software engineering or quantitative trading resume, you can use the following bullet points:

* **Designed and implemented a low-latency C++ memory allocator** using fixed-size blocks and an $O(1)$ free-list, eliminating dynamic heap overhead for high-frequency object creation/destruction workflows.
* **Developed an STL-compatible allocator adapter (`PoolAllocator<T>`)** to accelerate node-heavy structures like `std::list` by 2x, and reduced standard object lifecycle latency by over 16x (benchmarked at 1M operations).

---

## 🚀 How to Run the Benchmarks

To compile and run the benchmark suite locally (ensure you have CMake and a C++17 compatible compiler installed):

1. **Build in Release Mode:**
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release -S . -B build_release
   cmake --build build_release
   ```

2. **Run Benchmarks:**
   This will output the timings and speedups, generating raw data files in `benchmark_results/`.
   ```bash
   ./build_release/benchmark_allocators
   ```

3. **Generate Graphs (Optional):**
   To recreate the PNG plots, ensure you have python and `pillow` installed, then run the plotting script:
   ```bash
   .venv/bin/python tools/plot_benchmarks.py
   ```
   The generated charts will be saved to the `results/` and `results2/` directories.
