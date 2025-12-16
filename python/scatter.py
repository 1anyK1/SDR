import numpy as np
import math
import matplotlib.pyplot as plt

name = "../pcm/rxdata.pcm"

start_point = 3 
data = []
imag = []
real = []
count = []
counter = 0

temp_real = []
temp_imag = []

with open(name, "rb") as f:
    index = 0
    while (byte := f.read(2)):
        if (index - start_point) % 10 == 0:
            if len(temp_real) == len(temp_imag):
                I = int.from_bytes(byte, byteorder='little', signed=True)
                temp_real.append(I)
            else:
                Q = int.from_bytes(byte, byteorder='little', signed=True)
                temp_imag.append(Q)
                counter += 1
                count.append(counter)
        
        index += 1

for i in range(min(len(temp_real), len(temp_imag))):
    if abs(temp_real[i]) > 100 and abs(temp_imag[i]) > 100:
        real.append(temp_real[i])
        imag.append(temp_imag[i])


print(len(real))
plt.figure(figsize=(8, 8))
plt.scatter(real, imag, alpha=0.5, s=10)  
plt.grid(True)
plt.show()