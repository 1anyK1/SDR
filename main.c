#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "./include/bpsk.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <tx_uri> <rx_uri>\n", argv[0]);
        return 1;
    }

    const char *tx_uri = argv[1];
    const char *rx_uri = argv[2];

    // === BPSK и подготовка сигнала ===
    int str[] = {0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0,
                 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0};

    int len = sizeof(str) / sizeof(str[0]);
    int *IQ = to_bpsk(str, len);
    int lenIQ = len * 2;

    int sampling = 10;
    int *tx = upsampling(IQ, lenIQ, sampling);
    free(IQ);

    int lenTX = lenIQ * sampling;
    int filter[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int *result = convolve_filter(tx, filter, lenTX, sampling);
    free(tx);

    // === Инициализация TX-устройства ===
    SoapySDRKwargs tx_args = {};
    SoapySDRKwargs_set(&tx_args, "driver", "plutosdr");
    SoapySDRKwargs_set(&tx_args, "uri", tx_uri);
    SoapySDRKwargs_set(&tx_args, "direct", "1");
    SoapySDRKwargs_set(&tx_args, "loopback", "0");

    SoapySDRDevice *tx_sdr = SoapySDRDevice_make(&tx_args);
    SoapySDRKwargs_clear(&tx_args);

    if (tx_sdr == NULL) {
        fprintf(stderr, "Failed to open TX device: %s\n", tx_uri);
        free(result);
        return 1;
    }

    // === Инициализация RX-устройства ===
    SoapySDRKwargs rx_args = {};
    SoapySDRKwargs_set(&rx_args, "driver", "plutosdr");
    SoapySDRKwargs_set(&rx_args, "uri", rx_uri);
    SoapySDRKwargs_set(&rx_args, "direct", "1");
    SoapySDRKwargs_set(&rx_args, "loopback", "0");

    SoapySDRDevice *rx_sdr = SoapySDRDevice_make(&rx_args);
    SoapySDRKwargs_clear(&rx_args);

    if (rx_sdr == NULL) {
        fprintf(stderr, "Failed to open RX device: %s\n", rx_uri);
        SoapySDRDevice_unmake(tx_sdr);
        free(result);
        return 1;
    }

    int sample_rate = 1000000;     // 1 MHz
    int carrier_freq = 800000000;  // 800 MHz

    // Настройка TX
    SoapySDRDevice_setSampleRate(tx_sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(tx_sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);
    SoapySDRDevice_setGain(tx_sdr, SOAPY_SDR_TX, 0, -10.0);

    // Настройка RX
    SoapySDRDevice_setSampleRate(rx_sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(rx_sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);
    SoapySDRDevice_setGain(rx_sdr, SOAPY_SDR_RX, 0, 20.0);

    // Создание потоков
    size_t channel = 0;
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(tx_sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, &channel, 1, NULL);
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(rx_sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, &channel, 1, NULL);

    SoapySDRDevice_activateStream(tx_sdr, txStream, 0, 0, 0);
    SoapySDRDevice_activateStream(rx_sdr, rxStream, 0, 0, 0);

    size_t tx_mtu = SoapySDRDevice_getStreamMTU(tx_sdr, txStream);
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(rx_sdr, rxStream);

    // Подготовка TX буфера
    int16_t *tx_buff = (int16_t *)malloc(2 * lenTX * sizeof(int16_t));
    for (int i = 0; i < lenTX; i++) {
        // Масштабируем сигнал: [-1, 1] → [-2047*16, 2047*16] ≈ 12-битный сигнал, сдвинутый влево
        int16_t val = (int16_t)(result[i] * 2047);
        tx_buff[2 * i]     = (val << 4);  // In-phase
        tx_buff[2 * i + 1] = 0;           // Quadrature = 0 (BPSK)
    }

    FILE *file1 = fopen("./pcm/txdata.pcm", "wb");
    if (file1) {
        fwrite(tx_buff, sizeof(int16_t), 2 * lenTX, file1);
        fclose(file1);
    }

    // Отправка сигнала один раз
    const long timeoutUs = 400000;
    void *tx_buffs[] = {tx_buff};
    int tx_flags = SOAPY_SDR_HAS_TIME | SOAPY_SDR_END_BURST;
    long long tx_time = 0; // мгновенная отправка

    printf("Sending TX signal...\n");
    int st = SoapySDRDevice_writeStream(tx_sdr, txStream, (const void * const*)tx_buffs, lenTX, &tx_flags, tx_time, timeoutUs);
    if (st < 0) {
        fprintf(stderr, "TX error: %d\n", st);
    }

    // Приём на другом устройстве
    int16_t *rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));
    FILE *file2 = fopen("./pcm/rxdata.pcm", "wb");
    if (!file2) {
        perror("fopen rxdata.pcm");
    }

    printf("Receiving...\n");
    size_t iteration_count = 5;
    for (size_t i = 0; i < iteration_count; i++) {
        void *rx_buffs[] = {rx_buffer};
        int flags;
        long long timeNs;
        int sr = SoapySDRDevice_readStream(rx_sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

        if (sr > 0 && file2) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, file2);
        }
        printf("RX buffer %zu: %d samples, time=%lld\n", i, sr, timeNs);
    }

    if (file2) fclose(file2);
    free(rx_buffer);
    free(tx_buff);
    free(result);

    // Завершение работы
    SoapySDRDevice_deactivateStream(tx_sdr, txStream, 0, 0);
    SoapySDRDevice_deactivateStream(rx_sdr, rxStream, 0, 0);

    SoapySDRDevice_closeStream(tx_sdr, txStream);
    SoapySDRDevice_closeStream(rx_sdr, rxStream);

    SoapySDRDevice_unmake(tx_sdr);
    SoapySDRDevice_unmake(rx_sdr);

    printf("Done. Data saved to ./pcm/txdata.pcm and ./pcm/rxdata.pcm\n");
    return 0;
}