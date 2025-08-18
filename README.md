# StockeX: An Low-Latency C++23 Matching Engine

StockeX is a high-performance C++23 stock exchange matching engine simulator engineered from the ground up for **low-latency** performance. It serves as a research platform for implementing and testing advanced trading system architectures capable of processing millions of events per second with predictable, nanosecond-level determinism.

The engine's core is a novel order book implementation that leverages cache-friendly data structures, custom memory management, and modern CPU intrinsics (AVX2, BMI1) to achieve extreme performance in adding, canceling, and matching orders.

## Key Features & Architecture 🚀

The architecture is meticulously designed to minimize latency at every stage of an order's lifecycle.

* **High-Throughput Order Book**: A fully featured order book that supports limit orders for a single instrument, designed for high-frequency trading scenarios.
* **SIMD-Accelerated Order Queue**: The `OrderQueue` is a custom-built, cache-friendly data structure that uses a bitmap-based validity check.
    * It avoids linear scanning by using **AVX2** intrinsics to check 256 bits (4 x 64-bit words) of the validity bitmap at a time.
    * It leverages the `_tzcnt_u64` intrinsic (BMI1) to instantly find the next active order in the bitmap, making iteration O(1) in the best case.
* **Custom Memory Pool**: A highly efficient, pre-allocated `MemoryPool` is used for all `PriceLevel` and `OrderQueue` chunks. This strategy virtually eliminates the overhead of expensive system calls like `new` and `delete` during trading sessions.
* **Detailed Benchmarking Suite**: The project includes a comprehensive suite to measure performance under various realistic market conditions, including different order-to-trade ratios and queue fragmentation scenarios.
* **CI Integration**: A GitHub Actions workflow automatically builds and runs the test suite with multiple modern compilers (GCC 14, Clang 18) to ensure code quality and stability.

## Tech Stack 🛠️

* **Core Language**: **C++23**
* **Build System**: **CMake** (v3.20+)
* **CPU Architecture**: **x86-64** with the **Haswell instruction set** or newer is required.
    * This is necessary for the AVX2 and BMI1 intrinsics (`_mm256_testz_si256`, `_tzcnt_u64`) which are critical for performance.
* **Analysis & Visualization**: **Python 3**, Pandas, Matplotlib, Seaborn.

---

## Quickstart: Automated Benchmarking & Visualization

This is the recommended workflow for a comprehensive performance analysis.

### Step 1: Install Prerequisites

* **C++ Compiler**: A C++23 compliant compiler (e.g., GCC 12+, Clang 15+) and CMake (v3.20+).
* **Python Libraries**: The required libraries for analysis and plotting.
    ```bash
    pip install pandas matplotlib seaborn
    ```
* **libuv**: A multi-platform support library with a focus on asynchronous I/O.
    ```bash
    # Example on Ubuntu
    sudo apt-get install libuv1-dev
    ```

### Step 2: Run the Full Suite

A single bash script automates the entire process: building the C++ application, running all benchmarks, and generating comparative plots.

1.  **Make the script executable:**
    ```bash
    chmod +x scripts/run_benchmarks.sh
    ```
2.  **Run the suite from the project root:**
    ```bash
    ./scripts/run_benchmarks.sh
    ```

When the script finishes, all raw latency data files will be in the `benchmark_results/` directory, and the final plots will be saved in `benchmark_plots/`.

---

## Benchmarking Scenarios

The suite includes several distinct benchmarks to stress-test the engine under different conditions.

| Benchmark Executable      | Description                                                                                                                              |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `marketSimulation`        | Simulates four different market scenarios: **add-heavy**, **cancel-heavy**, **match-heavy**, and **balanced** to test general performance. |
| `fragmentation_benchmark` | Measures performance when the order queue is highly fragmented (i.e., many orders have been added and canceled).                         |
| `sweep_benchmark`         | Tests the engine's ability to match a large quantity against thousands of smaller resting orders in a single, aggressive sweep.          |

## Advanced Usage

### Ad-Hoc Comparison & Analysis

For more targeted analysis, the `latency_analyser.py` script can plot one or more specific result files against each other. It can either save the plot to a file or display it in an interactive window.

**Example 1: Save a Comparison Plot to a File**

```bash
python scripts/latency_analyser.py \
    benchmark_results/latencies_add_bitmap_chunked_queueSIMD_add_heavy_150.txt \
    benchmark_results/latencies_cancel_bitmap_chunked_queueSIMD_cancel_heavy_150.txt \
    -o custom_comparison.png \
    -t "Add vs. Cancel Latency"
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

## License

This project is licensed under the **MIT License**.
