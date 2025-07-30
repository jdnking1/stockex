# StockeX: An Ultra-Low-Latency HFT Simulator

StockeX is a high-performance, C++23 stock exchange matching engine built from the ground up. It's designed to research and implement ultra-low-latency trading systems, capable of processing millions of events per second with predictable, nanosecond-level performance.

The core of the engine is a custom-built order book that leverages cache-friendly data structures, custom memory management, and modern CPU features to minimize latency for adding, canceling, and matching orders.

## Key Features & Architecture 🚀

* **High-Performance Matching Engine:** A fully functional order book that can handle limit orders for a single instrument.
* **Dual `OrderQueue` Implementations:** The system can be compiled with one of two queue implementations for performance comparison. Both queues use lazy deletion but differ in the way they track deleted orders:
    * **Bitmap Queue (Default):** Uses `std::uint64_t` bitmaps and CPU intrinsics (`_tzcnt_u64`, `_lzcnt_u64`) for `O(1)` order cancellations and extremely fast iteration over active orders, even in highly fragmented queues.
    * **Linear Scan Queue:** This implementation uses a simple boolean flag to check order validity and uses a linear scan to iterate over active orders.
* **Cache-Friendly Design:** Utilizes a direct-mapped `std::array` for `O(1)` price-level lookups and chunked data structures to ensure high memory locality.
* **Custom Memory Pools:** Avoids system call overhead from frequent `new`/`delete` calls by using efficient, pre-allocated memory pools for `PriceLevel` and `OrderQueue` chunks.
* **Detailed Benchmarking Suite:** Includes multiple benchmarks to test performance under various realistic market scenarios.

## Tech Stack 🛠️

* **Core:** C++23
* **Build System:** CMake (v3.20+)
* **CPU Architecture:** x86-64 with **Haswell instruction set** or newer (required for BMI1 intrinsics like `_tzcnt_u64`).
* **Analysis:** Python 3, Pandas, Matplotlib, Seaborn

## Quickstart: Automated Benchmarking & Visualization

This is the recommended workflow for testing the engine.

### Step 1: Install Prerequisites

* **C++:** A C++23 compliant compiler (e.g., GCC 12+, Clang 15+) and CMake (v3.20+).
* **Python:** Python 3 and the required analysis libraries.
    ```bash
    pip install pandas matplotlib seaborn
    ```

### Step 2: Run the Full Suite

A single bash script handles the entire process: building both C++ implementations, running all benchmarks, and generating the final plots.

1.  **Make the script executable:**
    ```bash
    chmod +x scripts/run_benchmarks.sh
    ```
2.  **Run the suite from the project root:**
    ```bash
    ./scripts/run_benchmarks.sh
    ```

Upon completion, all raw latency data will be in the `benchmark_results/` directory, and the comparative plots will be saved in `benchmark_plots/`.

## Advanced Usage

### Ad-Hoc Comparison & Analysis

For more targeted analysis, the `latency_analyser.py` script can plot one or more specific result files against each other. It can either save the plot to a file or display it in an interactive window.

**Example 1: Save a Comparison Plot to a File**
```bash
python scripts/latency_analyser.py \
    benchmark_results/latencies_add_bitmap_add_heavy_5.txt \
    benchmark_results/latencies_add_linear_scan_add_heavy_5.txt \
    -o add_latency_comparison.png \
    -t "Add Latency: Bitmap vs. Linear Scan"
```

**Example 2: Show an Interactive Plot for Exploration**
```bash
python scripts/latency_analyser.py \
    benchmark_results/latencies_fragmentation_bitmap.txt \
    benchmark_results/latencies_fragmentation_linear_scan.txt \
    --show
```

### Manual Build & Run

If you wish to build and run the benchmarks manually, you can do so using CMake.

1.  **Configure:**
    ```bash
    # For the Bitmap queue
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_BITMAP=ON
    
    # For the Linear Scan queue
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_BITMAP=OFF
    ```
2.  **Build:**
    ```bash
    cmake --build build -j
    ```
3.  **Run:** The executables will be located in the `./build/bench/` directory.

## Project Status & Roadmap

The core matching engine and data structures are complete and heavily benchmarked. Future work is focused on building out the surrounding system architecture.

* [ ] **Networking Layer:** Implement a TCP/UDP server for order entry and market data dissemination.
* [ ] **Inter-thread Communication:** Design and build robust, lock-free communication protocols for a multi-threaded architecture.
* [ ] **CI Integration:** Add a GitHub Actions workflow to automatically run benchmarks on every push.

## License

This project is licensed under the **MIT License**.
