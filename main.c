#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>
#include <complex.h>

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

int main(){

    int str[] = {0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 
        0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0};

    int len = sizeof(str) / sizeof(str[0]);

    printf("%d\n", len);
    int *IQ = to_bpsk(str, len);
    int lenIQ = len*2;
    printf("IQ\n");
    for (int i = 0; i < lenIQ; i++){
        printf("%d ", IQ[i]);
    }
    printf("\n");

    int sampling = 10;

    int *tx = upsampling(IQ, lenIQ, sampling);
    free(IQ);
    int lenTX = lenIQ * sampling;
    printf("TX\n");
    for (int i = 0; i < len*sampling; i++){
        printf("%d ", tx[i]);
    }
    printf("\n");
    int filter[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    int *result = convolve_filter(tx, filter, lenTX, sampling);
    free(tx);

    printf("RESULT\n");
    for (int i = 0; i < len*sampling; i++){
        printf("%d ", result[i]);
    }
    printf("\n");

    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");        // Говорим какой Тип устройства 
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");           // Способ обмена сэмплами (USB)
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // Или по IP-адресу
    }
    SoapySDRKwargs_set(&args, "direct", "1");               // 
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
    SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
    SoapySDRKwargs_clear(&args);

    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq , NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq , NULL);

    // Инициализация количества каналов RX\\\\TX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    // Настройки усилителей на RX\\\\TX
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels, 20.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels, -10.0);// Усиление передатчика

    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    // Формирование потоков для передачи и приема сэмплов
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

    // Получение MTU (Maximum Transmission Unit), в нашем случае - размер буферов. 
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    // Выделяем память под буферы RX и TX
    int16_t rx_buffer[2*rx_mtu];

    // size_t sample_count;
    // FILE *filename = "./pcm/audio.pcm";
    // int16_t *samples = read_pcm(filename, &sample_count);
    // printf("OUR SAMPLE COUNT = %d\n", sample_count);

    int16_t tx_buff[2*lenTX];

    for(int i = 0; i < lenTX; i++){
        tx_buff[2*i] = (int16_t)result[i] * 1500 << 4;
        tx_buff[2*i+1] = 0;
    }

    FILE *file1 = fopen("./pcm/txstart.pcm", "w");
    fwrite(tx_buff, sizeof(int16_t), 2 * lenTX, file1);
    fclose(file1);

    //prepare fixed bytes in transmit buffer
    //we transmit a pattern of FFFF FFFF [TS_0]00 [TS_1]00 [TS_2]00 [TS_3]00 [TS_4]00 [TS_5]00 [TS_6]00 [TS_7]00 FFFF FFFF
    //that is a flag (FFFF FFFF) followed by the 64 bit timestamp, split into 8 bytes and packed into the lsb of each of the DAC words.
    //DAC samples are left aligned 12-bits, so each byte is left shifted into place
    for(size_t i = 0; i < 2; i++)
    {
        tx_buff[0 + i] = 0xffff;
        // 8 x timestamp words
        tx_buff[10 + i] = 0xffff;
    }

    const long  timeoutUs = 400000;
    long long last_time = 0;
    // Количество итерация чтения из буфера
    size_t iteration_count = 5;


    FILE *file2 = fopen("./pcm/txdata.pcm", "rw");

    // Начинается работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags;        // flags set by receive operation
        long long timeNs; //timestamp for receive buffer
        
        // считали буффер RX, записали его в rx_buffer
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

        fwrite(rx_buffer, sizeof(int16_t), 2 * rx_mtu, file2);

        // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);
        last_time = timeNs;

        // Переменная для времени отправки сэмплов относительно текущего приема
        long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 [мс] в будущее

        void *tx_buffs[] = {tx_buff};

        int tx_flags = SOAPY_SDR_HAS_TIME;

        if(buffers_read == 0){

            int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &tx_flags, tx_time, timeoutUs);

        }
        

    }
    // Исправление: используйте двойные кавычки для строки режима


    // Записываем данные в формате I Q (через пробел)

    int buffR[5500];

    fgets(buffR, sizeof(buffR), file2);

    int lenR = sizeof(buffR) / sizeof(buffR[0]);

    for (int i = 0; i < lenR; i++){
        printf("%d ", buffR[i]);
    }
    putchar('\n');

    printf("%d\n", lenR);

    free(result);
    fclose(file2);
    printf("Save success\n");

    //stop streaming
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    //shutdown the stream
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    return 0;

}