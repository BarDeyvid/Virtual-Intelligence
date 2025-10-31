import asyncio
import numpy as np
import simpleaudio as sa
import logging
import requests
import io # Para lidar com dados de áudio em memória
from pydub import AudioSegment # Para converter MP3 para PCM
AudioSegment.converter = r"C:\\ffmpeg\\bin\\ffmpeg.exe"
import gc # Para limpeza de memória
import aiohttp # Importado para requisições assíncronas
import re # Importado para expressões regulares

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

class StreamingTTSClient:
    def __init__(self, voice_id: str = None):
        self.elevenlabs_api_key = "sk_8db772bd93ffc8f13f0435f1771aed52d7dd04bcc34f8c81"  # <<< SUBSTITUA PELA SUA CHAVE DE API DO ELEVENLABS
        self.elevenlabs_base_url = "https://api.elevenlabs.io/v1/text-to-speech"
        
        # ID de uma voz feminina e jovem em inglês.
        # Você pode encontrar outras vozes e seus IDs no seu painel do ElevenLabs.
        # Ex: Bella (EXAVITQu4vr4xnSDxMaL), Rachel (21m00TzcC4bSptRebO3p), etc.
        self.voice_id = voice_id or "T3ZeSw265kJ0jRIeLTFw"  # Voz "Bella" - Feminina e Jovem
        if not self.voice_id:
            logging.error("❌ ID da voz não configurado. Por favor, adicione um ID de voz válido.")
            raise ValueError("ID da voz ausente.")

        # A taxa de amostragem padrão para a maioria das vozes do ElevenLabs é 22050 Hz
        self.sample_rate = 22050 
        
        self.audio_queue = asyncio.Queue()
        self.is_playing = False

    async def _load_tts_model(self):
        """
        Para ElevenLabs, esta função é mais um placeholder para verificar a API Key.
        Os modelos são gerenciados pela API do ElevenLabs.
        """
        if not self.elevenlabs_api_key or self.elevenlabs_api_key == "SUA_API_KEY_AQUI":
            logging.error("❌ Chave de API do ElevenLabs não configurada. Por favor, adicione sua chave.")
            raise ValueError("Chave de API do ElevenLabs ausente.")
        logging.info("✅ Configuração do ElevenLabs pronta.")

    def _clean_text_for_tts(self, text: str) -> str:
        """
        Remove todo o conteúdo que está dentro de parênteses, incluindo os parênteses.
        Também remove espaços múltiplos.
        Ex: "Texto normal (ação da personagem) mais texto." -> "Texto normal mais texto."
        """
        # Expressão regular para encontrar qualquer coisa entre parênteses.
        # r'\([^)]*\)' significa:
        #   \(   -> um parêntese de abertura literal
        #   [^)]* -> qualquer caractere que NÃO seja um parêntese de fechamento, zero ou mais vezes
        #   \)   -> um parêntese de fechamento literal
        cleaned_text = re.sub(r'\([^)]*\)', '', text)
        
        # Remover espaços múltiplos que podem surgir após a remoção dos parênteses
        # Ex: "Texto (ação) aqui" pode virar "Texto  aqui"
        cleaned_text = re.sub(r'\s+', ' ', cleaned_text).strip()
        
        return cleaned_text

    async def _generate_audio_stream(self, text):
        """
        Gera o áudio completo usando a API do ElevenLabs e o coloca na fila.
        Não há mais streaming de chunks MP3, o arquivo é baixado integralmente.
        """
        if not self.elevenlabs_api_key:
            logging.error("Chave de API do ElevenLabs não disponível. Não é possível gerar áudio.")
            await self.audio_queue.put(None)
            return

        # --- NOVA ETAPA DE LIMPEZA DE TEXTO ---
        original_text = text
        text = self._clean_text_for_tts(text)
        if original_text != text:
            logging.info(f"✨ Texto limpo para TTS: '{original_text[:50]}...' -> '{text[:50]}...'")
        # --- FIM DA NOVA ETAPA ---

        if not text.strip(): # Verifica se o texto ficou vazio após a limpeza
            logging.warning("⚠️ Texto vazio após limpeza de parênteses. Nenhuma áudio será gerado.")
            await self.audio_queue.put(None)
            return

        logging.info("🎤 Iniciando geração de áudio com ElevenLabs (download completo)...")
        
        headers = {
            "Accept": "audio/mpeg", # Solicita áudio em formato MP3
            "xi-api-key": self.elevenlabs_api_key
        }
        
        data = {
            "text": text,
            "model_id": "eleven_multilingual_v2", # Modelo para suporte a múltiplos idiomas, incluindo português
            "voice_settings": {
                "stability": 0.75,
                "similarity_boost": 0.75
            }
        }

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    f"{self.elevenlabs_base_url}/{self.voice_id}",
                    headers=headers,
                    json=data,
                    timeout=60 # Timeout para a requisição
                ) as response:
                    response.raise_for_status() # Lança exceção para status de erro (4xx ou 5xx)
                    
                    logging.info("🎧 ElevenLabs: Baixando áudio completo...")
                    full_mp3_data = await response.read() # Lê todo o conteúdo da resposta
                    
                    if not full_mp3_data:
                        logging.warning("⚠️ ElevenLabs: Nenhuma dado MP3 recebido.")
                        await self.audio_queue.put(None)
                        return

                    mp3_buffer = io.BytesIO(full_mp3_data)
                    mp3_buffer.seek(0) # Volta para o início do buffer para ler
                    
                    # Tenta carregar o MP3 e extrair o PCM
                    audio_segment = AudioSegment.from_file(mp3_buffer, format="mp3")
                    
                    # Converte para o formato desejado pelo simpleaudio:
                    # 1 canal (mono), 16-bit (2 bytes por sample), sample_rate correto
                    audio_segment = audio_segment.set_channels(1)
                    audio_segment = audio_segment.set_frame_rate(self.sample_rate)
                    audio_segment = audio_segment.set_sample_width(2) # 16-bit
                    
                    # Obtém os dados brutos PCM
                    raw_pcm_data = audio_segment.raw_data
                    
                    # Converte para array numpy int16
                    audio_array = np.frombuffer(raw_pcm_data, dtype=np.int16)
                    
                    if audio_array.size > 0:
                        logging.info(f"🎤 ElevenLabs: Áudio completo gerado (tamanho: {len(audio_array)} samples). Colocando na fila.")
                        await self.audio_queue.put(audio_array)
                    else:
                        logging.warning("⚠️ ElevenLabs: Áudio decodificado resultou em array vazio.")
                        
        except requests.exceptions.RequestException as e:
            logging.error(f"❌ Erro na requisição ao ElevenLabs: {e}")
        except Exception as e:
            logging.error(f"❌ Erro inesperado durante a geração de áudio: {e}")
        finally:
            logging.info("🎤 ElevenLabs: Geração de áudio concluída. Sinalizando fim do stream.")
            await self.audio_queue.put(None) # Sinaliza o fim do stream

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
        Libera a memória. Menos crítico para APIs externas, mas boa prática.
        """
        gc.collect()
        # Não há cache de GPU para limpar aqui, pois a inferência é remota.

    async def speak_streamed(self, text):
        """
        Inicia o processo de streaming e reprodução.
        """
        await self._load_tts_model()

        generator_task = asyncio.create_task(self._generate_audio_stream(text))
        player_task = asyncio.create_task(self._play_audio_from_queue())

        await asyncio.gather(generator_task, player_task)
        logging.info("Streaming TTS com ElevenLabs concluído.")

# Exemplo de uso
async def main():
    client = StreamingTTSClient()
    
    long_text_pt = ("""
        Olá! Eu sou a Alyssa, sua assistente virtual. (Ela pisca os olhos) Estou muito feliz em poder te ajudar hoje.
        Este é um exemplo de como minha voz soa usando o ElevenLabs. (Um sorriso suave aparece em seu rosto)
        Espero que você goste da qualidade e da naturalidade.
        Estou sempre aprendendo e melhorando para oferecer a melhor experiência possível.
        Se precisar de algo, é só me chamar! (Ela acena com a cabeça)
    """
    )

    long_text_en = ("""
        Hello! I am Alyssa, your virtual assistant. (She blinks her eyes) I am very happy to help you today.
        This is an example of how my voice sounds using ElevenLabs. (A gentle smile appears on her face)
        I hope you enjoy the quality and naturalness.
        I am always learning and improving to offer the best possible experience.
        If you need anything, just call me! (She nods her head)
    """
    )
    
    print("\n--- Teste em Português com ElevenLabs (com parênteses) ---")
    await client.speak_streamed(long_text_pt)
    print("\n--- Teste em Inglês com ElevenLabs (com parênteses) ---")
    await client.speak_streamed(long_text_en)


if __name__ == "__main__":
    import aiohttp
    asyncio.run(main())
