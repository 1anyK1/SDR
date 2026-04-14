#include "./common.h"
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <complex>

extern SDRData g_sdr_data;

volatile int running = 1;

void sigint_handler(int) {
    running = 0;
}

void prepare_tx_buffer(int16_t *buffer, int buffer_samples,
                       const int16_t *tx_samples,
                       size_t &sample_offset,
                       size_t total_samples) {

    memset(buffer, 0, buffer_samples * 2 * sizeof(int16_t));

    size_t remaining = total_samples - sample_offset;
    size_t samples_to_copy = std::min((size_t)buffer_samples, remaining);

    for (size_t i = 0; i < samples_to_copy; i++) {
        size_t idx = sample_offset + i;
        if (idx * 2 + 1 < total_samples * 2) {
            buffer[i * 2]     = tx_samples[idx * 2];
            buffer[i * 2 + 1] = tx_samples[idx * 2 + 1];
        }
    }

    sample_offset += samples_to_copy;
    if (sample_offset >= total_samples) sample_offset = 0;
}

int sdr_run(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    if (argc < 3) return 1;

    bool is_tx = strcmp(argv[2], "1") == 0;
    bool is_rx = strcmp(argv[2], "0") == 0;

    char *uri = argv[1];

    int16_t *tx_samples = nullptr;
    size_t total_tx_samples = 0;

    if (is_tx) {
        int bits[] = {
            0,1,1,0,0,0,1,0,0,1,1,0,1,1,1,1,0,1,1,0,0,0,1,1,
            0,1,1,0,1,0,0,0,0,1,1,1,1,0,1,0,0,1,1,0,0,0,0,1,
            0,1,1,0,1,1,1,0,0,0,0,0,1,0,1,0
        };

        int len = sizeof(bits) / sizeof(int);

        int *bpsk = to_bpsk(bits, len);
        if (!bpsk) return -1;

        int *ups = upsampling(bpsk, len);
        if (!ups) {
            free(bpsk);
            return -1;
        }

        int up_len = len * 10 - 9;

        int pulse[8] = {1,1,1,1,1,1,1,1};
        int *conv = convolution(ups, pulse, up_len, 8);
        if (!conv) {
            free(bpsk);
            free(ups);
            return -1;
        }

        size_t base_len = up_len + 8 - 1;
        size_t repeat = 2000;

        total_tx_samples = base_len * repeat;
        if (total_tx_samples == 0) {
            free(bpsk); free(ups); free(conv);
            return -1;
        }

        tx_samples = (int16_t*)malloc(total_tx_samples * 2 * sizeof(int16_t));
        if (!tx_samples) {
            free(bpsk); free(ups); free(conv);
            return -1;
        }

        for (size_t r = 0; r < repeat; r++) {
            for (size_t i = 0; i < base_len; i++) {
                size_t idx = r * base_len + i;
                tx_samples[idx * 2]     = conv[i] * 3000;
                tx_samples[idx * 2 + 1] = 0;
            }
        }

        free(bpsk);
        free(ups);
        free(conv);
    }

    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", uri);
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (!sdr) {
        free(tx_samples);
        return -1;
    }

    double sample_rate = 1e6;
    double frequency   = 800e6;
    double bandwidth   = 1e6;

    if (is_tx) {
        SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
        SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, frequency, NULL);
        SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, -30);
        SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_TX, 0, bandwidth);
    }

    if (is_rx) {
        SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
        SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, frequency, NULL);
        SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 90);
        SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_RX, 0, bandwidth);
    }

    size_t channels[] = {0};
    SoapySDRStream *stream = SoapySDRDevice_setupStream(
        sdr,
        is_tx ? SOAPY_SDR_TX : SOAPY_SDR_RX,
        SOAPY_SDR_CS16,
        channels,
        1,
        NULL
    );

    if (!stream) {
        SoapySDRDevice_unmake(sdr);
        free(tx_samples);
        return -1;
    }

    if (SoapySDRDevice_activateStream(sdr, stream, 0, 0, 0) != 0) {
        SoapySDRDevice_closeStream(sdr, stream);
        SoapySDRDevice_unmake(sdr);
        free(tx_samples);
        return -1;
    }

    size_t mtu = SoapySDRDevice_getStreamMTU(sdr, stream);
    if (mtu == 0) mtu = 1024;

    size_t buffer_size_samples = std::min(mtu, (size_t)2048);

    int16_t *buffer = (int16_t*)malloc(buffer_size_samples * 2 * sizeof(int16_t));
    if (!buffer) {
        SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
        SoapySDRDevice_closeStream(sdr, stream);
        SoapySDRDevice_unmake(sdr);
        free(tx_samples);
        return -1;
    }

    FILE *rx_file = nullptr;
    if (is_rx) rx_file = fopen("./pcm/rxdata.pcm", "wb");

    const long timeoutUs = 400000;
    size_t sample_offset = 0;

    while (running) {
        if (is_tx && tx_samples) {
            prepare_tx_buffer(buffer, buffer_size_samples,
                              tx_samples, sample_offset, total_tx_samples);

            void *buffs[] = {buffer};
            int flags = 0;
            long long tx_time = 0;

            int ret = SoapySDRDevice_writeStream(
                sdr, stream,
                (const void * const*)buffs,
                buffer_size_samples,
                &flags, tx_time, timeoutUs
            );

            if (ret < 0) break;
            usleep(10);
        }

        if (is_rx) {
            void *buffs[] = {buffer};
            int flags;
            long long timeNs;

            int ret = SoapySDRDevice_readStream(
                sdr, stream, buffs,
                buffer_size_samples,
                &flags, &timeNs, timeoutUs
            );

            if (ret > 0) {
                if (rx_file)
                    fwrite(buffer, ret * 2 * sizeof(int16_t), 1, rx_file);

                g_sdr_data.update_samples(buffer, ret);
            } else if (ret < 0 && ret != SOAPY_SDR_TIMEOUT) {
                break;
            }
        }
    }

    if (rx_file) fclose(rx_file);

    free(buffer);
    free(tx_samples);

    if (stream) {
        SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
        SoapySDRDevice_closeStream(sdr, stream);
    }

    if (sdr) SoapySDRDevice_unmake(sdr);

    return 0;
}
