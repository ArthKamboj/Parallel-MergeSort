#define _WIN32_WINNT 0x0600
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <random>
#include <future>
#include <thread>
#include <windows.h>
#include <atomic>
#include <queue>
#include <deque>
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
    vector<int>* aux;
    int left;
    int right;
    int depth;
    HANDLE doneEvent;
};

const int MAX_THREADS = getCoreCount();
const int MAX_DEPTH = log2(MAX_THREADS)+2;
atomic<int> activethreads(1);

vector<deque<SortTask>> localQueues(MAX_THREADS);    
vector<CRITICAL_SECTION> localLocks(MAX_THREADS);
CONDITION_VARIABLE taskReady;
bool stopPool = false;


void sequentialMergeSort(vector<int>& arr, vector<int>& aux, int left, int right);
void parallelMergeSort(vector<int>& arr, vector<int>& aux, int left, int right, int depth, int threadId);


DWORD WINAPI poolWorker(LPVOID lpParam) {

    int myId = *(int*)lpParam;
    delete (int*)lpParam;

    while (true){
        SortTask task;
        bool foundTask = false;

        EnterCriticalSection(&localLocks[myId]);
        if (!localQueues[myId].empty()){
            task = localQueues[myId].front();
            localQueues[myId].pop_front();
            foundTask = true;
        }
        LeaveCriticalSection(&localLocks[myId]);

        if(!foundTask){
            for(int i=0; i<MAX_THREADS; i++){
                if(i == myId) continue;

                if(TryEnterCriticalSection(&localLocks[i])){
                    if(!localQueues[i].empty()){
                        task = localQueues[i].back();
                        localQueues[i].pop_back();
                        foundTask = true;
                    }
                    LeaveCriticalSection(&localLocks[i]);
                }
                if(foundTask) break;
            }
        }

        if(!foundTask){
            if(stopPool) break;
            Sleep(1);
            continue;
        }

        sequentialMergeSort(*(task.arr), *(task.aux), task.left, task.right);
        SetEvent(task.doneEvent);

    }
}

void merge_standard(vector<int>& arr, int left, int mid, int right){

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

void merge_fast(vector<int>& arr, vector<int>& aux, int left, int mid, int right) {

    for(int i=left; i<=right; i++) {
        aux[i]= arr[i];
    }

    int i = left;
    int j = mid+1;
    int k = left;

    while(i<=mid && j<=right) {
        if(aux[i] <= aux[j]) {
            arr[k++] = aux[i++];
        }
        else {
            arr[k++] = aux[j++];
        }   
    }

    while(i<=mid) {
        arr[k++] = aux[i++];
    }
}

void sequentialMergeSort(vector<int>& arr, vector<int>& aux, int left, int right){
    
    if(left<right){
        int mid = left + ((right-left)>>1);
        sequentialMergeSort(arr, aux, left, mid);
        sequentialMergeSort(arr, aux, mid+1, right);
        merge_fast(arr, aux, left, mid, right);
    }

}


void parallelMergeSort(vector<int>& arr, vector<int>& aux, int left, int right, int depth, int threadId){

    if(right - left < Threshold){
        sequentialMergeSort(arr, aux, left, right);
        return;
    }

    int mid = left + ((right-left)>>1);

    if(depth < MAX_DEPTH){

        HANDLE doneSignal = CreateEvent(NULL, TRUE, FALSE, NULL);

        EnterCriticalSection(&localLocks[threadId]);
        localQueues[threadId].push_front({&arr, &aux, left, mid, depth+1, doneSignal});
        LeaveCriticalSection(&localLocks[threadId]);

        parallelMergeSort(arr, aux, mid+1, right, depth+1,threadId);

        WaitForSingleObject(doneSignal, INFINITE);
        CloseHandle(doneSignal);
        
    }
    else{

        sequentialMergeSort(arr, aux, left, mid);
        sequentialMergeSort(arr, aux, mid + 1, right);

    }

    merge_fast(arr, aux, left, mid, right);
}

int main() {

    localQueues.resize(MAX_THREADS);
    localLocks.resize(MAX_THREADS);

    for(int i=0; i<MAX_THREADS; i++){
        InitializeCriticalSection(&localLocks[i]);
    }

    InitializeConditionVariable(&taskReady);

    HANDLE* threads = new HANDLE[MAX_THREADS];

    for(int i=0; i<MAX_THREADS; i++){
        int* id = new int(i);
        threads[i] = CreateThread(NULL, 0, poolWorker, id, 0, NULL);
    }

    cout << "Load Balancer initialized with " << MAX_THREADS << "worker threads." << "\n";

    const int N = 10000000;
    vector<int> data1(N);
    vector<int> data2(N);
    vector<int> aux(N);

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 10000000);
    for(int& x:data1) x = dis(gen);
    for(int& x:data2) x = dis(gen);

    cout << MAX_THREADS << "\n";

    cout << "Starting parallel sort for N = " << N << "\n";

    auto start1 = chrono::high_resolution_clock::now();

    parallelMergeSort(data1, aux, 0, N-1, 0, 0);

    auto end1 = chrono::high_resolution_clock::now();
    
    chrono::duration<double> elapsed1 = end1-start1;

    if(is_sorted(data1.begin(), data1.end())){
        cout << "Sort Successful" << "\n";
        cout << "Time Taken: " << elapsed1.count() << " seconds"<<"\n"; 
    }
    else{
        cout << "Sort Failed!" << "\n";
    }

    cout << "Starting sequential sort for N = " << N << "\n";

    auto start2 = chrono::high_resolution_clock::now();

    sequentialMergeSort(data2, aux, 0, N-1);

    auto end2 = chrono::high_resolution_clock::now();
    
    chrono::duration<double> elapsed2 = end2-start2;

    if(is_sorted(data2.begin(), data2.end())){
        cout << "Sort Successful" << "\n";
        cout << "Time Taken: " << elapsed2.count() << " seconds"<<"\n";
        cout << "Proportional Difference: " << elapsed2.count()/elapsed1.count() <<"\n";
    }
    else{
        cout << "Sort Failed!" << "\n";
    }

    EnterCriticalSection(&localLocks[0]);
    stopPool = true;
    LeaveCriticalSection(&localLocks[0]);

    WakeAllConditionVariable(&taskReady);

    for(int i=0; i<MAX_THREADS; i++){
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
    }

    for(int i=0; i<MAX_THREADS; i++){
        DeleteCriticalSection(&localLocks[i]);
    }
    delete[] threads;

    return 0;

}
