import asyncio
import logging
import discord
import time
import io # Para lidar com BytesIO para FFmpegPCMAudio

# Importe suas classes da Alyssa
from engine.alyssa_engine import AlyssaEngine
from inOut.voice_output_eleven_discord import StreamingTTSClient
from inOut.voice_input import VoicePipeline 
from config.system_prompt import DISCORD_TOKEN

logging.basicConfig(level=logging.INFO)

logging.basicConfig(level=logging.INFO)

# --- Configurações dos Modelos da Alyssa ---
CONSCIOUS_MODEL_PATH = "models/gemma-3-1b-it-Q3_K_L.gguf"
SUBCONSCIOUS_MODEL_PATH = "models/Qwen2-500M-Instruct-IQ4_XS.gguf"
MAIN_USER_ID = 1328578037192593440 # <<< SUBSTITUA PELO SEU ID DE USUÁRIO DO DISCORD

# --- Gerenciamento de Instâncias da Alyssa por Usuário ---
# Cada entrada será: {user_id: AlyssaEngine_instance}
user_alyssa_engines = {} 

# --- Instâncias Globais de Clientes de Voz (Compartilhadas) ---
alyssa_voice_client_tts = None 
alyssa_voice_pipeline_stt = None 

# --- Variáveis para Controle de Voz no Discord ---
discord_voice_connection = None # Armazena a conexão de voz do Discord
discord_text_channel = None     # Armazena o canal de texto padrão para respostas/logs
alyssa_is_in_voice_call = False # Flag para controlar se a Alyssa está em uma call

# --- Detecção de Ociosidade (Global para o bot, mas pode ser por usuário se desejar) ---
# Usamos um dicionário para rastrear o tempo do último input por usuário,
# o que é mais consistente com a memória por usuário.
last_user_input_times = {} 

# --- Configuração do Discord Bot ---
intents = discord.Intents.default()
intents.message_content = True 
intents.voice_states = True    
intents.members = True         

client = discord.Client(intents=intents)

@client.event
async def on_ready():
    """
    Evento disparado quando o bot se conecta ao Discord.
    Aqui inicializamos os clientes de voz e iniciamos a tarefa de processamento de voz local.
    """
    global alyssa_voice_client_tts, alyssa_voice_pipeline_stt
    
    logging.info(f'🤖 Logado como {client.user} (ID: {client.user.id})')
    print("--- Bot Discord da Alyssa Online ---")

    alyssa_voice_client_tts = StreamingTTSClient() # ElevenLabs TTS
    alyssa_voice_pipeline_stt = VoicePipeline()    # Local Microphone STT
    
    # Inicia o pipeline de voz local da Alyssa (STT)
    # Ele sempre escuta o microfone local. A decisão de *processar* o input
    # e *responder* a ele será feita na lógica do bot, dependendo se a Alyssa
    # está em uma call e de qual usuário está falando.
    alyssa_voice_pipeline_stt.start()
    logging.info("🧠 Clientes de Voz (TTS/STT) iniciados.")
    print("Pronto para interagir!")

    # Inicia a tarefa de processamento de voz local e detecção de ociosidade
    client.loop.create_task(process_local_voice_input_and_idle_detection())

async def get_or_create_alyssa_engine(user_id: int, loop: asyncio.AbstractEventLoop) -> AlyssaEngine:
    """
    Retorna a instância do AlyssaEngine para um dado user_id.
    Cria uma nova instância se não existir, com um DB de memória separado.
    """
    if user_id not in user_alyssa_engines:
        # Define um caminho de DB único para cada usuário
        db_path = f"sqlite:///memory/memoria_discord_user_{user_id}.sqlite"
        user_alyssa_engines[user_id] = AlyssaEngine(
            CONSCIOUS_MODEL_PATH,
            SUBCONSCIOUS_MODEL_PATH,
            db_path=db_path
        )
        logging.info(f"✨ Nova instância do AlyssaEngine criada para o usuário {user_id} com DB em {db_path}")
    return user_alyssa_engines[user_id]

async def process_local_voice_input_and_idle_detection():
    """
    Tarefa assíncrona para processar input de voz do microfone local
    e lidar com a detecção de ociosidade.
    Este input é assumido como sendo do proprietário do bot (DeyvidB) para simplificar.
    """
    global discord_text_channel, alyssa_is_in_voice_call

    # ID do usuário principal (DeyvidB) para atribuir o input de voz local
    # Você precisará descobrir seu próprio ID de usuário do Discord (clique com o botão direito no seu nome -> Copiar ID)

    # Garante que a instância do engine para o usuário principal exista
    alyssa_engine_for_main_user = await get_or_create_alyssa_engine(MAIN_USER_ID, client.loop)
    last_user_input_times[MAIN_USER_ID] = time.time() # Inicializa o tempo para o usuário principal

    while True:
        user_input_result = alyssa_voice_pipeline_stt.get_last_result()
        
        if user_input_result:
            user_input_text = user_input_result.get('text', '')
            
            # Processa o input de voz local SOMENTE se a Alyssa estiver em uma call
            if alyssa_is_in_voice_call and discord_text_channel:
                logging.info(f"Você (voz local - MAIN_USER): {user_input_text}")
                
                # Processa a entrada com a AlyssaEngine (agora assíncrona)
                result = alyssa_engine_for_main_user.handle_input(user_input_text)

                internal_dialogue = result.get("internal_dialogue", "(sem diálogo)")
                alyssa_response_text = result.get("alyssa_voice", "(falha ao responder)")

                logging.info(f"\n[🧠 Diálogo Interno (MAIN_USER)]\n{internal_dialogue}")
                
                # Envia a resposta da Alyssa (decide entre voz e texto)
                # Sempre vocaliza se o input veio da voz local e ela está em call
                await send_alyssa_response(alyssa_response_text, discord_text_channel, vocalize_if_in_call=True)
                
                last_user_input_times[MAIN_USER_ID] = time.time() # Reseta o tempo do último input do usuário
            else:
                logging.debug(f"Input de voz local detectado, mas Alyssa não está em call ou canal de texto não definido. Ignorando: {user_input_text[:50]}...")
            
        else:
            # Verifica ociosidade para o usuário principal
            current_time = time.time()
            if (current_time - last_user_input_times.get(MAIN_USER_ID, current_time)) > alyssa_engine_for_main_user.idle_threshold:
                logging.info(f"⏳ Ociosidade detectada para o usuário principal. Gerando pensamento espontâneo...")
                
                spontaneous_thought_result = await alyssa_engine_for_main_user.initiate_spontaneous_thought() # Agora assíncrono
                
                spontaneous_dialogue = spontaneous_thought_result.get("internal_dialogue", "(sem diálogo)")
                logging.info(f"\n[💭 Alyssa pensa sozinha (MAIN_USER)...]\n{spontaneous_dialogue}")
                
                # Se a Alyssa não estiver em uma call, ela pode "pensar sozinha" no console/log.
                # Não vocalizamos pensamentos espontâneos por padrão, mas podemos enviar para o chat de texto.
                if discord_text_channel and not alyssa_is_in_voice_call:
                    await discord_text_channel.send(f"💭 Alyssa pensa sozinha... (verifique o console para detalhes)")
                
                last_user_input_times[MAIN_USER_ID] = time.time() # Reseta o timer de ociosidade após o pensamento espontâneo

        await asyncio.sleep(0.1) # Pequeno atraso para evitar loop muito apertado

async def send_alyssa_response(response_text: str, channel: discord.TextChannel, vocalize_if_in_call: bool = False):
    """
    Envia a resposta da Alyssa para o Discord, decidindo entre voz ou texto.
    Aplica a limpeza de parênteses para a resposta de texto e vocalizada.
    """
    global discord_voice_connection, alyssa_is_in_voice_call 

    # A limpeza de parênteses é feita internamente por _generate_audio_data no StreamingTTSClient
    # Vamos aplicá-la também para a mensagem de texto no Discord para consistência.
    cleaned_response_text = alyssa_voice_client_tts._clean_text_for_tts(response_text)

    # Condição para vocalizar: se for explicitamente pedido E a Alyssa estiver em uma call E a conexão de voz estiver ativa
    should_vocalize = vocalize_if_in_call and alyssa_is_in_voice_call and discord_voice_connection and discord_voice_connection.is_connected()

    if should_vocalize:
        logging.info(f"🗣️ Alyssa (voz no Discord): {cleaned_response_text}")
        
        # Gera os dados de áudio (MP3 em BytesIO)
        mp3_buffer = await alyssa_voice_client_tts._generate_audio_data(response_text)

        if mp3_buffer:
            try:
                source = discord.FFmpegPCMAudio(mp3_buffer, pipe=True) 
                
                if discord_voice_connection.is_playing():
                    discord_voice_connection.stop() 
                
                discord_voice_connection.play(source, after=lambda e: logging.error(f'Player error: {e}') if e else None)
                logging.info(f"🗣️ Alyssa (voz no Discord): Reproduzindo áudio.")
                
                while discord_voice_connection.is_playing():
                    await asyncio.sleep(0.1)
            except Exception as e:
                logging.error(f"❌ Erro ao reproduzir voz no Discord: {e}", exc_info=True)
                # Envia mensagem de texto APENAS SE A VOCALIZAÇÃO FALHOU
                await channel.send(f"❌ Alyssa: Desculpe, tive um problema com minha voz. {cleaned_response_text}")
        else:
            logging.warning("⚠️ Nenhum áudio MP3 recebido do ElevenLabs para reprodução no Discord.")
            # Envia mensagem de texto APENAS SE A VOCALIZAÇÃO FALHOU
            await channel.send(f"⚠️ Alyssa: (Problema ao gerar áudio) {cleaned_response_text}")
    else:
        # Sempre envia a resposta de texto se não houve tentativa de vocalizar
        await channel.send(f"🗣️ Alyssa: {cleaned_response_text}")
        logging.info(f"🗣️ Alyssa (texto no Discord): {cleaned_response_text}")


@client.event
async def on_message(message):
    """
    Evento disparado quando uma mensagem é enviada em um canal que o bot pode ver.
    """
    global discord_text_channel, alyssa_is_in_voice_call, discord_voice_connection 

    if message.author == client.user:
        return

    # Define o canal de texto padrão para respostas, se ainda não estiver definido
    if not discord_text_channel:
        discord_text_channel = message.channel

    # Atualiza o tempo do último input para o usuário específico
    last_user_input_times[message.author.id] = time.time() 

    logging.info(f'Mensagem de {message.author.display_name} ({message.author.id}): {message.content}')

    # --- Comandos de Voz ---
    if message.content.lower() == '!join':
        if message.author.voice:
            try:
                if discord_voice_connection and discord_voice_connection.is_connected():
                    await discord_voice_connection.move_to(message.author.voice.channel)
                    await message.channel.send(f"Já estou aqui, movendo para {message.author.voice.channel.mention}!")
                else:
                    discord_voice_connection = await message.author.voice.channel.connect()
                    await message.channel.send(f"Conectado ao canal de voz: {message.author.voice.channel.mention}!")
                
                alyssa_is_in_voice_call = True 
                logging.info(f"Bot conectado ao canal de voz: {message.author.voice.channel.name}. alyssa_is_in_voice_call = True")
            except discord.ClientException as e:
                await message.channel.send(f"Não consegui me conectar ao canal de voz: {e}")
                logging.error(f"Erro ao conectar ao canal de voz: {e}")
            except Exception as e:
                await message.channel.send(f"Ocorreu um erro ao tentar conectar: {e}")
                logging.error(f"Erro inesperado ao conectar ao canal de voz: {e}")
        else:
            await message.channel.send("Você precisa estar em um canal de voz para me usar o `!join`!")
        return # Não processa como input da Alyssa

    if message.content.lower() == '!leave':
        if discord_voice_connection and discord_voice_connection.is_connected():
            await discord_voice_connection.disconnect()
            discord_voice_connection = None
            alyssa_is_in_voice_call = False 
            await message.channel.send("Desconectado do canal de voz. Até mais!")
            logging.info("Bot desconectado do canal de voz. alyssa_is_in_voice_call = False")
        else:
            await message.channel.send("Eu não estou em nenhum canal de voz no momento.")
        return 

    # --- Processamento de Mensagens de Texto para a Alyssa ---
    # Obtém a instância do AlyssaEngine para o usuário que enviou a mensagem
    alyssa_engine_for_user = await get_or_create_alyssa_engine(message.author.id, client.loop)

    user_input_text = message.content
    if user_input_text.strip().lower() == "sair":
        await message.channel.send("Encerrando a Alyssa. Adeus!")
        alyssa_voice_pipeline_stt.stop() 
        await client.close() 
        return

    # Processa a entrada com a AlyssaEngine (agora assíncrona)
    result = alyssa_engine_for_user.handle_input(user_input_text)

    internal_dialogue = result.get("internal_dialogue", "(sem diálogo)")
    alyssa_response_text = result.get("alyssa_voice", "(falha ao responder)")

    logging.info(f"\n[🧠 Diálogo Interno para {message.author.display_name}]\n{internal_dialogue}")

    # Nova lógica:
    # Vocaliza APENAS se a mensagem veio do MAIN_USER_ID e a Alyssa estiver em uma call.
    # Caso contrário, apenas responde por texto.
    should_vocalize = (message.author.id == MAIN_USER_ID) and alyssa_is_in_voice_call

    await send_alyssa_response(alyssa_response_text, message.channel, vocalize_if_in_call=should_vocalize)


# --- Função principal para rodar o bot ---
def run_discord_bot():
    try:
        client.run(DISCORD_TOKEN)
    except discord.LoginFailure:
        logging.error("❌ Falha no login. Verifique seu token do bot.")
    except Exception as e:
        logging.error(f"❌ Erro ao iniciar o bot Discord: {e}", exc_info=True)

if __name__ == '__main__':
    run_discord_bot()