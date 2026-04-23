# High-Performance Parallel Merge Sort With Adaptive Load Balancing

A professional-grade, header-only C++ template library for multi-threaded sorting. This engine leverages the Windows API to implement a decentralized **Work-Stealing Scheduler** and **AVX2 SIMD acceleration**, specifically engineered to saturate hardware memory bandwidth and minimize overhead.

---

## Core Engineering Features

### 1. Decentralized Work-Stealing Architecture
Unlike standard parallel implementations that use a single global task queue (creating a synchronization bottleneck), this engine utilizes a decentralized model to eliminate lock contention.
* **Implementation**: Each worker thread maintains a private **Deque** (Double-ended queue) and a dedicated `CRITICAL_SECTION`. 
* **Worker Logic**: Threads primarily push and pop tasks from their own "local" queue. If a thread's queue becomes empty, it becomes a "thief," attempting to steal tasks from the back of other threads' queues. This ensures all CPU cores remain productive with minimal interference.

### 2. SIMD Vectorization (AVX2)
The computational bottleneck of the merge process is often the data-copying phase. This engine utilizes 256-bit wide SIMD (Single Instruction, Multiple Data) registers to accelerate data movement.
* **Implementation**: Utilizing `__m256i` intrinsics, the engine moves 8 integers per clock cycle during the array-copy phase.
* **Compile-Time Dispatch**: Through template specialization, the engine automatically selects the SIMD "VIP Lane" for integer types while falling back to standard generic paths for complex types (like `std::string` or `double`).

### 3. Adaptive Load Balancing & Gating
To ensure management overhead never exceeds the benefit of parallelism, the engine employs two specific gating mechanisms:
* **Threshold Gating**: Sub-arrays smaller than 10,000 elements are handled by a highly optimized sequential sort to avoid task-creation latency.
* **Recursion Depth Gating**: The engine limits parallel task generation to a depth of `log2(cores) + 2`, preventing the system from over-saturating the pool once all physical cores are active.

### 4. Zero-Allocation Memory Strategy
Merge sort is traditionally memory-bound. This implementation eliminates runtime heap pressure.
* **Implementation**: A single auxiliary "scratchpad" buffer is allocated once at the start of the sort and passed via reference through the recursion tree. This prevents threads from fighting for the OS memory lock, which is a common performance killer in multi-threaded C++.

### 5. Low-Latency Spin-Yield Strategy
Worker threads utilize a hybrid waiting strategy to remain highly responsive without wasting power.
* **Implementation**: Idle threads enter a "Spin-Wait" loop using the `_mm_pause()` hint. If work still does not appear, the thread executes a `SwitchToThread()` yield. This ensures threads pick up stolen tasks in microseconds rather than the milliseconds associated with traditional `Sleep()` calls.

---

## Performance Benchmarks

Results obtained using 10,000,000 integer elements ($N = 10^7$):

| Implementation | Execution Time | Speedup Factor |
| :--- | :--- | :--- |
| Sequential Sort (Standard) | ~6.84 seconds | 1.0x (Baseline) |
| Sequential Sort (SIMD-Optimized) | ~3.73 seconds | 1.83x |
| **Parallel Sort (Work-Stealing + SIMD)** | **~2.02 seconds** | **3.38x** |

*Note: Performance is heavily dependent on hardware memory bandwidth limits and core count.*

---

## Technical Requirements

* **Platform**: Windows (Vista or higher)
* **Compiler**: MinGW-w64 (GCC 6.3+) or MSVC
* **Hardware**: AVX2 support (Intel Haswell / AMD Excavator or newer)
* **Language Standard**: C++11 or higher (C++17 recommended)

---

## Usage Example

As a header-only library, implementation is straightforward. Simply include the header and instantiate the template for your data type.

```cpp
#include "fast_sort.h"
#include <vector>

int main() {
    // Initialize the sorter for your data type
    ParallelSorter<int> sorter;

    // Prepare your data
    std::vector<int> data = { ... };

    // Sort (Thread pool and memory are managed automatically)
    sorter.sort(data);

    return 0;
}

```
---

## Build Instructions

To achieve the benchmarked performance, the compiler must be instructed to unlock AVX2 hardware instructions and high-level optimizations.

### Using MinGW/GCC:
```bash
g++ -O3 -mavx2 -std=c++11 main.cpp -o fast_sort.exe
```

### Using MSVC:
```bash
cl /O2 /arch:AVX2 main.cpp /Fe:fast_sort.exe
