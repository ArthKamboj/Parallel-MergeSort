#define _WIN32_WINNT 0x0600
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <future>
#include <thread>
#include <windows.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

const int Threshold = 10000;

int getCoreCount() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}    

struct SortTask {
    vector<int>* arr;
    int left;
    int right;
    int depth;
};

const int MAX_THREADS = getCoreCount();
const int MAX_DEPTH = log2(MAX_THREADS)+2;
atomic<int> activethreads(1);

queue<SortTask> taskQueue;
CRITICAL_SECTION queueLock;
CONDITION_VARIABLE taskReady;
bool stopPool = false;



void parallelMergeSort(vector<int>& arr, int left, int right);


DWORD WINAPI poolWorker(LPVOID lpParam) {

    while (true) {
        SortTask task;
        EnterCriticalSection(&queueLock);

        while(taskQueue.empty() && !stopPool) {
                SleepConditionVariableCS(&taskReady, &queueLock, INFINITE);
        }

        if(stopPool && taskQueue.empty()) {
            LeaveCriticalSection(&queueLock);
            break;
        }

        task = taskQueue.front();
        taskQueue.pop();
        LeaveCriticalSection(&queueLock);

        sequentialMergeSort(*(task.arr), task.left, task.right);
    }

    return 0;
}

void merge(vector<int>& arr, int left, int mid, int right){

    int n1 = mid - left + 1;
    int n2 = right - mid;

    vector<int> L(n1), R(n2);

    for (int i=0; i<n1; i++) L[i] = arr[left+i];
    for (int j=0; j<n2; j++) R[j] = arr[mid+1+j];

    int i=0, j=0, k=left;
    while (i<n1 && j<n2){
        if (L[i] <= R[j]) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }

    while (i<n1) arr[k++] = L[i++];
    while (j<n2) arr[k++] = R[j++];
}


void sequentialMergeSort(vector<int>& arr, int left, int right){
    
    if(left<right){
        int mid = left + ((right-left)>>1);
        sequentialMergeSort(arr, left, mid);
        sequentialMergeSort(arr, mid+1, right);
        merge(arr, left, mid, right);
    }

}

void parallelMergeSort(vector<int>& arr, int left, int right){

    if(right - left < Threshold){
        sequentialMergeSort(arr, left, right);
        return;
    }

    int mid = left + ((right-left)>>1);

    if(activethreads < MAX_THREADS){
        activethreads++;

        ThreadData data = {&arr, left, mid};
        HANDLE hThread = CreateThread(NULL, 0, threadWorker, &data, 0, NULL);

        parallelMergeSort(arr, mid+1, right);
    
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);

        activethreads--;
    }
    else{

        sequentialMergeSort(arr, left, mid);
        parallelMergeSort(arr, mid+1, right);
    }

    merge(arr, left, mid, right);
}

int main() {

    InitializeCriticalSection(&queueLock);
    InitializeConditionVariable(&taskReady);

    const int N = 10000000;
    vector<int> data(N);

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 10000000);
    for(int& x:data) x = dis(gen);

    cout << MAX_THREADS << "\n";

    cout << "Starting parallel sort for N = " << N << "\n";

    auto start = chrono::high_resolution_clock::now();

    parallelMergeSort(data, 0, N-1);

    auto end = chrono::high_resolution_clock::now();
    
    chrono::duration<double> elapsed = end-start;

    if(is_sorted(data.begin(), data.end())){
        cout << "Sort Successful" << "\n";
        cout << "Time Taken: " << elapsed.count() << " seconds"<<"\n"; 
    }
    else{
        cout << "Sort Failed!" << "\n";
    }
    cout << endl;

    return 0;

}
