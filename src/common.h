#ifndef COMMON_H
#define COMMON_H

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include <fstream>
#include <mutex>
#include <atomic>
#include <SoapySDR/Device.h> 
#include <SoapySDR/Formats.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h>
#include <complex.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include "imgui.h"
#include "implot.h"

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

// functions bpsk
int *to_bpsk(int *bit_arr, int length);
int *upsampling(int *bpsk_arr, int length);
int *convolution(int *upsampling_arr, int *impulse_arr, int length, int impulse_length);

// SDR
int sdr_run(int argc, char *argv[]);

// GUI
void run_gui();

// struct SDRdata
struct SDRData {
    std::vector<int16_t> iq_samples;      
    std::mutex mutex;                     
    std::atomic<bool> new_data_available; 

    void update_samples(const int16_t* samples, size_t num_iq_pairs) {
        std::lock_guard<std::mutex> lock(mutex);
        iq_samples.resize(num_iq_pairs * 2);
        memcpy(iq_samples.data(), samples, num_iq_pairs * 2 * sizeof(int16_t));
        new_data_available = true;
    }

    std::vector<int16_t> get_samples_copy() {
        std::lock_guard<std::mutex> lock(mutex);
        new_data_available = false;
        return iq_samples; 
    }

    bool has_new_data() const {
        return new_data_available.load();
    }
};

extern SDRData g_sdr_data;

#endif // COMMON_H