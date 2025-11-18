#include "../include/bpsk.h"

int* convolve_filter(int signal[], int filter[], int len, int sampling) {

    int sum = 0;
    int *result = (int*)malloc(len * sampling * sizeof(int));
    for (int i = 0; i < len;){
        for(int j = 0; j < sampling; j++){
            result[i] = filter[j] * signal[i] + sum;
            sum|=result[i];
            i++;
        }
        sum = 0;
    }

    return result;
}