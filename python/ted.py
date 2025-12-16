import numpy as np
import matplotlib.pyplot as plt
import sys

def gardner_ted(I, Q, n, Nsps, shift):
    shift_int = int(shift)
    r0 = I[n + shift_int]
    r1 = I[n + shift_int + Nsps//2]
    r2 = I[n + shift_int + Nsps]
    
    i0 = Q[n + shift_int]
    i1 = Q[n + shift_int + Nsps//2]
    i2 = Q[n + shift_int + Nsps]
    
    e = (r2 - r0) * r1 + (i2 - i0) * i1
    return e

def main():
    
    filename = "../pcm/rxdata.pcm"
    
    rx = np.fromfile(filename, dtype=np.int16)
    real = rx[0::2]
    imag = rx[1::2]
    
    start = 5000
    end = 20000
    if len(real) > end:
        real = real[start:end]
        imag = imag[start:end]
    
    Nsps = 10
    BnTs = 0.01
    Kp = 1.0
    zeta = 0.707
    
    theta = (BnTs / Nsps) / (zeta + 1/(4*zeta))
    K1 = (-4*zeta*theta) / ((1 + 2*zeta*theta + theta**2) * Kp)
    K2 = (-4*theta**2) / ((1 + 2*zeta*theta + theta**2) * Kp)
    
    print(f"K1 = {K1:.6f}")
    print(f"K2 = {K2:.6f}")
    print(f"theta = {theta:.6f}")
    print(f"BnTs = {BnTs}, Nsps = {Nsps}, Kp = {Kp}, zeta = {zeta:.6f}\n")
    
    h = np.ones(Nsps)
    conv_real = np.convolve(real, h, mode='same')
    conv_imag = np.convolve(imag, h, mode='same')
    
    print("Анализ зависимости TED от offset:")
    offsets = np.arange(-Nsps//2, Nsps//2 + 1)
    ted_values = []
    
    n = Nsps * 480
    
    for offset in offsets:
        e = gardner_ted(conv_real, conv_imag, n, Nsps, offset)
        ted_values.append(e)
        print(f"  offset={offset:3d}, e={e:10.8f}")
    
    min_index = np.argmin(np.abs(ted_values))
    min_offset = offsets[min_index]
    
    print(f"\nОптимальный offset = {min_offset}")
    print(f"Значение TED в оптимальной точке = {ted_values[min_index]:.8f}\n")
    
    p1_accum = 0
    p2_accum = 0
    
    num_symbols = 4800
    symbol_indices = []
    shift_history = []
    e_values = []
    
    current_index = min_offset
    
    for k in range(num_symbols):
        if current_index + Nsps >= len(conv_real):
            break
        
        e = gardner_ted(conv_real, conv_imag, current_index, Nsps, 0)
        
        e_values.append(e)
        
        p1 = e * K1
        p1_accum += p1
        
        p2 = p2_accum + p1 + e * K2
        p2_accum = p2
        
        while p2_accum > 1.0:
            p2_accum -= 2.0
        while p2_accum < -1.0:
            p2_accum += 2.0
        
        shift = int(np.round(p2_accum * Nsps))
        shift_history.append(shift)
        
        symbol_index = current_index + shift
        symbol_indices.append(symbol_index)
        
        if k < 4800:
            print(f"  Символ {k}: e={e:10.8f}, p2={p2_accum:8.6f}, shift={shift}, index={symbol_index}")
        
        current_index = symbol_index + Nsps
    
    print(f"\nНайдено {len(symbol_indices)} символов")
    
    symbols_I = []
    symbols_Q = []
    for i, idx in enumerate(symbol_indices):
        if idx < len(conv_real):
            I_val = conv_real[idx]
            Q_val = conv_imag[idx]
            symbols_I.append(I_val)
            symbols_Q.append(Q_val)

    for i in range(min(1920, len(symbols_I))):
        print(f"  Символ {i}: I={symbols_I[i]:.6f}, Q={symbols_Q[i]:.6f}")
    
    plt.figure(figsize=(15, 10))
    
    plt.plot(offsets, ted_values, 'bo-', linewidth=2, markersize=8)
    plt.plot(min_offset, ted_values[min_index], 'ro', markersize=10, label=f'Оптимальный (offset={min_offset})')
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.axvline(x=0, color='k', linestyle='-', alpha=0.3)
    plt.grid(True, alpha=0.3)
    plt.xlabel('Offset (смещение в отсчетах)')
    plt.ylabel('Выход TED (e)')
    plt.title('S-кривая детектора временной ошибки Гарднера')
    plt.legend()
    
    plt.figure(figsize=(15, 10))
    if e_values:
        plt.plot(range(len(e_values)), e_values, 'g-', linewidth=2, marker='o', markersize=4)
        plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
        plt.grid(True, alpha=0.3)
        plt.xlabel('Номер символа')
        plt.ylabel('Ошибка TED')
        plt.title('Ошибка TED во времени')
    
    plt.figure(figsize=(15, 10))
    if shift_history:
        plt.plot(range(len(shift_history)), shift_history, 'r-', linewidth=2, marker='s', markersize=4)
        plt.grid(True, alpha=0.3)
        plt.xlabel('Номер символа')
        plt.ylabel('Смещение (отсчеты)')
        plt.title('Смещение для каждого символа')
    
    plt.figure(figsize=(15, 10))
    if symbols_I:
        plt.scatter(symbols_I, symbols_Q, c='b', s=30, alpha=0.7)
        plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
        plt.axvline(x=0, color='k', linestyle='-', alpha=0.3)
        plt.grid(True, alpha=0.3)
        plt.xlabel('I компонента')
        plt.ylabel('Q компонента')
        plt.title(f'Созвездие ({len(symbols_I)} символов)')

    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    main()  