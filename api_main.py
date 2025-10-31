# api_main.py

import asyncio
import logging
import warnings
from typing import Any, Dict, Optional, Generator
from contextlib import asynccontextmanager
import uvicorn
from fastapi import FastAPI, HTTPException, Body
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

# ------------------------------
# 1️⃣  Importações do projeto original
# ------------------------------
from engine.alyssa_engine import AlyssaEngine
from inOut.voice_output_eleven import StreamingTTSClient as ElevenLabsTTSClient
from inOut.voice_output import JarvisTTSClient  # local Piper TTS
# VoicePipeline é removido, pois a entrada virá de requisições HTTP
from config.characters import CHARACTER_CONFIGS

# ------------------------------
# 2️⃣  Configurações globais
# ------------------------------
warnings.filterwarnings("ignore", category=DeprecationWarning)
warnings.filterwarnings("ignore", category=FutureWarning)
warnings.filterwarnings("ignore", category=UserWarning, module="llama_cpp")

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s - %(levelname)s - %(message)s")

loaded_engines: Dict[str, AlyssaEngine] = {}

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Código executado na inicialização do servidor
    print("🚀 Servidor iniciando... Pré-carregando modelos.")
    for name, config in CHARACTER_CONFIGS.items():
        print(f"   -> Carregando modelo para '{name}'...")
        try:
            # Esta é a parte demorada que agora acontece na inicialização
            engine = AlyssaEngine(config)
            loaded_engines[name] = engine
            print(f"   ✅ Modelo para '{name}' carregado com sucesso.")
        except Exception as e:
            print(f"   ❌ Falha ao carregar modelo para '{name}': {e}")
    
    yield # O servidor fica online e processa requisições aqui

    # Código executado no encerramento do servidor (opcional)
    print("💤 Encerrando servidor. Limpando recursos...")
    loaded_engines.clear()

# --- SETUP DA APLICAÇÃO ---
app = FastAPI(
    title="Alyssa Engine API",
    description="API interativa para conversar com personagens de IA.",
    version="1.0.0",
    lifespan=lifespan # Conecta o evento de lifespan
)

# O estado agora guarda apenas a seleção ATUAL
app_state: Dict[str, Any] = {
    "engine": None,
    "voice_client": None,
    "config": None,
}

# (Opcional) A vision_queue pode ser mantida se a visão for usada
# vision_queue: "asyncio.Queue[Dict[str, Any]]" = asyncio.Queue()

# A função `vision_light_loop` (se usada) permaneceria a mesma,
# mas sua inicialização e encerramento seriam gerenciados pela API.


# ----------------------------------------------
# 4️⃣  Modelos Pydantic para Entradas e Saídas
# ----------------------------------------------
class UserInput(BaseModel):
    user_input: str
    include_internal_dialogue: bool = False
    roberta_input: bool = False

class CharacterSelectionResponse(BaseModel):
    message: str
    character_name: str

class TextResponse(BaseModel):
    response_text: str
    internal_dialogue: Optional[str] = None

# ----------------------------------------------
# 5️⃣  Endpoints da API
# ----------------------------------------------

@app.get("/characters", summary="Lista os personagens disponíveis")
async def list_characters():
    """Retorna uma lista com os nomes de todos os personagens configurados."""
    return {"characters": list(CHARACTER_CONFIGS.keys())}

@app.post("/select-character/{character_name}",
            response_model=CharacterSelectionResponse,
            summary="Seleciona um personagem pré-carregado")
async def select_character(character_name: str):
    """
    Seleciona o motor de IA que já foi carregado na memória.
    Esta operação agora é extremamente rápida.
    """
    character_name = character_name.lower()
    if character_name not in loaded_engines:
        raise HTTPException(status_code=404, detail=f"Personagem '{character_name}' não foi carregado ou não existe.")

    # --- Lógica de seleção agora é instantânea ---
    config = CHARACTER_CONFIGS[character_name]
    app_state["config"] = config
    # Apenas pega a referência do engine já carregado
    app_state["engine"] = loaded_engines[character_name]
    logging.info(f"Engine para '{config['name']}' selecionado.")

    # A configuração do cliente de TTS continua rápida, então pode ficar aqui
    if config.get("voice_model_path"):
        app_state["voice_client"] = JarvisTTSClient()
    elif config.get("voice_id"):
        app_state["voice_client"] = ElevenLabsTTSClient(voice_id=config["voice_id"])
    else:
        app_state["voice_client"] = None

    return CharacterSelectionResponse(
        message="Personagem selecionado e pronto para interagir.",
        character_name=config["name"]
    )

@app.post("/interact",
           response_class=StreamingResponse,
           summary="Envia texto e recebe a resposta em áudio (stream)")
async def interact_audio(user_input: UserInput):
    """
    Recebe o texto do usuário e retorna um stream de áudio com a resposta.
    Um personagem deve ter sido selecionado previamente.
    """
    if not app_state.get("engine") or not app_state.get("voice_client"):
        raise HTTPException(
            status_code=400,
            detail="Nenhum personagem com suporte a voz foi selecionado. Use /select-character/{nome} primeiro."
        )

    engine: AlyssaEngine = app_state["engine"]
    voice_client = app_state["voice_client"]

    resp = await engine.handle_input(
        user_input=user_input.user_input,
        include_internal_dialogue=user_input.include_internal_dialogue,
        roberta_input=user_input.roberta_input
    )

    
    reply_text = resp.get("alyssa_voice", "Não consegui formular uma resposta.")
    logging.info(f"Resposta do Engine: {reply_text}")

    try:
        audio_generator = voice_client.speak_streamed(reply_text)
        return StreamingResponse(audio_generator, media_type="audio/mpeg")
    except Exception as e:
        logging.error(f"Falha ao gerar áudio: {e}")
        raise HTTPException(status_code=500, detail="Erro ao processar a resposta de áudio.")


@app.post("/interact-text",
           response_model=TextResponse,
           summary="Envia texto e recebe a resposta em texto")
async def interact_text(user_input: UserInput):
    if not app_state.get("engine"):
        raise HTTPException(
            status_code=400,
            detail="Nenhum personagem selecionado. Use /select-character/{nome} primeiro."
        )

    engine: AlyssaEngine = app_state["engine"]

    # Run the blocking method in a thread and await the result
    resp = await engine.handle_input(
        user_input=user_input.user_input,
        include_internal_dialogue=user_input.include_internal_dialogue,
        roberta_input=user_input.roberta_input
    )


    return TextResponse(
        response_text=resp.get("alyssa_voice", "Não consegui formular uma resposta."),
        internal_dialogue=resp.get("internal_dialogue")
    )
    
@app.get("/spontaneous-thought",
         response_model=TextResponse,
         summary="Gera um pensamento espontâneo do personagem")
async def get_spontaneous_thought():
    """
    Simula o comportamento de ociosidade, gerando um pensamento ou diálogo interno.
    """
    if not app_state.get("engine"):
        raise HTTPException(
            status_code=400,
            detail="Nenhum personagem selecionado. Use /select-character/{nome} primeiro."
        )
    
    engine: AlyssaEngine = app_state["engine"]
    thought = engine.initiate_spontaneous_thought()
    
    return TextResponse(
        response_text="(Pensamento espontâneo gerado)",
        internal_dialogue=thought.get("internal_dialogue", "(sem diálogo)")
    )

@app.post("/reset", summary="Reinicia o estado da sessão no servidor")
async def reset_session():
    """
    Limpa o personagem e o cliente de voz selecionados no estado do servidor.
    Ideal para ser chamado quando um novo cliente se conecta.
    """
    app_state["engine"] = None
    app_state["voice_client"] = None
    app_state["config"] = None
    logging.info("🔄 Estado da sessão reiniciado.")
    return {"message": "Session state has been reset."}


# ----------------------------------------------
# 6️⃣  Execução do Servidor
# ----------------------------------------------
if __name__ == "__main__":
    # Para executar: uvicorn api_main:app --reload
    uvicorn.run(app, host="0.0.0.0", port=8000)