import numpy as np
import librosa
from pydub import AudioSegment

pcm_load = np.fromfile("../pcm/txdata.pcm", dtype=np.int16)

audio = AudioSegment(
    data=pcm_load.tobytes(),
    sample_width=4,      # 4 байта = 32 бит
    frame_rate=44100,    # частота дискретизации
    channels=1           # моно
)

audio.export("../mp3/audio_new.mp3", format="mp3", bitrate="192k")