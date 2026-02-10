import numpy as np
import matplotlib.pyplot as plt

def gardner_ted(I, Q, n, Nsps):
    try:
        r0 = I[n]
        r1 = I[n + Nsps // 2]
        r2 = I[n + Nsps]
        i0 = Q[n]
        i1 = Q[n + Nsps // 2]
        i2 = Q[n + Nsps]
        e = (r2 - r0) * r1 + (i2 - i0) * i1
        return e
    except IndexError:
        return 0.0

def estimate_frequency_offset(y, Nt, L=1):

    N = len(y)
    if N < 2 * Nt:
        return 0.0
    
    # Вычисление суммы для оценки смещения
    sum_val = 0j
    for l in range(L, Nt):
        if l + Nt < N:
            sum_val += y[l + Nt] * np.conj(y[l])
    
    # Вычисление нормированного частотного смещения
    if abs(sum_val) > 0:
        epsilon_hat = np.angle(sum_val) / (2 * np.pi * Nt)
    else:
        epsilon_hat = 0.0
    
    return epsilon_hat

def correct_frequency_offset(signal, epsilon_hat):

    n = np.arange(len(signal))
    correction = np.exp(-1j * 2 * np.pi * epsilon_hat * n)
    return signal * correction

def main():
    filename = "../pcm/rxdata.pcm"
    try:
        rx = np.fromfile(filename, dtype=np.int16)
        if len(rx) % 2 != 0:
            rx = rx[:-1]
        real = rx[0::2].astype(np.float64)
        imag = rx[1::2].astype(np.float64)
    except FileNotFoundError:
        print(f"Файл {filename} не найден. Генерируем тестовый QPSK сигнал.")
        np.random.seed(0)
        num_symbols = 10000
        symbols = np.random.choice([1+1j, -1+1j, -1-1j, 1-1j], size=num_symbols)
        Nsps = 10
        
        # Добавляем преамбулу (две одинаковые части)
        Nt = 32  # Длина одной части преамбулы
        preamble_symbols = np.random.choice([1+1j, -1+1j, -1-1j, 1-1j], size=Nt)
        preamble = np.tile(preamble_symbols, 2)  # Две одинаковые части
        
        # Формируем полный сигнал с преамбулой
        full_symbols = np.concatenate([preamble, symbols])
        tx = np.repeat(full_symbols, Nsps)
        
        # Добавляем частотное смещение для тестирования
        epsilon_true = 0.005  # Истинное нормированное смещение
        t = np.arange(len(tx))
        freq_offset = np.exp(1j * 2 * np.pi * epsilon_true * t / Nsps)
        tx_with_offset = tx * freq_offset
        
        real = np.real(tx_with_offset) + 0.01 * np.random.randn(len(tx))
        imag = np.imag(tx_with_offset) + 0.01 * np.random.randn(len(tx))
        
        print(f"Сгенерирован тестовый сигнал с частотным смещением ε={epsilon_true}")

    start, end = 5000, 20000
    if len(real) > end:
        real = real[start:end]
        imag = imag[start:end]

    Nsps = 10
    BnTs = 0.01
    Kp = 1.0
    zeta = 0.707  

    theta = (BnTs / Nsps) / (zeta + 1/(4*zeta))
    denom = (1 + 2*zeta*theta + theta**2) * Kp
    K1 = (-4 * zeta * theta) / denom
    K2 = (-4 * theta**2) / denom

    print(f"K1 = {K1:.6f}")
    print(f"K2 = {K2:.6f}")
    print(f"theta = {theta:.6f}")
    print(f"BnTs = {BnTs}, Nsps = {Nsps}, Kp = {Kp}, zeta = {zeta:.3f}\n")

    # Согласованный фильтр
    h = np.ones(Nsps, dtype=np.float64)
    conv_real = np.convolve(real, h, mode='same')
    conv_imag = np.convolve(imag, h, mode='same')
    
    # Объединяем в комплексный сигнал
    y = conv_real + 1j * conv_imag

    print("=== ОЦЕНКА ЧАСТОТНОГО СМЕЩЕНИЯ ===")
    # Параметры преамбулы
    Nt = 32  # Длина одной части преамбулы в отсчетах
    L = 1    # Длина импульсной характеристики канала
    
    # Оцениваем частотное смещение
    epsilon_hat = estimate_frequency_offset(y, Nt, L)
    print(f"Оценка частотного смещения: ε̂ = {epsilon_hat:.6f}")
    print(f"Частотное смещение в Гц (при T=1): fo = {epsilon_hat:.6f} * Fs")
    
    # Коррекция частотного смещения
    print("\n=== КОРРЕКЦИЯ ЧАСТОТНОГО СМЕЩЕНИЯ ===")
    y_corrected = correct_frequency_offset(y, epsilon_hat)
    print("Частотное смещение скорректировано")
    
    # Обновляем real и imag после коррекции
    conv_real = np.real(y_corrected)
    conv_imag = np.imag(y_corrected)

    print("\nАнализ зависимости TED от offset (S-кривая):")
    n_center = Nsps * 100
    offsets = np.arange(-Nsps//2, Nsps//2 + 1)
    ted_values = []

    for offset in offsets:
        e = gardner_ted(conv_real, conv_imag, n_center + offset, Nsps)
        ted_values.append(e)
        print(f"  offset={offset:3d}, e={e:10.8f}")

    min_index = np.argmin(np.abs(ted_values))
    min_offset = offsets[min_index]
    print(f"\nОптимальный offset = {min_offset}")
    print(f"Значение TED = {ted_values[min_index]:.8f}\n")

    num_symbols = 5000
    p1_accum = 0.0
    p2_accum = 0.0 

    acc = 0.0
    symbol_indices = []
    e_history = []
    p2_history = []

    n = 0 
    symbols_found = 0

    while symbols_found < num_symbols and n + Nsps < len(conv_real):
        e = gardner_ted(conv_real, conv_imag, int(n), Nsps)
        e_history.append(e)

        p1 = e * K1
        p1_accum = p1 
        p2 = p2_accum + p1 + e * K2
        p2_accum = p2 % 1.0
        p2_history.append(p2_accum)

        acc += 1.0 + p2_accum
        if acc >= Nsps:
            index = int(acc)
            if index < len(conv_real):
                symbol_indices.append(index)
                symbols_found += 1
            acc -= Nsps 

        n += 1

    print(f"Найдено {len(symbol_indices)} символов")

    # Извлекаем символы после коррекции
    symbols_complex = [y_corrected[i] for i in symbol_indices if i < len(y_corrected)]

    # Построение созвездия до и после коррекции
    plt.figure(figsize=(15, 5))
    
    # Созвездие до коррекции
    plt.subplot(1, 3, 1)
    symbols_before = [y[i] for i in symbol_indices if i < len(y)]
    if symbols_before:
        plt.scatter(np.real(symbols_before[:1000]), np.imag(symbols_before[:1000]), 
                   alpha=0.5, s=10)
    plt.axhline(0, color='k', linestyle='--', alpha=0.5)
    plt.axvline(0, color='k', linestyle='--', alpha=0.5)
    plt.grid(True, alpha=0.3)
    plt.xlabel('I')
    plt.ylabel('Q')
    plt.title('Созвездие до коррекции')
    plt.axis('equal')
    
    # Созвездие после коррекции
    plt.subplot(1, 3, 2)
    if symbols_complex:
        plt.scatter(np.real(symbols_complex[:1000]), np.imag(symbols_complex[:1000]), 
                   alpha=0.5, s=10, color='green')
    plt.axhline(0, color='k', linestyle='--', alpha=0.5)
    plt.axvline(0, color='k', linestyle='--', alpha=0.5)
    plt.grid(True, alpha=0.3)
    plt.xlabel('I')
    plt.ylabel('Q')
    plt.title('Созвездие после коррекции')
    plt.axis('equal')
    
    # S-кривая
    plt.subplot(1, 3, 3)
    plt.plot(offsets, ted_values, 'bo-', linewidth=2, markersize=8)
    plt.plot(min_offset, ted_values[min_index], 'ro', markersize=10, 
             label=f'Оптимальный (offset={min_offset})')
    plt.axhline(0, color='k', linestyle='--', alpha=0.5)
    plt.axvline(0, color='k', linestyle='--', alpha=0.5)
    plt.grid(True, alpha=0.3)
    plt.xlabel('Смещение (отсчёты)')
    plt.ylabel('Выход TED')
    plt.title('S-кривая детектора Гарднера')
    plt.legend()
    
    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    main()