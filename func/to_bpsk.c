#include "../include/bpsk.h"

int* to_bpsk(int arr[], int len){
    int *IQ = (int*)malloc(len * 2 * sizeof(int));
    for(int i = 0; i < len; i++){
        if(arr[i] == 0){
            IQ[2*i] = 1;
        } else{
            IQ[2*i] = -1;
        }
        IQ[2*i+1] = 0;
    }

    return IQ;
}