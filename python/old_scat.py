import numpy as np
import math
import matplotlib.pyplot as plt

name = "../pcm/rxdata.pcm"

data = []
imag = []
real = []
count = []
counter = 0
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

plt.figure(figsize=(8, 8))
plt.scatter(real, imag, alpha=0.5, s=10)  
plt.title("IQ Constellation Diagram")
plt.xlabel("In-phase (I)")
plt.ylabel("Quadrature (Q)")
plt.grid(True)
plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
plt.axvline(x=0, color='k', linestyle='-', alpha=0.3)
plt.axis('equal')  
plt.show()