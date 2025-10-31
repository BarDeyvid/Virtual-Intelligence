import sounddevice as sd
import numpy as np
import torch
import torchaudio
import logging
import os
from silero_vad import load_silero_vad
from resemblyzer import VoiceEncoder, preprocess_wav
from scipy.spatial.distance import cosine
import time
import speechbrain as sb
from speechbrain.inference.interfaces import foreign_class

class EmotionRecognizer:
    def __init__(self, device="cpu"):
        self.device = device
        self.classifier = foreign_class(source="speechbrain/emotion-recognition-wav2vec2-IEMOCAP", pymodule_file="custom_interface.py", classname="CustomEncoderWav2vec2Classifier")
        
    def predict_emotion(self, wav, sample_rate=16000):
        if not torch.is_tensor(wav):
            wav = torch.tensor(wav)

        if len(wav.shape) == 1:
            wav = wav.unsqueeze(0)

        if sample_rate != 16000:
            resampler = torchaudio.transforms.Resample(orig_freq=sample_rate, new_freq=16000)
            wav = resampler(wav)

        with torch.no_grad():
            out_prob, score, index, text_lab = self.classifier.classify_batch(wav)
            predicted_label = text_lab


        return predicted_label

class SpeakerIdentifier:
    def __init__(self, sample_rate=16000, frame_size=512, speech_threshold=0.6, silence_threshold=0.3, profiles_path="voice_profiles"):
        self.sample_rate = sample_rate
        self.frame_size = frame_size
        self.speech_threshold = speech_threshold
        self.silence_threshold = silence_threshold
        self.profiles_path = profiles_path

        self.vad_model = load_silero_vad()
        self.encoder = VoiceEncoder(device="cpu", verbose=False)
        self.emotion_recognizer = EmotionRecognizer(device="cpu")


        self.embeddings_db = {}
        self.audio_buffer = []
        self.vad_state = False
        self.last_speaker = None
        self.hook_segment = None  # Callback hook

        self.load_embeddings()

    def load_embeddings(self):
        self.embeddings_db = {}
        if os.path.exists(self.profiles_path):
            for fname in os.listdir(self.profiles_path):
                if fname.endswith(".npy"):
                    name = fname.replace(".npy", "")
                    emb = np.load(os.path.join(self.profiles_path, fname))
                    self.embeddings_db[name] = emb
            logging.info(f"🔍 Perfis carregados: {list(self.embeddings_db.keys())}")
        else:
            logging.warning(f"⚠️ Diretório de perfis '{self.profiles_path}' não encontrado.")

    def compare_embedding(self, emb):
        best_score = -1
        best_name = "Desconhecido"
        for name, ref_emb in self.embeddings_db.items():
            score = 1 - cosine(emb, ref_emb)
            if score > best_score:
                best_score = score
                best_name = name
        return best_name, best_score

    def process_segment(self, segment_audio):
        # Verificação inicial de comprimento
        seg_duration = len(segment_audio) / self.sample_rate
        if seg_duration < 0.5:
            logging.info(f"⚠️ Segmento muito curto ({seg_duration:.2f}s), ignorando.")
            if self.hook_segment is not None:
                self.hook_segment(segment_audio, None)
            return

        try:
            wav = preprocess_wav(segment_audio)

            # Verificar comprimento após pré-processamento
            if len(wav) < 8000:  # 0.5s a 16kHz
                logging.warning(f"⚠️ Áudio muito curto após pré-processamento ({len(wav)/self.sample_rate:.2f}s).")
                if self.hook_segment is not None:
                    self.hook_segment(segment_audio, None)
                return

            # Normalizar para evitar problemas de escala
            if np.max(np.abs(wav)) > 1.0:
                wav = wav / np.max(np.abs(wav))

            # Verificar NaN/Inf
            if np.any(np.isnan(wav)) or np.any(np.isinf(wav)):
                logging.error("❌ Áudio inválido (NaN/inf).")
                if self.hook_segment is not None:
                    self.hook_segment(segment_audio, None)
                return

            emb = self.encoder.embed_utterance(wav)

            name, score = self.compare_embedding(emb)

            # Emotion
            emotion = self.emotion_recognizer.predict_emotion(wav)

            self.last_speaker = {"name": name, "score": score, "emotion": emotion}
            logging.info(f"🗣️ Provável falante: {name} (score={score:.2f} [{emotion}])")

            # Chamar o hook com os dados do falante
            if self.hook_segment is not None:
                self.hook_segment(segment_audio, {"name": name, "score": score, "emotion": emotion})

        except Exception as e:
            logging.error(f"❌ Erro no processamento: {str(e)}")
            # Chamar o hook mesmo em caso de erro
            if self.hook_segment is not None:
                self.hook_segment(segment_audio, None)

    def audio_callback(self, indata, frames, time_info, status):
        audio_np = indata[:, 0]
        audio_tensor = torch.from_numpy(audio_np).float()

        speech_prob = self.vad_model(audio_tensor, sr=self.sample_rate)

        if speech_prob > self.speech_threshold and not self.vad_state:
            logging.info(f"🟢 Fala detectada!")
            self.vad_state = True
            self.audio_buffer = []  # Começa novo buffer

        if self.vad_state:
            self.audio_buffer.append(audio_np.copy())

            # Limitar o buffer a 120 blocos (cerca de 6 segundos)
            if len(self.audio_buffer) > 120:
                logging.info("🔴 Corte por buffer cheio")
                self.vad_state = False
                segment_audio = np.concatenate(self.audio_buffer)
                self.process_segment(segment_audio)

        if speech_prob < self.silence_threshold and self.vad_state:
            logging.info(f"🔴 Fim da fala.")
            self.vad_state = False
            if self.audio_buffer:
                segment_audio = np.concatenate(self.audio_buffer)
                self.process_segment(segment_audio)

    def start_stream(self):
        self.stream = sd.InputStream(callback=self.audio_callback, samplerate=self.sample_rate, channels=1, blocksize=self.frame_size)
        self.stream.start()
        logging.info("👂 Iniciando reconhecimento de falante...")

    def stop_stream(self):
        if hasattr(self, 'stream'):
            self.stream.stop()
            self.stream.close()
            logging.info("🛑 Encerrando reconhecimento de falante.")

    def get_last_speaker(self):
        speaker = self.last_speaker
        self.last_speaker = None
        return speaker

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    speaker_id = SpeakerIdentifier()
    try:
        speaker_id.start_stream()
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        speaker_id.stop_stream()