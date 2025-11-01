# config/characters.py

from config.system_prompt import *

CONSCIOUS_MODEL_PATHS = {
    "alyssa": "models/gemma-3-1b-it-q4_0.gguf",
    "fairy": "models/gemma-3-1b-it-q4_0.gguf",
    "ishtar": "models/gemma-3-1b-it-q4_0.gguf",
    "jarvis": "models/gemma-3-1b-it-q4_0.gguf"
}
SUBCONSCIOUS_MODEL_PATHS = {
    "alyssa": "models/gemma-3-270m-it-F16.gguf",
    "fairy": "models/gemma-3-270m-it-F16.gguf",
    "ishtar": "models/Qwen2-500M-Instruct-IQ4_XS.gguf",
    "jarvis": "models/Qwen2-500M-Instruct-IQ4_XS.gguf"
}

CHARACTER_CONFIGS = {
    "alyssa": {
        "name": "Alyssa",
        "conscious_model_path": CONSCIOUS_MODEL_PATHS["alyssa"],
        "subconscious_model_path": SUBCONSCIOUS_MODEL_PATHS["alyssa"],
        "system_prompt": ALYSSA_SYSTEM_PROMPT,  
        "subconscious_prompt": ALYSSA_SUBCONSCIOUS_PROMPT,
        "db_path": "sqlite:///memory/alyssa_memoria_db.sqlite",
        "initial_message": "🧠 Alyssa Engine iniciado. Digite 'sair' para encerrar.",
        "voice_id": "T3ZeSw265kJ0jRIeLTFw",
        "voice_model_path": None, # Alyssa might not use local TTS
        "use_vision": False,
        "context_token_limit": 32768
    },
    "ishtar": {
        "name": "Ishtar",
        "conscious_model_path": CONSCIOUS_MODEL_PATHS["ishtar"],
        "subconscious_model_path": SUBCONSCIOUS_MODEL_PATHS["ishtar"],
        "system_prompt": ISHTAR_SYSTEM_PROMPT,
        "subconscious_prompt": ISHTAR_SUBCONSCIOUS_PROMPT,
        "db_path": "sqlite:///memory/ishtar_memoria_db.sqlite",
        "initial_message": "📊 Ishtar Online. Aguardando comandos.",
        "voice_id": "8EkOjt4xTPGMclNlh1pk",
        "voice_model_path": None, # Ishtar might not use local TTS
        "use_vision": False
    },
    "jarvis": {
        "name": "Jarvis",
        "conscious_model_path": CONSCIOUS_MODEL_PATHS["jarvis"],
        "subconscious_model_path": SUBCONSCIOUS_MODEL_PATHS["jarvis"],
        "system_prompt": JARVIS_SYSTEM_PROMPT,
        "subconscious_prompt": JARVIS_SUBCONSCIOUS_PROMPT,
        "db_path": "sqlite:///memory/jarvis_memoria_db.sqlite",
        "initial_message": "🤖 Jarvis at your service. How may I assist you?",
        "voice_id": "abRFZIdN4pvo8ZPmGxHP",
        "voice_model_path": "models/Jarvis-High.onnx", # Assuming Jarvis uses a local TTS model
        "use_vision": True
    },
    "fairy": {
        "name": "Fairy",
        "system_prompt": FAIRY_SYSTEM_PROMPT,
        "subconscious_prompt": FAIRY_SUBCONSCIOUS_PROMPT,
        "conscious_model_path": CONSCIOUS_MODEL_PATHS["fairy"],
        "subconscious_model_path": SUBCONSCIOUS_MODEL_PATHS["fairy"],
        "db_path": "sqlite:///memory/fairy_memoria_db.sqlite",
        "sass_level": 3,
        "voice_id": "EXAVITQu4vr4xnSDxMaL",  # ElevenLabs sassy voice
        "initial_message": "Fairy online. Try not to disappoint me, Proxy.",
        "context_token_limit": 8192,
    }
}