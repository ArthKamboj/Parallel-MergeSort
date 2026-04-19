# High-Performance Parallel Merge Sort with Adaptive Load Balancing

This project implements a multi-threaded Merge Sort algorithm in C++ leveraging the Windows API. It transitions from a standard recursive approach to a high-performance systems-level implementation utilizing a thread pool, task queuing, and hardware-aware load balancing.

## Core Implementation Features

### 1. Thread Pool and Worker Lifecycle
Instead of the overhead associated with creating and destroying threads during recursion, this project implements a persistent Thread Pool.
* **Implementation**: A fixed number of worker threads are spawned at initialization based on `GetSystemInfo`. These threads execute a `poolWorker` loop that persists for the duration of the program, utilizing `SleepConditionVariableCS` to remain dormant without consuming CPU cycles when no tasks are available.

### 2. Producer-Consumer Task Queue
Work distribution is managed through a centralized queue, allowing any available core to pick up the next available sub-sort task.
* **Implementation**: The system uses a `std::queue<SortTask>` protected by a Windows `CRITICAL_SECTION`. Thread signaling is handled via `CONDITION_VARIABLE` to ensure that workers are woken immediately upon the arrival of new data segments.

### 3. Adaptive Load Balancing
To prevent the "Thread Explosion" problem—where management overhead exceeds execution time—the algorithm uses two gating mechanisms:
* **Threshold Gating**: Any sub-array smaller than 10,000 elements is automatically routed to a sequential sort.
* **Depth Gating**: Parallelism is restricted to a maximum recursion depth (calculated as log2 of core count + 2). Once this depth is reached, the system stops creating new tasks for the pool and completes the branch on the current thread, maximizing cache locality.



### 4. Zero-Allocation Memory Management
Standard Merge Sort implementations suffer from "Memory Allocator Contention" due to frequent heap allocations.
* **Implementation**: This project utilizes a "Zero-Allocation" merge strategy. A single auxiliary buffer (`aux`) is allocated once in the main thread and passed via reference/pointer through the task structure. The `merge_fast` function performs all operations within this pre-allocated space, eliminating the need for temporary vectors and reducing pressure on the OS memory manager.



### 5. Event-Driven Synchronization
Since threads in a pool do not exit upon task completion, the parent thread requires a mechanism to know when a sub-task is finished.
* **Implementation**: Each `SortTask` contains a Windows `HANDLE` to an Event object. When a worker finishes a task, it triggers `SetEvent`. The parent thread uses `WaitForSingleObject` to block until the specific sub-task is complete before proceeding to the final merge step.

## Performance Benchmarks

Results based on 10,000,000 integer elements:

* **Sequential Sort (Unptimized)**: ~6.84 seconds
* **Sequential Sort (Optimized)**: ~3.97 seconds
* **Parallel Sort (Thread Pool)**: ~2.13 seconds
* **Performance Gain**: Approximately 1.6x - 2.0x speedup (dependent on hardware memory bandwidth limits).



## Technical Requirements
* **Platform**: Windows (Vista or higher)
* **Headers**: `windows.h`, `process.h`
* **Compiler**: MinGW-w64 or MSVC with C++11 support

## Build Instructions
To compile using MinGW:
```bash
g++ main.cpp -o main.exe
