import asyncio
from collections import deque
import logging
import warnings
import time
from typing import Any, Dict

import torch.multiprocessing as mp
# ------------------------------
# 1️⃣  Importações principais
# ------------------------------
from engine.alyssa_engine import AlyssaEngine          
from inOut.voice_output_eleven import StreamingTTSClient as ElevenLabsTTSClient
from inOut.voice_output import JarvisTTSClient          
from inOut.voice_input import VoicePipeline
from config.characters import CHARACTER_CONFIGS        
# ------------------------------
# 2️  Configurações globais
# ------------------------------
warnings.filterwarnings("ignore", category=DeprecationWarning)
warnings.filterwarnings("ignore", category=FutureWarning)
warnings.filterwarnings(
    "ignore",
    category=UserWarning,
    module="llama_cpp"
)

# ----------------------------------------------
# 3️  Criação da queue compartilhada entre threads
# ----------------------------------------------
vision_queue: "asyncio.Queue[Dict[str, Any]]" = asyncio.Queue()

logging.basicConfig(level=logging.INFO, filemode="a", filename="logs/alyssa_engine.log",
                    format="%(asctime)s - %(levelname)s - %(message)s")


# ----------------------------------------------
# 4  Função principal assíncrona
# ----------------------------------------------
async def main_loop() -> None:
    # --------- Escolha do personagem ----------
    print("Selecione um personagem para interagir:")
    for i, name in enumerate(CHARACTER_CONFIGS.keys(), start=1):
        print(f"{i}. {name.capitalize()}")

    chosen = None
    while chosen not in CHARACTER_CONFIGS:
        inp = input("Digite o número ou nome do personagem: ").strip().lower()
        if inp.isdigit() and 1 <= int(inp) <= len(CHARACTER_CONFIGS):
            chosen = list(CHARACTER_CONFIGS.keys())[int(inp)-1]
        elif inp in CHARACTER_CONFIGS:
            chosen = inp
        else:
            print("Escolha inválida. Tente novamente.")

    config = CHARACTER_CONFIGS[chosen]

    # --------- Inicializar engine ----------
    engine = AlyssaEngine(config)          # O __init__ já aceita o dicionário completo

    # 4a – Iniciar a visão leve em background
    # asyncio.create_task(vision_light_loop(engine))

    # 4b – Consumir a queue dentro do próprio engine (nova task)
    #      (o método consume_vision_queue() está implementado em alyssa_engine.py)
    #asyncio.create_task(engine.consume_vision_queue())

    # --------- Selecionar cliente de TTS ----------
    voice_client = None
    if config.get("voice_model_path"):
        voice_client = JarvisTTSClient()   # Piper local
        logging.info(f"🎤 {config['name']} usará TTS local (Piper).")
    elif config.get("voice_id"):
        voice_client = ElevenLabsTTSClient(voice_id=config["voice_id"])
        logging.info(f"🎤 {config['name']} usará ElevenLabs TTS.")
    else:
        logging.warning(
            f"⚠️ Nenhum TTS configurado para {config['name']}. Apenas texto será exibido."
        )

    # --------- Inicializar pipeline de voz ----------
    pipeline = VoicePipeline()
    pipeline.start()

    print(config.get("initial_message", "Engine iniciado. Digite 'sair' para encerrar.\n"))

    last_input_ts = time.time()

    try:
        while True:
            start = time.time()
            result = pipeline.get_last_result()
            if result:
                text = result.get("text", "")
                print(f"🗣️ Entrada do usuário: {text}")
                if text.strip().lower() == "sair":
                    break

                last_input_ts = time.time()

                # Chamada assíncrona ao engine
                resp = await engine.enqueue_text(
                    user_input=text,
                    include_internal_dialogue=False,   # podemos mudar se quiser o diálogo interno
                    roberta_input=False                 # ativa/desativa Roberta conforme necessidade
                )
                reply_text = resp.get("alyssa_voice", "(falha ao responder)")
                print(reply_text)
                print("-" * 60)

                end = time.time()
                print(f"Tempo de resposta: {end - start:.2f} segundos")
                if voice_client:
                    await voice_client.speak_streamed(reply_text)

            else:
                # Se não há entrada, verificamos o tempo de inatividade
                now = time.time()
                if (now - last_input_ts) > engine.idle_threshold:
                    logging.info(
                        f"⏳ Ociosidade detectada para {config['name']}. Gerando pensamento espontâneo..."
                    )
                    spont = engine.initiate_spontaneous_thought()
                    print(f"\n[💭 {config['name'].capitalize()} pensa sozinho...]")
                    print(spont.get("internal_dialogue", "(sem diálogo)"))
                    print("-" * 60)
                    last_input_ts = time.time()

            await asyncio.sleep(0.1)

    except KeyboardInterrupt:
        print(f"\nEncerrando {config['name']} Engine.")
    finally:
        pipeline.stop()
        print("✅ VoicePipeline encerrada.")


# ----------------------------------------------
# 5 Execução
# ----------------------------------------------
if __name__ == "__main__":
    mp.set_start_method('spawn', force=True)
    asyncio.run(main_loop())