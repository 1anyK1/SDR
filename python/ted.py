import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from math import sqrt

# Загрузка данных из файла .pcm (предполагаем 16-bit I/Q interleaved: I0,Q0,I1,Q1,...)
def load_pcm_file(filename, sample_rate=1.0):
    data = np.fromfile(filename, dtype=np.int16)  # 16-bit signed
    if data.size % 2 != 0:
        # отрезаем последний сэмпл, если нечётное количество
        data = data[:-1]
    I = data[0::2].astype(np.float32)
    Q = data[1::2].astype(np.float32)

    samples = I + 1j * Q
    return samples

# Алгоритм Gardner TED для конкретного символа
def gardner_ted(samples, sps, sym_idx, offset):

    idx_prev = sym_idx * sps + offset - sps
    idx_curr = sym_idx * sps + offset
    idx_half = idx_prev + (sps // 2)

    N = len(samples)
    if idx_prev < 0 or idx_half < 0 or idx_curr >= N or idx_half >= N:
        return 0.0

    x_prev = samples[idx_prev]
    x_half = samples[idx_half]
    x_curr = samples[idx_curr]

    diff = (x_prev - x_curr)
    e_complex = x_half * np.conjugate(diff)
    error = np.real(e_complex)


    return float(error)

# Функция для расчета параметров фильтра контура
def calculate_loop_filter_params(N_sps, zeta=0.707, Bn_Ts=0.01, Kp=1.0):
    theta = (Bn_Ts / N_sps) / (zeta + 1.0/(4.0*zeta))
    denominator = 1.0 + 2.0*zeta*theta + theta**2
    K1 = -4.0 * zeta * theta / (denominator * Kp)
    K2 = -4.0 * theta**2 / (denominator * Kp)
    return K1, K2, theta

# Символьная синхронизация с петлей
def symbol_sync_loop(samples, sps, initial_offset=0):
    # Параметры петли
    zeta = sqrt(2) / 2  # коэффициент демпфирования
    Bn_Ts = 0.01  # нормированная полоса контура
    Kp = 0.002      # коэффициент усиления детектора (можно подбирать)

    # Вычисляем параметры фильтра контура
    K1, K2, theta = calculate_loop_filter_params(sps, zeta, Bn_Ts, Kp)

    print(f"Параметры фильтра контура:")
    print(f"  K1 = {K1:.6e}")
    print(f"  K2 = {K2:.6e}")
    print(f"  theta = {theta:.6e}")
    print(f"  zeta = {zeta}")
    print(f"  Bn*Ts = {Bn_Ts}")
    print(f"  Kp = {Kp}")

    # Инициализация переменных
    p1 = 0.0  # переменная ветви K1
    p2 = 0.0  # переменная ветви K2 (дробное смещение в диапазоне [0,1))
    offset = int(initial_offset)  # текущее целочисленное смещение (0..sps-1)

    # Массивы для результатов
    synced_samples = []  # синхронизированные отсчеты (по символам)
    offsets = []         # смещения для каждого символа
    ted_errors = []      # ошибки TED (по символам)
    p1_history = []      # история p1 (для отладки)
    p2_history = []      # история p2 (для отладки)

    # Количество символов для обработки (оставляем запас 2 символов для prev/half)
    max_symbols = len(samples) // sps
    if max_symbols < 3:
        return np.array([]), np.array([]), np.array([]), np.array([]), np.array([])

    print(f"\nНачинаем синхронизацию...")
    print(f"Максимальное количество символов: {max_symbols}")

    # начинаем с sym_idx = 1, чтобы был предыдущий символ (k-1)
    for sym_idx in range(1, max_symbols):
        # Вычисляем ошибку TED для текущего символа и текущего смещения
        error = gardner_ted(samples, sps, sym_idx, offset)
        ted_errors.append(error)

        # Обновляем ветвь K1
        p1 = error * K1
        p1_history.append(p1)

        # Обновляем ветвь K2
        p2 = p2 + p1 + error * K2

        # Нормируем p2 в [0,1)
        p2 = p2 % 1.0

        p2_history.append(p2)

        # Вычисляем новое целочисленное смещение
        offset = int(round(p2 * sps)) % sps

        # Сохраняем смещение
        offsets.append(offset)

        # Извлекаем синхронизированный отсчет для текущего символа (k*T + tau)
        sample_idx = sym_idx * sps + offset
        if 0 <= sample_idx < len(samples):
            synced_samples.append(samples[sample_idx])
        else:
            synced_samples.append(0 + 0j)

        # Прогресс (редко печатаем)
        if sym_idx % 10000 == 0 and sym_idx > 0:
            print(f"  Обработано {sym_idx} символов...")

    return (np.array(synced_samples), np.array(offsets),
            np.array(ted_errors), np.array(p1_history), np.array(p2_history))

# Основная программа
if __name__ == "__main__":
    # 1. Загрузка файла .pcm
    filename = "../pcm/rxdata.pcm"  # укажите имя файла
    print(f"Загрузка файла: {filename}")
    samples = load_pcm_file(filename)

    print(f"Загружено отсчетов: {len(samples)}")

    # 2. Параметры
    sps = 10  # samples per symbol

    # 3. Простой анализ сигнала (до синхронизации)
    print(f"\n=== Анализ сигнала до синхронизации ===")

    # Простой Gardner TED без петли (по символам)
    ted_errors_simple = []
    max_symbols = len(samples) // sps
    for k in range(1, max_symbols - 1):
        offset0 = 0  # грубая оценка смещения (можно заменить на другое)
        e = gardner_ted(samples, sps, k, offset0)
        ted_errors_simple.append(e)

    ted_errors_simple = np.array(ted_errors_simple)
    if ted_errors_simple.size > 0:
        print(f"Средняя ошибка TED (простой): {np.mean(ted_errors_simple):.6f}")
        print(f"Максимальная ошибка TED (простой): {np.max(np.abs(ted_errors_simple)):.6f}")
    else:
        print("Недостаточно данных для простого TED.")

    # 4. Запуск петли символьной синхронизации
    print(f"\n=== Запуск петли символьной синхронизации ===")

    # Начальное смещение (можно попробовать разные значения)
    initial_offset = 0

    # Запускаем петлю синхронизации
    synced_samples, offsets, ted_errors, p1_history, p2_history = symbol_sync_loop(
        samples, sps, initial_offset
    )

    # 5. Анализ результатов
    print(f"\n=== Результаты синхронизации ===")
    print(f"Синхронизировано отсчетов (символов): {len(synced_samples)}")
    if len(ted_errors) > 0:
        print(f"Средняя ошибка TED (в петле): {np.mean(ted_errors):.6f}")
    print(f"Среднее смещение: {np.mean(offsets) if len(offsets)>0 else 0:.2f} отсчетов")
    print(f"Стандартное отклонение смещения: {np.std(offsets) if len(offsets)>0 else 0:.2f} отсчетов")

    # 6. Визуализация результатов
    plt.figure(figsize=(15, 10))

    # График 1: Ошибки TED
    plt.subplot(3, 2, 1)
    if len(ted_errors) > 0:
        plt.plot(ted_errors, 'b-', linewidth=0.5, alpha=0.7)
    plt.xlabel('Номер символа')
    plt.ylabel('Ошибка TED')
    plt.title(f'Ошибки TED (все {len(ted_errors)} символов)')
    plt.grid(True, alpha=0.3)
    plt.axhline(y=0, color='r', linestyle='--', alpha=0.5)

    # График 2: Смещения
    plt.subplot(3, 2, 2)
    if len(offsets) > 0:
        plt.plot(offsets[:1000], 'g-', linewidth=0.5)
    plt.xlabel('Номер символа')
    plt.ylabel('Смещение (отсчеты)')
    plt.title('Смещения отсчетов (первые 1000 символов)')
    plt.grid(True, alpha=0.3)
    plt.ylim([0, sps])

    # График 3: p1 и p2
    plt.subplot(3, 2, 3)
    if len(p1_history) > 0:
        plt.plot(p1_history[:500], label='p1', linewidth=0.5)
    if len(p2_history) > 0:
        plt.plot(p2_history[:500], label='p2', linewidth=0.5)
    plt.xlabel('Номер символа')
    plt.ylabel('Значение')
    plt.title('Переменные петли p1 и p2 (первые 500 символов)')
    plt.grid(True, alpha=0.3)
    plt.legend()

    # График 4: Гистограмма смещений
    plt.subplot(3, 2, 4)
    if len(offsets) > 0:
        plt.hist(offsets, bins=sps, edgecolor='black', alpha=0.7)
    plt.xlabel('Смещение (отсчеты)')
    plt.ylabel('Частота')
    plt.title('Распределение смещений')
    plt.grid(True, alpha=0.3)

    # График 5: Созвездие до синхронизации
    plt.subplot(3, 2, 5)
    rough_synced = samples[::sps]
    if len(rough_synced) > 0:
        plt.scatter(np.real(rough_synced[:1000]), np.imag(rough_synced[:1000]), s=1, alpha=0.5)
    plt.xlabel('I компонента')
    plt.ylabel('Q компонента')
    plt.title('Созвездие ДО синхронизации')
    plt.grid(True, alpha=0.3)
    plt.axis('equal')

    # График 6: Созвездие после синхронизации
    plt.subplot(3, 2, 6)
    if len(synced_samples) > 0:
        plt.scatter(np.real(synced_samples[:1000]), np.imag(synced_samples[:1000]), s=1, alpha=0.5)
    plt.xlabel('I компонента')
    plt.ylabel('Q компонента')
    plt.title('Созвездие ПОСЛЕ синхронизации')
    plt.grid(True, alpha=0.3)
    plt.axis('equal')

    plt.tight_layout()
    plt.show()
