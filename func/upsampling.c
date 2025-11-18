#include "../include/bpsk.h"

int* upsampling(int arr[], int len, int sampling){
    int count = 0;
    int *tx_buff = (int*)malloc(len * sampling * sizeof(int));
    for (int i = 0; i < len; i+=2){
        tx_buff[count]=arr[i];
        count++;
        for (int j = 0; j < sampling-1; j++){
            tx_buff[count] = 0;
            count++;
        }
    }
    return tx_buff;
}