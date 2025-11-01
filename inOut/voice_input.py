import concurrent.futures
import queue
import threading
import os
import logging
import time
import numpy as np
import torch
from multiprocessing import Process, Queue
from threading import Thread
import torch.multiprocessing as mp

# Configurar método de inicialização (Windows compatível)
#try:
#    mp.set_start_method('spawn', force=True)
#except RuntimeError:
#    pass

from transformers import pipeline
from transformers.utils import is_flash_attn_2_available
logging.basicConfig(level=logging.ERROR, format='%(asctime)s - %(levelname)s - %(message)s')

def whisper_worker(input_queue: Queue, output_queue: Queue, model_name="openai/whisper-large-v3-turbo"):
    device = "cuda" if torch.cuda.is_available() else "cpu"
    torch_dtype = torch.float16 if torch.cuda.is_available() else torch.float32

    # pipeline configurado para baixa latência (streaming curto)
    pipe = pipeline(
        "automatic-speech-recognition",
        model=model_name,
        torch_dtype=torch_dtype,
        device=device,
        chunk_length_s=2.5,
        stride_length_s=[0.5, 0.5],
        generate_kwargs = {
            "num_beams": 1,
            "condition_on_prev_tokens": False,
            "compression_ratio_threshold": 1.35,  # zlib compression ratio threshold (in token space)
            "temperature": (0.0, 0.2, 0.4, 0.6, 0.8, 1.0),
            "logprob_threshold": -1.0,
            "no_speech_threshold": 0.6,
            "return_timestamps": True,
        },
        model_kwargs={
            "attn_implementation": "sdpa"
        } if is_flash_attn_2_available() else {"attn_implementation": "sdpa"},
    )
    os_name_clear = 'clear' if os.name == 'posix' else 'cls'
    try:
        import os as _os
        _os.system(os_name_clear)
    except Exception:
        pass
    print(f"🧠 Whisper pipeline carregado: {model_name}")

    while True:
        item = input_queue.get()
        if item is None:
            print("🛑 Whisper worker encerrando.")
            break

        audio_data = item["audio"]
        speaker = item.get("speaker", {"name": "Desconhecido", "score": 0.0})

        # Use a chamada simples: deixe o pipeline aplicar seu chunking configurado
        result = pipe(
            {"raw": audio_data, "sampling_rate": 16000},
            #return_timestamps=False,
            #generate_kwargs={"language": "pt"}
            return_timestamps=False
        )

        text = result.get("text", "").strip()
        output_queue.put({
            "speaker": speaker.get("name", "Desconhecido"),
            "score": speaker.get("score", 0.0),
            "text": text,
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
        try:
            self.whisper_input_queue.put(None)
            self.whisper_process.join(timeout=2)
            if self.whisper_process.is_alive():
                self.whisper_process.terminate()
        except Exception:
            pass

        try:
            self.speaker_id.stop_stream()
        except Exception:
            pass

    def _handle_speaker_segment(self, segment_audio, speaker_data):
        """Hook chamado quando SpeakerIdentifier termina uma fala."""
        if speaker_data is None:
            speaker_data = {"name": "Proxy", "score": 1.0}
            logging.warning("⚠️ Segmento recebido sem dados de falante, ignorando.")
            #return


        try:
            self.segment_buffer.append(segment_audio)
            total_length_sec = sum(len(seg) for seg in self.segment_buffer) / self.speaker_id.sample_rate

            if total_length_sec >= self.segment_min_length:
                full_segment = np.concatenate(self.segment_buffer)
                # Limpa o buffer corretamente para evitar vazamento entre transcrições
                self.segment_buffer.clear()

                wav = full_segment.astype(np.float32)

                self.whisper_input_queue.put({
                    "audio": wav,
                    "speaker": speaker_data or {"name": "Proxy", "score": 1.0}
                })
                logging.info(f"🔗 Segmento enviado pro Whisper ({total_length_sec:.2f}s)")


        except Exception as e:
            logging.error(f"❌ Erro ao pré-processar segmento: {e}")

    def get_last_result(self):
        try:
            return self.queue_results.get_nowait()
        except queue.Empty:
            return None

if __name__ == "__main__":
    mp.set_start_method('spawn', force=True)
    pipeline = VoicePipeline()
    pipeline.start()
    try:
        while True:
            result = pipeline.get_last_result()
            if result:
                print(f"🗣️ {result['speaker']} ({result['score']:.2f}): {result['text']}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        pipeline.stop()
