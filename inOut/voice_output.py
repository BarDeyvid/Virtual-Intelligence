import asyncio
import numpy as np
import simpleaudio as sa
import logging
import gc
import torch
import json
from pathlib import Path
from enum import Enum
from typing import Iterable, List, Optional, Union

# Importa as partes necessárias do Piper para inferência direta
import onnxruntime
# Removida a importação de piper_train.vits.utils.audio_float_to_int16

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Definindo as constantes e enums do Piper para phonemization
PAD = "_"  # padding (0)
BOS = "^"  # beginning of sentence
EOS = "$"  # end of sentence

class PhonemeType(str, Enum):
    ESPEAK = "espeak"
    TEXT = "text"

# --- Funções auxiliares do Piper (adaptadas da sua documentação) ---
def load_config(model_path: Union[str, Path]):
    """Carrega o arquivo de configuração JSON do modelo ONNX."""
    config_path = Path(model_path).with_suffix(".onnx.json")
    if not config_path.exists():
        raise FileNotFoundError(f"Arquivo de configuração não encontrado: {config_path}")
    with open(config_path, "r", encoding="utf-8") as file:
        config = json.load(file)
    return config

def load_onnx(model_path: Union[str, Path], sess_options: onnxruntime.SessionOptions, providers: List[str]):
    """Carrega o modelo ONNX e sua configuração."""
    logging.debug("Loading model from %s", model_path)
    config = load_config(model_path)
    model = onnxruntime.InferenceSession(
        str(model_path),
        sess_options=sess_options,
        providers=providers
    )
    logging.info("Loaded model from %s", model_path)
    return model, config

def phonemize(config: dict, text: str) -> List[List[str]]:
    """Texto para fonemas agrupados por sentença."""
    if config["phoneme_type"] == PhonemeType.ESPEAK:
        if config["espeak"]["voice"] == "ar":
            # Arabic diacritization (function not implemented, skipping)
            # text = tashkeel_run(text)
            pass
        return phonemize_espeak(text, config["espeak"]["voice"])
    if config["phoneme_type"] == PhonemeType.TEXT:
        return phonemize_codepoints(text)
    raise ValueError(f'Unexpected phoneme type: {config["phoneme_type"]}')

def phonemize_codepoints(text: str) -> List[List[str]]:
    """
    Converts text into lists of codepoints (characters) grouped by sentences.
    """
    sentences = [s.strip() for s in text.replace("!", ". ").replace("?", ". ").split(". ") if s.strip()]
    return [[char for char in sentence] for sentence in sentences]

def phonemize_espeak(text: str, voice: str) -> List[List[str]]:
    """
    Dummy implementation for phonemize_espeak.
    Replace this with actual eSpeak phonemization logic if available.
    """
    # For demonstration, split text into words and treat each word as a phoneme.
    return [[word for word in text.split()]]

def phonemes_to_ids(config: dict, phonemes: List[str]) -> List[int]:
    """Fonemas para IDs."""
    id_map = config["phoneme_id_map"]
    ids: List[int] = list(id_map[BOS])
    for phoneme in phonemes:
        if phoneme not in id_map:
            logging.warning("Missing phoneme from id map: %s", phoneme)
            continue
        ids.extend(id_map[phoneme])
        ids.extend(id_map[PAD])
    ids.extend(id_map[EOS])
    return ids

# --- Fim das funções auxiliares do Piper ---

class JarvisTTSClient:
    def __init__(self):
        self.sample_rate = None
        self.audio_queue = asyncio.Queue()
        self.is_playing = False
        self.onnx_model = None # O modelo ONNX carregado
        self.model_config = None # A configuração do modelo ONNX
        
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        logging.info(f"Usando dispositivo: {self.device} para inferência Piper (se onnxruntime-gpu estiver instalado).")

        # Caminho para o diretório onde você baixou os modelos do Piper.
        # O log mostra 'Jarvis-High.onnx', então vou usar esse nome de arquivo.
        self.model_path = "./models/Jarvis-High.onnx" 

    async def _load_tts_model(self):
        """
        Carrega o modelo Piper TTS (ONNX) e sua configuração.
        """
        if self.onnx_model is None:
            logging.info(f"🔊 Carregando modelo Piper TTS de: {self.model_path}... Isso pode levar um tempo na primeira vez.")
            try:
                # Configurações para a sessão ONNX Runtime
                sess_options = onnxruntime.SessionOptions()
                providers = [
                    "CPUExecutionProvider"
                    if self.device == "cpu"
                    else ("CUDAExecutionProvider", {"cudnn_conv_algo_search": "DEFAULT"})
                ]
                
                self.onnx_model, self.model_config = load_onnx(self.model_path, sess_options, providers)
                self.sample_rate = self.model_config["audio"]["sample_rate"]
                logging.info(f"✅ Modelo Piper TTS carregado com sucesso. Taxa de amostragem: {self.sample_rate} Hz.")
            except Exception as e:
                logging.error(f"❌ Erro ao carregar o modelo Piper TTS: {e}")
                self.onnx_model = None
                self.model_config = None
                raise

    async def _generate_audio_stream(self, text):
        """
        Gera chunks de áudio usando o modelo Piper TTS (inferência ONNX) e os coloca na fila.
        """
        if self.onnx_model is None or self.model_config is None:
            logging.error("Modelo Piper TTS não carregado. Não é possível gerar áudio.")
            await self.audio_queue.put(None)
            return

        logging.info("🎤 Iniciando geração de áudio com Piper TTS...")
        
        # O Piper TTS gera o áudio para o texto completo de uma vez.
        # Para simular streaming, ainda dividimos o texto em sentenças e geramos cada uma.
        # Isso permite que a reprodução comece enquanto as próximas sentenças são processadas.
        sentences = [s.strip() for s in text.replace("!", ". ").replace("?", ". ").split(". ") if s.strip()]
        
        # Definir parâmetros de inferência (podem ser ajustados se o modelo permitir)
        length_scale = 1.0
        noise_scale = 0.667
        noise_scale_w = 0.8

        for i, sentence in enumerate(sentences):
            if not sentence:
                continue
            logging.info(f"🎤 Piper: Gerando áudio para sentença {i+1}/{len(sentences)}: '{sentence[:50]}...'")
            try:
                # 1. Phonemize o texto
                # A função phonemize espera o config, mas o tipo de phoneme pode ser uma string literal.
                # Ajustamos o config temporariamente se necessário.
                current_phoneme_type = self.model_config["phoneme_type"]
                if isinstance(current_phoneme_type, PhonemeType):
                    # Se já for um enum, use-o diretamente
                    pass
                elif current_phoneme_type == "espeak":
                    self.model_config["phoneme_type"] = PhonemeType.ESPEAK
                elif current_phoneme_type == "text":
                    self.model_config["phoneme_type"] = PhonemeType.TEXT
                else:
                    logging.warning(f"Tipo de fonema desconhecido no config: {current_phoneme_type}. Usando 'text'.")
                    self.model_config["phoneme_type"] = PhonemeType.TEXT # Fallback

                phonemes_list_of_lists = phonemize(self.model_config, sentence)
                
                # Se houver múltiplos fonemas (sentenças divididas internamente pelo phonemizer),
                # vamos processar cada uma e concatenar o áudio.
                full_audio_for_sentence = []

                for phonemes in phonemes_list_of_lists:
                    # 2. Converter fonemas para IDs
                    phoneme_ids = phonemes_to_ids(self.model_config, phonemes)
                    
                    # 3. Preparar inputs para o modelo ONNX
                    text_input = np.expand_dims(np.array(phoneme_ids, dtype=np.int64), 0)
                    text_lengths = np.array([text_input.shape[1]], dtype=np.int64)
                    scales = np.array(
                        [noise_scale, length_scale, noise_scale_w],
                        dtype=np.float32,
                    )
                    
                    # Speaker ID (se o modelo tiver múltiplos speakers)
                    # O modelo Jarvis-High provavelmente não tem múltiplos speakers,
                    # mas se tivesse, você precisaria de um speaker_id aqui.
                    # Por enquanto, mantemos como None para modelos de um único speaker.
                    speaker_id_input = None
                    if self.model_config["num_speakers"] > 1:
                        # Se o modelo tiver múltiplos speakers, você precisaria definir um sid.
                        # Por exemplo, sid = np.array([0], dtype=np.int64) para o primeiro speaker.
                        # Para Jarvis-High, assumimos num_speakers = 1.
                        pass # Não faz nada, speaker_id_input permanece None

                    # 4. Executar inferência ONNX
                    # Os inputs para model.run() dependem do modelo.
                    # Para modelos de um único speaker, 'sid' pode não ser necessário.
                    # Verificamos se 'sid' é um input esperado pelo modelo.
                    input_names = [i.name for i in self.onnx_model.get_inputs()]
                    run_inputs = {
                        "input": text_input,
                        "input_lengths": text_lengths,
                        "scales": scales,
                    }
                    if "sid" in input_names and speaker_id_input is not None:
                        run_inputs["sid"] = speaker_id_input

                    raw_audio_output = self.onnx_model.run(None, run_inputs)[0]
                    
                    # Força o array a ser 1D para evitar problemas de concatenação
                    audio_output = raw_audio_output.flatten() 
                    
                    # 5. Converter áudio de float para int16 manualmente
                    # Escala o áudio para o range de int16 e converte o tipo
                    # O valor 32767 é o máximo para um int16 assinado (2^15 - 1)
                    audio_int16 = (audio_output * 32767).astype(np.int16)
                    full_audio_for_sentence.append(audio_int16)

                if full_audio_for_sentence:
                    # Concatena todos os segmentos de áudio para a sentença
                    final_audio_array = np.concatenate(full_audio_for_sentence)
                else:
                    final_audio_array = np.array([], dtype=np.int16) # Caso a sentença não gere áudio

                logging.info(f"🎤 Piper: Chunk {i+1} gerado (tamanho: {len(final_audio_array)} samples). Colocando na fila. Fila: {self.audio_queue.qsize()}")
                await self.audio_queue.put(final_audio_array)
                
                self._cleanup_memory()
                
            except Exception as e:
                logging.error(f"❌ Erro ao gerar áudio com Piper para a sentença '{sentence[:50]}...': {e}")
        
        logging.info("🎤 Piper: Todos os chunks gerados. Sinalizando fim do stream.")
        await self.audio_queue.put(None)

    async def _play_audio_from_queue(self):
        """
        Tarefa assíncrona que consome e reproduz chunks de áudio da fila.
        """
        self.is_playing = True
        logging.info("🎧 Player: Iniciando reprodução da fila.")
        while True:
            audio_chunk = await self.audio_queue.get()
            if audio_chunk is None:
                logging.info("🎧 Player: Sinal de término recebido. Encerrando reprodução.")
                break
            
            try:
                if self.sample_rate is None:
                    logging.error("Taxa de amostragem não definida. Não é possível reproduzir.")
                    continue

                logging.info(f"🎧 Player: Reproduzindo chunk de {len(audio_chunk)} samples. Fila restante: {self.audio_queue.qsize()}")
                play_obj = sa.play_buffer(
                    audio_chunk,
                    1, # channels (mono)
                    2, # bytes per sample (16-bit)
                    self.sample_rate
                )
                play_obj.wait_done()
            except Exception as e:
                logging.error(f"❌ Erro ao reproduzir chunk de áudio: {e}")
            finally:
                self.audio_queue.task_done()
        self.is_playing = False
        logging.info("🎧 Player: Reprodução concluída.")

    def _cleanup_memory(self):
        """
        Libera a memória da GPU e do Python. Importante para aplicações de longa duração.
        """
        gc.collect()
        if torch.cuda.is_available():
            torch.cuda.empty_cache()

    async def speak_streamed(self, text):
        """
        Inicia o processo de streaming e reprodução.
        """
        await self._load_tts_model()

        generator_task = asyncio.create_task(self._generate_audio_stream(text))
        player_task = asyncio.create_task(self._play_audio_from_queue())

        await asyncio.gather(generator_task, player_task)
        logging.info("Streaming TTS com Piper concluído.")

# Exemplo de uso
async def main():
    client = JarvisTTSClient()
    
    long_text = ("""
        Hello! I am Jarvis, your virtual assistant. I am very happy to help you today.
        This is an example of how my voice sounds using the Piper TTS model.
        I hope you enjoy the quality and naturalness.
        I am always learning and improving to offer the best possible experience.
        If you need anything, just call me!
    """
    )
    await client.speak_streamed(long_text)

if __name__ == "__main__":
    asyncio.run(main())
