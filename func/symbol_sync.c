#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// Структура для комплексных чисел (I/Q)
typedef struct {
    float i;  // in-phase (I)
    float q;  // quadrature (Q)
} complex_t;

// Функция для загрузки PCM файла (16-bit I/Q чередованные)
complex_t* load_pcm_file(const char* filename, int* num_samples) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Ошибка открытия файла: %s\n", filename);
        return NULL;
    }
    
    // Определяем размер файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Количество 16-битных отсчетов
    int total_shorts = file_size / sizeof(int16_t);
    *num_samples = total_shorts / 2;  // Каждый комплексный отсчет = 2 int16
    
    // Выделяем память
    complex_t* samples = (complex_t*)malloc(*num_samples * sizeof(complex_t));
    if (!samples) {
        printf("Ошибка выделения памяти\n");
        fclose(file);
        return NULL;
    }
    
    // Читаем и конвертируем данные
    int16_t* buffer = (int16_t*)malloc(file_size);
    fread(buffer, 1, file_size, file);
    
    for (int n = 0; n < *num_samples; n++) {
        samples[n].i = (float)buffer[2*n] / 32768.0f;   // Нормализация к [-1, 1]
        samples[n].q = (float)buffer[2*n + 1] / 32768.0f;
    }
    
    free(buffer);
    fclose(file);
    return samples;
}

// Gardner TED (non-data-aided)
float gardner_ted(complex_t* samples, int sps, int start_idx) {
    int k = start_idx;
    
    // x((k-1/2)T + τ) - средний отсчет
    complex_t x_half = samples[k + sps/2];
    
    // x((k-1)T + τ) - предыдущий символ
    complex_t x_prev = samples[k];
    
    // x(kT + τ) - текущий символ
    complex_t x_curr = samples[k + sps];
    
    // Формула Gardner
    float error_i = x_half.i * (x_prev.i - x_curr.i);
    float error_q = x_half.q * (x_prev.q - x_curr.q);
    
    return error_i + error_q;
}

// Основная функция
int main() {
    // Параметры (можно менять здесь)
    const char* filename = "../pcm/txdata.pcm";  // Имя файла
    int sps = 10;  // Отсчетов на символ
    
    printf("=== TED Детектор временной ошибки ===\n");
    printf("Файл: %s\n", filename);
    printf("Отсчетов на символ: %d\n\n", sps);
    
    // 1. Загрузка данных
    int num_samples = 0;
    complex_t* samples = load_pcm_file(filename, &num_samples);
    if (!samples) {
        printf("Не удалось загрузить файл. Убедитесь, что файл '%s' существует.\n", filename);
        return 1;
    }
    
    printf("Загружено отсчетов: %d\n", num_samples);
    
    // Проверка, что данных достаточно
    if (num_samples < 3 * sps) {
        printf("Слишком мало данных для обработки!\n");
        free(samples);
        return 1;
    }
    
    // 2. Вычисление ошибок синхронизации
    int num_symbols = (num_samples - 2 * sps) / sps;
    printf("Будет обработано символов: %d\n\n", num_symbols);
    
    float sum_error = 0.0f;
    float max_error = 0.0f;
    float min_error = 0.0f;
    
    for (int sym = 0; sym < num_symbols; sym++) {
        int start_idx = sym * sps;
        float error = gardner_ted(samples, sps, start_idx);
        
        sum_error += error;
        
        if (error > max_error) max_error = error;
        if (error < min_error) min_error = error;
        
        
        // Прогресс
        if (sym % 1000 == 0 && sym > 0) {
            printf("Обработано %d символов...\n", sym);
        }
    }
    
    // 3. Статистика
    printf("\n=== СТАТИСТИКА ===\n");
    printf("Всего символов: %d\n", num_symbols);
    printf("Средняя ошибка: %+10.6f\n", sum_error / num_symbols);
    printf("Максимальная ошибка: %+10.6f\n", max_error);
    printf("Минимальная ошибка: %+10.6f\n", min_error);
    printf("Размах ошибки: %10.6f\n", max_error - min_error);
    
    // 4. Анализ S-кривой
    printf("\n=== АНАЛИЗ СИНХРОНИЗАЦИИ ===\n");
    float avg_error = sum_error / num_symbols;
    
    if (avg_error > 0.05f) {
        printf("СИНХРОНИЗАЦИЯ: Ошибка положительная -> Время отсчета РАННЕЕ\n");
        printf("РЕКОМЕНДАЦИЯ: Увеличить задержку взятия отсчета\n");
    }
    else if (avg_error < -0.05f) {
        printf("СИНХРОНИЗАЦИЯ: Ошибка отрицательная -> Время отсчета ПОЗДНЕЕ\n");
        printf("РЕКОМЕНДАЦИЯ: Уменьшить задержку взятия отсчета\n");
    }
    else {
        printf("СИНХРОНИЗАЦИЯ: Ошибка близка к нулю -> Время отсчета ОПТИМАЛЬНО\n");
        printf("РЕКОМЕНДАЦИЯ: Корректировка не требуется\n");
    }
    
    // Очистка памяти
    free(samples);

    
    return 0;
}