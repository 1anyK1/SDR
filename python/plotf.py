import numpy as np
import matplotlib.pyplot as plt

name = "../pcm/final.pcm"

data = np.fromfile(name, dtype=np.int16)

count = np.arange(len(data))

plt.figure(figsize=(12, 6))
plt.plot(count, data, color='blue', linewidth=0.5)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title('PCM Data Values')
plt.grid(True, alpha=0.3)
plt.show()