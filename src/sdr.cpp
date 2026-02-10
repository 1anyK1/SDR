#include "./common.h"

int sdr_run(int argc, char *argv[]){

    if (argc < 3) {
        printf("Использовать uri(SoapySDRUtil --find): %s <usb:tx_uri> <usb:rx_uri>\n", argv[0]);
        return EXIT_FAILURE;
    }    

    char *tx_uri = argv[1];
    char *rx_uri = argv[2];
    
    printf("BPSK Transmission\n");
    printf("TX URI: %s\n", tx_uri);
    printf("RX URI: %s\n", rx_uri);
    
    int bit_arr[] = {0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 
        0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0};
    int len_arr = sizeof(bit_arr) / sizeof(bit_arr[0]);
    printf("Length of bit array: %d\n", len_arr);
    
    int *bpsk_arr = to_bpsk(bit_arr, len_arr);
    printf("BPSK: ");
    for(int i = 0; i < len_arr; i++) {
        printf("%d ", bpsk_arr[i]);
    }
    printf("\n");

    int *bpsk_after_arr = upsampling(bpsk_arr, len_arr);
    printf("upsampling: ");
    for(int i = 0; i < len_arr * 10; i++) {
        printf("%d ", bpsk_after_arr[i]);
    }
    printf("\n");

    int pulse[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int pulse_length = 10;

    int *conv_result = convolution(bpsk_after_arr, pulse, len_arr * 10, pulse_length);

    printf("Convolution: ");
    int conv_length = len_arr * 10;
    for(int i = 0; i < conv_length; i++) {
        printf("%d ", conv_result[i]);
    }
    printf("\n");

    int16_t *tx_samples = (int16_t *)malloc(conv_length * 2 * sizeof(int16_t));
    int scale_factor = 1;
    
    for(int i = 0; i < conv_length; i++) {
        tx_samples[i * 2] = (int16_t)(conv_result[i] * scale_factor);
        tx_samples[i * 2 + 1] = 0;
    }
    
    printf("Converted to %d I/Q samples\n", len_arr * 10);

    //запись tx
    FILE *fpt_pcm = fopen("./pcm/txdata.pcm", "wb");
    if (fpt_pcm) {
        fwrite(tx_samples, sizeof(int16_t), len_arr * 10 * 2, fpt_pcm);
        fclose(fpt_pcm);
        printf("Saved samples to rxdata.pcm\n");
    }

    // Инициализация TX устройства
    SoapySDRKwargs tx_args = {};
    SoapySDRKwargs_set(&tx_args, "driver", "plutosdr"); 
    SoapySDRKwargs_set(&tx_args, "uri", tx_uri);
    SoapySDRKwargs_set(&tx_args, "direct", "1");
    SoapySDRKwargs_set(&tx_args, "timestamp_every", "1920");
    
    SoapySDRDevice *tx_sdr = SoapySDRDevice_make(&tx_args);
    SoapySDRKwargs_clear(&tx_args);

    if (tx_sdr == NULL) {
        printf("Failed to create TX SDR device\n");
        return EXIT_FAILURE;
    }

    // Инициализация RX устройства
    SoapySDRKwargs rx_args = {};
    SoapySDRKwargs_set(&rx_args, "driver", "plutosdr"); 
    SoapySDRKwargs_set(&rx_args, "uri", rx_uri);
    SoapySDRKwargs_set(&rx_args, "direct", "1");
    SoapySDRKwargs_set(&rx_args, "timestamp_every", "1920");
    
    SoapySDRDevice *rx_sdr = SoapySDRDevice_make(&rx_args);
    SoapySDRKwargs_clear(&rx_args);

    if (rx_sdr == NULL) {
        printf("Failed to create RX SDR device\n");
        SoapySDRDevice_unmake(tx_sdr);
        return EXIT_FAILURE;
    }

    int sample_rate = 1e6;
    int carrier_freq = 800e6;

    // Настройка TX устройства
    SoapySDRDevice_setSampleRate(tx_sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(tx_sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    // Настройка RX устройства
    SoapySDRDevice_setSampleRate(rx_sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(rx_sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

    // Инициализация количества каналов
    const size_t channels_count = 0;
    size_t channels[] = {0};
    
    // Настройки усилителей
    SoapySDRDevice_setGain(tx_sdr, SOAPY_SDR_TX, channels_count, -30.0);
    SoapySDRDevice_setGain(rx_sdr, SOAPY_SDR_RX, channels_count, 25.0);

    const size_t channel_count = 1;
    
    // Формирование потоков
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(tx_sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(rx_sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    if (txStream == NULL) {
        printf("Failed to setup TX stream\n");
        SoapySDRDevice_unmake(tx_sdr);
        SoapySDRDevice_unmake(rx_sdr);
        return EXIT_FAILURE;
    }

    if (rxStream == NULL) {
        printf("Failed to setup RX stream\n");
        SoapySDRDevice_unmake(tx_sdr);
        SoapySDRDevice_unmake(rx_sdr);
        return EXIT_FAILURE;
    }

    SoapySDRDevice_activateStream(tx_sdr, txStream, 0, 0, 0);
    SoapySDRDevice_activateStream(rx_sdr, rxStream, 0, 0, 0);

    // Получение MTU
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(tx_sdr, txStream);
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(rx_sdr, rxStream);

    printf("TX MTU: %zu, RX MTU: %zu\n", tx_mtu, rx_mtu);

    int16_t *tx_buff = (int16_t*)malloc(2 * tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * rx_mtu * sizeof(int16_t));

    // Файл для записи принятых данных rx
    FILE *fptr = fopen("./pcm/rxdata.pcm", "wb");
    if(fptr == NULL) {
        perror("fptr err");
        free(tx_buff);
        free(rx_buffer);
        free(tx_samples);
        free(bpsk_arr);
        free(bpsk_after_arr);
        free(conv_result);
        SoapySDRDevice_unmake(tx_sdr);
        SoapySDRDevice_unmake(rx_sdr);
        return EXIT_FAILURE;
    }

    const long timeoutUs = 400000;

    // Передача подготовленных сэмплов
    int total_samples_sent = 0;
    int flags = SOAPY_SDR_HAS_TIME;
    long long tx_time = 0;

    printf("Starting transmission...\n");
    void *rx_buffs[] = {rx_buffer};
    int rx_flags;
    long long timeNs;
    int sr = SoapySDRDevice_readStream(rx_sdr, rxStream, rx_buffs, rx_mtu, &rx_flags, &timeNs, timeoutUs);
    if (sr > 0) {
        tx_time = timeNs + (4 * 1000 * 1000); // на 4 мс в будущее
        printf("Got timestamp for sync: %lld\n", timeNs);
    }

    while(1){
        // ---- СБРОС ПАРАМЕТРОВ ДЛЯ НОВОЙ ИТЕРАЦИИ ----
        total_samples_sent = 0;
        int flags = SOAPY_SDR_HAS_TIME;
        long long tx_time = 0;
        
        // ---- ПОЛУЧАЕМ ТЕКУЩЕЕ ВРЕМЯ ДЛЯ СИНХРОНИЗАЦИИ ----
        void *sync_buffs[] = {rx_buffer};
        int sync_flags;
        long long sync_time;
        int sync_result = SoapySDRDevice_readStream(rx_sdr, rxStream, sync_buffs, 1, 
                                                &sync_flags, &sync_time, 10000);
        
        if (sync_result > 0) {
            tx_time = sync_time + 10000000;  // +10 мс в будущее
            printf("Sync timestamp: %lld\n", sync_time);
        } else {
            // Если не получили время, используем относительное
            printf("No sync timestamp, using relative timing\n");
        }

        printf("Starting transmission...\n");
        while (total_samples_sent < conv_length) {
            int samples_to_send = (conv_length - total_samples_sent < tx_mtu) ? 
                                (conv_length - total_samples_sent) : tx_mtu;

            // Копируем сэмплы в tx_buff
            for (int i = 0; i < samples_to_send * 2; i++) {
                tx_buff[i] = tx_samples[total_samples_sent * 2 + i] * 1500 << 4;
            }

            // Очищаем остаток буфера
            for (int i = samples_to_send * 2; i < tx_mtu * 2; i++) {
                tx_buff[i] = 0;
            }

            void *tx_buffs[] = {tx_buff};
            int st = SoapySDRDevice_writeStream(tx_sdr, txStream, (const void * const*)tx_buffs, 
                                            tx_mtu, &flags, tx_time, timeoutUs);
            
            if (st < 0) {
                printf("TX Failed: %i\n", st);
                break;
            }
            
            total_samples_sent += samples_to_send;
            tx_time += (samples_to_send * 1000000000LL) / sample_rate;
            
            printf("Sent %d samples, total: %d/%d\n", samples_to_send, total_samples_sent, conv_length);
        }

        printf("Transmission completed. Total samples sent: %d\n", total_samples_sent);


        printf("Starting reception...\n");
        size_t iteration_count = 10;
        for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
            void *rx_buffs[] = {rx_buffer};
            int rx_flags;
            long long timeNs;
            
            int sr = SoapySDRDevice_readStream(rx_sdr, rxStream, rx_buffs, rx_mtu, 
                                            &rx_flags, &timeNs, timeoutUs);

            if (sr > 0) {
                fwrite(rx_buffer, sr * 2 * sizeof(int16_t), 1, fptr);
                fflush(fptr);  // Сбрасываем на диск
                g_sdr_data.update_samples(rx_buffer, sr);
                printf("Received buffer %lu: %d samples\n", buffers_read, sr);
            } else {
                printf("RX Failed: %i\n", sr);
            }
        }

        printf("Reception completed\n");
        
    }

    fclose(fptr);
    free(tx_buff);
    free(rx_buffer);
    free(tx_samples);
    free(bpsk_arr);
    free(bpsk_after_arr);
    free(conv_result);

    SoapySDRDevice_deactivateStream(tx_sdr, txStream, 0, 0);
    SoapySDRDevice_deactivateStream(rx_sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(tx_sdr, txStream);
    SoapySDRDevice_closeStream(rx_sdr, rxStream);
    SoapySDRDevice_unmake(tx_sdr);
    SoapySDRDevice_unmake(rx_sdr);

    return EXIT_SUCCESS;

}