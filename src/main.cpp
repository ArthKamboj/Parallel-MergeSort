#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
using namespace std;

const int Threshold = 10000;

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

void parellelMergeSort(vector<int>& arr, int left, int right){

    if(right - left < Threshold){
        sequentialMergeSort(arr, left, right);
        return;
    }

    int mid = left + ((right-left)>>1);
    parellelMergeSort(arr, left, mid);
    parellelMergeSort(arr, mid+1, right);
    merge(arr, left, mid, right);
}

int main() {

    const int N = 1000000;
    vector<int> data(N);

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 1000000);
    for(int& x:data) x = dis(gen);

    cout << "Starting sequential sort for N = " << N << "\n";

    auto start = chrono::high_resolution_clock::now();

    parellelMergeSort(data, 0, N-1);

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