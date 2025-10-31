import asyncio
import logging
from models.alyssa_brain import AlyssaBrain

# Ative o log de debug
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

# Caminhos para os modelos
CONSCIOUS_PATH = "models/gemma-3-1b-it-Q3_K_L.gguf"
SUBCONSCIOUS_PATH = "models/Qwen2-500M-Instruct-IQ4_XS.gguf"

async def main():
    print("💭 Teste de Diálogo Interno da Alyssa")
    brain = AlyssaBrain(conscious_model_path=CONSCIOUS_PATH, subconscious_model_path=SUBCONSCIOUS_PATH)

    while True:
        user_input = input("\nVocê (gatilho): ").strip()
        if user_input.lower() in ["sair", "exit", "quit"]:
            break

        print("\n🌀 Simulando diálogo interno...")
        internal_thoughts = brain.internal_dialogue_with_subconscious(
            trigger_text=user_input,
            emotion_state={"trust": 0.6, "curiosity": 0.4},
            context_info="intenção: introspecção"
        )

        print("\n--- Diálogo Interno ---")
        print(internal_thoughts)
        print("-----------------------")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nEncerrado pelo usuário.")
        