from resemblyzer import VoiceEncoder, preprocess_wav
import numpy as np
import sounddevice as sd

sample_rate = 16000
duration = 30  # segundos

print("Gravando amostra... fale normalmente")
audio = sd.rec(int(duration * sample_rate), samplerate=sample_rate, channels=1)
sd.wait()

wav = preprocess_wav(audio[:,0], source_sr=sample_rate)

encoder = VoiceEncoder()
emb = encoder.embed_utterance(wav)

np.save("voice_profiles/deyvid.npy", emb)

print("Perfil de voz salvo como deyvid.npy")
