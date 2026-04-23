#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <deque>
#include <type_traits>
#include <immintrin.h>

template <typename T>
struct MergeHelper {
    static void copy_to_aux(std::vector<T>& arr, std::vector<T>& aux, int left, int right) {
        for (int i = left; i <= right; i++) {
            aux[i] = arr[i];
        }
    }
};

template <>
struct MergeHelper<int> {
    static void copy_to_aux(std::vector<int>& arr, std::vector<int>& aux, int left, int right) {
        int i = left;
        for (; i <= right - 7; i += 8) {
            __m256i data = _mm256_loadu_si256((__m256i*)&arr[i]);
            _mm256_storeu_si256((__m256i*)&aux[i], data);
        }
        for (; i <= right; i++) {
            aux[i] = arr[i];
        }
    }
};

template <typename T>
struct SortTask {
    std::vector<T>* arr;
    std::vector<T>* aux;
    int left;
    int right;
    int depth;
    HANDLE doneEvent;
};


template <typename T>
class ParallelSorter {
private:
    int MAX_THREADS;
    int MAX_DEPTH;
    const int THRESHOLD = 10000;
    std::vector<std::deque<SortTask<T>>> localQueues;
    std::vector<CRITICAL_SECTION> localLocks;
    HANDLE* threads;
    bool stopPool;

    struct WorkerArgs {
        ParallelSorter* sorter;
        int id;
    };

    static DWORD WINAPI poolWorker(LPVOID lpParam) {
        WorkerArgs* args = static_cast<WorkerArgs*>(lpParam);
        args->sorter->workerLoop(args->id);
        delete args;
        return 0;
    }

    void workerLoop(int myId) {
        while (true) {
            SortTask<T> task;
            bool foundTask = false;

            EnterCriticalSection(&localLocks[myId]);
            if (!localQueues[myId].empty()) {
                task = localQueues[myId].front();
                localQueues[myId].pop_front();
                foundTask = true;
            }
            LeaveCriticalSection(&localLocks[myId]);

            if (!foundTask) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    if (i == myId) continue;
                    if (TryEnterCriticalSection(&localLocks[i])) {
                        if (!localQueues[i].empty()) {
                            task = localQueues[i].back();
                            localQueues[i].pop_back();
                            foundTask = true;
                        }
                        LeaveCriticalSection(&localLocks[i]);
                    }
                    if (foundTask) break;
                }
            }

            if (!foundTask) {
                if (stopPool) break;
                _mm_pause();
                SwitchToThread();
                continue;
            }

            sequentialMergeSort(*(task.arr), *(task.aux), task.left, task.right);
            SetEvent(task.doneEvent);
        }
    }

    void merge_fast(std::vector<T>& arr, std::vector<T>& aux, int left, int mid, int right) {
        
        MergeHelper<T>::copy_to_aux(arr, aux, left, right);

        int p1 = left, p2 = mid + 1, k = left;
        while (p1 <= mid && p2 <= right) {
            if (aux[p1] <= aux[p2]) arr[k++] = aux[p1++];
            else arr[k++] = aux[p2++];
        }
        while (p1 <= mid) arr[k++] = aux[p1++];
    }

    void sequentialMergeSort(std::vector<T>& arr, std::vector<T>& aux, int left, int right) {
        if (left < right) {
            int mid = left + ((right - left) >> 1);
            sequentialMergeSort(arr, aux, left, mid);
            sequentialMergeSort(arr, aux, mid + 1, right);
            merge_fast(arr, aux, left, mid, right);
        }
    }

    void parallelRecursive(std::vector<T>& arr, std::vector<T>& aux, int left, int right, int depth, int threadId) {
        if (right - left < THRESHOLD) {
            sequentialMergeSort(arr, aux, left, right);
            return;
        }

        int mid = left + ((right - left) >> 1);
        if (depth < MAX_DEPTH) {
            HANDLE doneSignal = CreateEvent(NULL, TRUE, FALSE, NULL);
            EnterCriticalSection(&localLocks[threadId]);
            localQueues[threadId].push_front({&arr, &aux, left, mid, depth + 1, doneSignal});
            LeaveCriticalSection(&localLocks[threadId]);

            parallelRecursive(arr, aux, mid + 1, right, depth + 1, threadId);
            WaitForSingleObject(doneSignal, INFINITE);
            CloseHandle(doneSignal);
        } else {
            sequentialMergeSort(arr, aux, left, mid);
            sequentialMergeSort(arr, aux, mid + 1, right);
        }
        merge_fast(arr, aux, left, mid, right);
    }

public:
    ParallelSorter() : stopPool(false) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        MAX_THREADS = sysinfo.dwNumberOfProcessors;
        MAX_DEPTH = static_cast<int>(std::log2(MAX_THREADS)) + 2;

        localQueues.resize(MAX_THREADS);
        localLocks.resize(MAX_THREADS);
        threads = new HANDLE[MAX_THREADS];

        for (int i = 0; i < MAX_THREADS; i++) {
            InitializeCriticalSection(&localLocks[i]);
            WorkerArgs* args = new WorkerArgs{this, i};
            threads[i] = CreateThread(NULL, 0, poolWorker, args, 0, NULL);
        }
    }

    ~ParallelSorter() {
        stopPool = true;
        for (int i = 0; i < MAX_THREADS; i++) {
            WaitForSingleObject(threads[i], INFINITE);
            CloseHandle(threads[i]);
            DeleteCriticalSection(&localLocks[i]);
        }
        delete[] threads;
    }

    void sort(std::vector<T>& data) {
        if (data.size() <= 1) return;
        std::vector<T> aux(data.size());
        parallelRecursive(data, aux, 0, (int)data.size() - 1, 0, 0);
    }
};