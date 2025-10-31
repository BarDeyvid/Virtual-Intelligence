import concurrent.futures
import queue
import threading
import logging
import time
import numpy as np
import torch
from resemblyzer import preprocess_wav
import os
#try:
#    from inOut.whosTalking import SpeakerIdentifier
#except ModuleNotFoundError:
#    from whosTalking import SpeakerIdentifier
from multiprocessing import Process, Queue
from threading import Thread
import multiprocessing
import torch.multiprocessing as mp

# Configurar método de inicialização
mp.set_start_method('spawn', force=True)

# Pipelines do Transformers
from transformers import pipeline
from transformers.utils import is_flash_attn_2_available
logging.basicConfig(level=logging.ERROR, format='%(asctime)s - %(levelname)s - %(message)s')

def whisper_worker(input_queue: Queue, output_queue: Queue, model_name="openai/whisper-large-v3"):
    device = "cuda" if torch.cuda.is_available() else "cpu"
    torch_dtype = torch.float16 if torch.cuda.is_available() else torch.float32

    pipe = pipeline(
        "automatic-speech-recognition",
        model=model_name,
        torch_dtype=torch_dtype,
        device=device,
        chunk_length_s=2.5,
        stride_length_s=[0.5, 0.5],
        model_kwargs={
            "attn_implementation": "sdpa"
        } if is_flash_attn_2_available() else {"attn_implementation": "sdpa"},
    )
    os.system('clear' if os.name == 'posix' else 'cls')
    print(f"🧠 Whisper pipeline carregado: {model_name}")

    while True:
        item = input_queue.get()
        if item is None:
            print("🛑 Whisper worker encerrando.")
            break

        audio_data = item["audio"]
        speaker = item["speaker"]

        result = pipe(
            {"raw": audio_data, "sampling_rate": 16000},
            chunk_length_s=40, ## 30 segundos originais, testei 40
            batch_size=8,
            return_timestamps=True,
            generate_kwargs={"language": "pt"}
        )

        output_queue.put({
            "speaker": speaker.get("name", "Desconhecido"),
            "score": speaker.get("score", 0.0),
            "text": result['text'].strip(),
            "chunks": result.get("chunks", [])
        })


class VoicePipeline:
    def __init__(self, model_name="openai/whisper-large-v3"):
        try:
            from inOut.whosTalking import SpeakerIdentifier
        except ModuleNotFoundError:
            from whosTalking import SpeakerIdentifier
        self.speaker_id = SpeakerIdentifier()
        self.queue_segments = queue.Queue()
        self.queue_results = queue.Queue()
        self.segment_min_length = 0.1  # segundos mínimos
        self.segment_buffer = []

        # Filas para comunicação com o worker do Whisper
        self.whisper_input_queue = Queue()
        self.whisper_output_queue = Queue()

        # Iniciar o worker dedicado ao Whisper
        self.whisper_process = Process(
            target=whisper_worker,
            args=(self.whisper_input_queue, self.whisper_output_queue),
            daemon=True
        )
        self.whisper_process.start()

        # Thread para consumir resultados do worker
        self.running = True
        Thread(target=self._consume_whisper_results, daemon=True).start()

        # Configurar o hook do SpeakerIdentifier
        self.speaker_id.hook_segment = self._handle_speaker_segment

    def _consume_whisper_results(self):
        while self.running:
            try:
                result = self.whisper_output_queue.get(timeout=0.5)
                self.queue_results.put(result)
            except queue.Empty:
                continue

    def start(self):
        self.running = True
        self.speaker_id.start_stream()
        logging.info("🚀 VoicePipeline iniciada")

    def stop(self):
        self.running = False

        # Encerrar o worker do Whisper
        self.whisper_input_queue.put(None)
        self.whisper_process.join(timeout=2)
        if self.whisper_process.is_alive():
            self.whisper_process.terminate()

        self.speaker_id.stop_stream()

    def _handle_speaker_segment(self, segment_audio, speaker_data):
        """Hook chamado quando SpeakerIdentifier termina uma fala."""
        if speaker_data is None:
            logging.warning("⚠️ Segmento recebido sem dados de falante, ignorando.")
            return

        try:
            self.segment_buffer.append(segment_audio)
            total_length_sec = sum(len(seg) for seg in self.segment_buffer) / self.speaker_id.sample_rate

            if total_length_sec >= self.segment_min_length:
                full_segment = np.concatenate(self.segment_buffer)
                self.segment_buffer = []

                # Pré-processar o áudio para entrada do Whisper
                #wav = preprocess_wav(full_segment, source_sr=self.speaker_id.sample_rate
                wav = segment_audio.astype(np.float32)

                # Enviar pro worker
                self.whisper_input_queue.put({
                    #"audio": wav.astype(np.float32),
                    "audio": wav,
                    "speaker": speaker_data or {"name": "Desconhecido", "score": 0.0}
                })
                logging.info(f"🔗 Segmento enviado pro Whisper ({total_length_sec:.2f}s)")
            else:
                logging.info(f"⏳ Acumulando... ({total_length_sec:.2f}s)")

        except Exception as e:
            logging.error(f"❌ Erro ao pré-processar segmento: {e}")
    
    def get_last_result(self):
        try:
            return self.queue_results.get_nowait()
        except queue.Empty:
            return None

if __name__ == "__main__":
    pipeline = VoicePipeline()
    pipeline.start()
    try:
        while True:
            result = pipeline.get_last_result()
            if result:
                print(f"🗣️ {result['speaker']} ({result['score']:.2f}): {result['text']}")
                # Para ver os timestamps:
                # print(result.get("chunks"))
            time.sleep(0.01)
    except KeyboardInterrupt:
        pipeline.stop()