import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile as wav
from scipy.fft import rfft, rfftfreq

# read wav file
two_files = False
if len(sys.argv)>1:
    sample_rate, data = wav.read(sys.argv[1])

else:
    sample_rate, data = wav.read('/media/3735-6230/MIC000.WAV')

if len(sys.argv)>3:
    sample_rate2, data2 = wav.read(sys.argv[2])
    sample_rate3, data3 = wav.read(sys.argv[3])
    two_files = True

# FFT
N = len(data)
f = rfftfreq(N, 1.0/sample_rate)
F = np.abs(rfft(data))
Fscaled = F /np.max(F)

if two_files:
    N2 = len(data2)
    f2 = rfftfreq(N2, 1.0/sample_rate2)
    F2 = np.abs(rfft(data2))
    F2scaled = F2 / np.max(F2)

    N3 = len(data3)
    f3 = rfftfreq(N3, 1.0/sample_rate3)
    F3 = np.abs(rfft(data3))
    F3scaled = F3 / np.max(F3)

# plot audio frequency domain
plt.plot(f, Fscaled, color='b', label='dry samples')

if two_files:
#    plt.plot(f2, F2scaled, color='g', label='semi-wet samples') 
    plt.plot(f3, F2scaled, color='r', label='wet samples') 

plt.xlabel('frequency [Hz]')
plt.ylabel('relative amplitude (to max in FFT)')

# display the plots
plt.legend()
plt.grid()
plt.show()
