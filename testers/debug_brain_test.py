import asyncio
import logging
from models.alyssa_brain import AlyssaBrain

# Ative logging detalhado para debug
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

# Caminhos dos modelos (ajuste para os caminhos corretos no seu sistema)
CONSCIOUS_PATH = "models/gemma-3-1b-it-Q3_K_L.gguf"
SUBCONSCIOUS_PATH = "models/Qwen2-500M-Instruct-IQ4_XS.gguf"

async def main():
    print("🧠 Iniciando teste de AlyssaBrain...")
    brain = AlyssaBrain(conscious_model_path=CONSCIOUS_PATH, subconscious_model_path=SUBCONSCIOUS_PATH)

    while True:
        user_input = input("\nVocê: ").strip()
        if user_input.lower() in ['sair', 'exit', 'quit']:
            break

        print("\n🎭 Gerando impulso subconsciente...")
        subconscious = brain.generate_subconscious_suggestion(
            raw_thought_trigger=user_input,
            emotion_state={"anticipation": 0.5, "joy": 0.2},
            meta_context="intenção: aprendizado"
        )
        print(f"Subconsciente:\n{subconscious}\n")

        print("🧩 Gerando resposta consciente...")
        response = brain.generate_conscious_response(
            user_input=user_input,
            structured_history="",  # pode adicionar história aqui futuramente
            emotion_state={"joy": 0.4, "trust": 0.6},
            subconscious_suggestion=subconscious["subconscious_suggestion"],
            context_info="hora: 16h; intenção ativa: aprendizado"
        )
        print(f"Alyssa: {response['AlyssaVoice']}\n")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nEncerrado pelo usuário.")
