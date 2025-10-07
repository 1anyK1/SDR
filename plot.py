import numpy as np
import math
import matplotlib.pyplot as plt

# Открываем файл для чтения
name = "txdata.pcm"

data = []
imag = []
real = []
count = []
counter = 0
absIQ = []
with open(name, "rb") as f:
    index = 0
    while (byte := f.read(2)):
        I = 0
        Q = 0
        if(index %2 == 0):
            Q = int.from_bytes(byte, byteorder='little', signed=True)
            real.append(Q)
            counter += 1
            count.append(counter)
        else:
            I = int.from_bytes(byte, byteorder='little', signed=True)
            imag.append(I)
        
        index += 1
    for i in range(len(imag)):
        abs = math.sqrt(imag[i]**2 + real[i]**2)
        absIQ.append(abs)
        
# Инициализируем список для хранения данных

# fig, axs = plt.subplots(2, 1, layout='constrained')
plt.figure(1)
# axs\[1\].plot(count, np.abs(data),  color='grey')  # Используем scatter для диаграммы созвездия
plt.plot(count,(imag),color='red')  # Используем scatter для диаграммы созвездия
plt.plot(count,(real), color='blue')  # Используем scatter для диаграммы созвездия
plt.show()

plt.figure(2)
# axs\[1\].plot(count, np.abs(data),  color='grey')  # Используем scatter для диаграммы созвездия
plt.plot(count,(absIQ),color='purple')  # Используем scatter для диаграммы созвездия
plt.show()