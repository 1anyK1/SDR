import numpy as np
import math
import matplotlib.pyplot as plt

name = "../pcm/rxdata.pcm"


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
        

plt.figure(1)
plt.plot(count,(imag),color='red')
plt.plot(count,(real), color='blue')  
plt.show()

plt.figure(2)
plt.plot(count,(absIQ),color='purple')  
plt.show()

name2 = np.convolve(real, np.ones(10))

plt.figure(3)
plt.plot(name2)
plt.show()
