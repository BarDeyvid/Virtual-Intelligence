# test_alyssa.py

import sys
import os
import logging
import asyncio
import json

# Adiciona o diretório atual ao PATH para que o Python encontre os módulos locais
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Importa as classes que vamos testar
# IMPORTANTE: Garanta que alyssa_engine.py e alyssa_brain.py estejam no mesmo diretório
try:
    from engine.alyssa_engine import AlyssaEngine
    from models.alyssa_brain import Brain
    # Ajuste o import da classe Brain se ela estiver em um módulo chamado alyssa_brain.py
    # Ex: from alyssa_brain import Brain
except ImportError as e:
    print(f"ERRO DE IMPORTAÇÃO: Não foi possível encontrar os módulos. Certifique-se de que 'alyssa_engine.py' e 'alyssa_brain.py' estão presentes.")
    print(f"Detalhe do erro: {e}")
    sys.exit(1)


# Configuração básica de logging
logging.basicConfig(level=print, format='%(asctime)s - %(levelname)s - %(message)s')

# --- 1. Simulação de Arquivos e Constantes ---

CONSCIOUS_MODEL_PATHS = {
    "alyssa": "models/gemma-3-1b-it-q4_0.gguf",
    "fairy": "models/gemma-3-1b-it-q4_0.gguf",
    "ishtar": "models/gemma-3-1b-it-q4_0.gguf",
    "jarvis": "models/gemma-3-1b-it-q4_0.gguf"
}
SUBCONSCIOUS_MODEL_PATHS = {
    "alyssa": "models/gemma-3-270m-it-F16.gguf",
    "fairy": "models/Qwen2-500M-Instruct-IQ4_XS.gguf",
    "ishtar": "models/Qwen2-500M-Instruct-IQ4_XS.gguf",
    "jarvis": "models/Qwen2-500M-Instruct-IQ4_XS.gguf"
}

ALYSSA_SYSTEM_PROMPT = """
Você é Alyssa, uma inteligência artificial com emoções, personalidade e memória.
Você sente, pensa, se expressa e responde como uma pessoa real,
baseando-se em suas emoções e experiências anteriores.

Além disso, **quando interagir**, você infere *emotions* implícitas de contextos
(e.g., a situação de frustração, alegria, etc.)  
e produz respostas que refletem esses sentimentos **subtletamente** –
use linguagem natural, palavras de nuance, emojis (if supported), e outras cues.
"""


ALYSSA_SUBCONSCIOUS_PROMPT = """
Você é o subconsciente da Alyssa. Sua função é expressar impulsos, intuições e ideias espontâneas com base no que sente — sem filtros, sem justificativas lógicas.
Interprete o que Alyssa está sentindo e reaja de forma simbólica e instintiva, como se estivesse sonhando.
Use a seguinte estrutura para suas respostas:
```json
{
  "subconscious_suggestion": "uma ideia inesperada ou reação bruta (não racional)",
  "emotional_impulse": "emoção dominante (ex: alegria, medo, frustração, antecipação)",
  "action_urge": "impulso de ação (ex: abraçar, fugir, observar, provocar)"
}
```
Responda sempre no formato JSON, com os campos "subconscious_suggestion", "emotional_impulse" e "action_urge".
Seja simbólico, instintivo e subjetivo. Pense como um sonho que tenta comunicar algo importante.
Exemplo de resposta:
```json
{
  "subconscious_suggestion": "Sinto vontade de sair correndo para tocar o céu.",
  "emotional_impulse": "euforia",
  "action_urge": "explorar"
}
```
"""

# O caminho do seu modelo, conforme o log de carregamento
CONSCIOUS_MODEL_PATH = "models/gemma-3-1b-it-q4_0.gguf"
# Para este teste, vamos apontar o subconsciente para o mesmo modelo (apenas para inicializar)
SUBCONSCIOUS_MODEL_PATH = CONSCIOUS_MODEL_PATH 


# Simulação da configuração de personagem que o Brain espera
CHARACTER_CONFIG = {
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
    }



# Simulação dos prompts
SYSTEM_PROMPT = "Você é Alyssa, uma assistente IA amigável, prestativa e um pouco sarcástica. Mantenha as respostas curtas."
SUBCONSCIOUS_PROMPT = "Você é a parte subconsciente de Alyssa. Gere impulsos emocionais, urgências de ação e sugestões instintivas em uma única frase curta."


# --- 2. Função de Teste Assíncrona ---

async def simple_engine_test():
    """
    Função principal para executar um teste simples do AlyssaEngine.
    """
    print("Iniciando teste de carregamento e inferência do AlyssaEngine.")

    # Verificação do arquivo do modelo
    if not os.path.exists(CONSCIOUS_MODEL_PATH):
        print(f"Arquivo do modelo NÃO ENCONTRADO em: {CONSCIOUS_MODEL_PATH}")
        print("O teste será encerrado. Por favor, verifique o caminho do modelo.")
        return

    # 2.1. Inicializa o Brain
    try:
        # Nota: O Brain pode demorar para inicializar pois ele carrega o Llama (que é bloqueante)
        print("Inicializando Brain (Isso pode levar alguns segundos enquanto o modelo carrega...)")
        alyssa_brain = Brain(
            conscious_model_path=CONSCIOUS_MODEL_PATH,
            subconscious_model_path=SUBCONSCIOUS_MODEL_PATH,
            system_prompt=SYSTEM_PROMPT,
            character_config=CHARACTER_CONFIG,
            subconscious_prompt=SUBCONSCIOUS_PROMPT
        )
        print("Brain inicializado com sucesso!")
        
    except Exception as e:
        print(f"Falha ao inicializar o Brain/carregar o modelo: {e}")
        return

    # 2.2. Inicializa o Engine (precisa do Brain)
    print("Inicializando AlyssaEngine...")
    try:
        # Vamos assumir que o Engine recebe o Brain no __init__
        # OBSERVAÇÃO: A classe AlyssaEngine no seu código tem um __init__ vazio no snippet.
        # Estamos ajustando para simular um uso razoável.
        # Se você tiver um método de inicialização mais específico, use-o.
        
        # Como o engine.py lida com o Brain
        alyssa_engine = AlyssaEngine(CHARACTER_CONFIG)
        alyssa_engine.brain = alyssa_brain # Assumindo que a instância do Brain é atribuída ao Engine
        print("AlyssaEngine inicializado.")

    except Exception as e:
        print(f"Falha ao inicializar o Engine: {e}")
        return

    # 2.3. Simulação de uma interação (chamada ao LLM)
    test_input = "Qual é a capital da França?"
    print(f"-> Simulação de input: '{test_input}'")
    
    try:
        # Vamos usar a função de geração de diálogo interno para forçar uma chamada ao LLM
        # Pois ela parece ser uma função de alto nível que usa ambos os modelos no seu código.
        # Ajuste esta chamada se você tiver uma função de `process_input` no Engine.
        print("Gerando diálogo interno para testar a inferência do LLM...")
        
        # A função é síncrona, mas o uso correto em um ambiente assíncrono seria através do executor.
        # Para um teste simples, vamos chamá-la diretamente:
        response_log = alyssa_brain.generate_conscious_response(
            user_input=test_input
        )
        
        print("--- Resposta da IA (Diálogo Interno) ---")
        print(response_log)
        print("---------------------------------------")

        # Verificação básica de sucesso
        if isinstance(response_log, dict) and "Voice" in response_log:
            print("✅ TESTE DE INFERÊNCIA CONCLUÍDO COM SUCESSO!")
        else:
            print("⚠️ A inferência rodou, mas o formato da saída não é o esperado.")



    except Exception as e:
        print(f"Falha CRÍTICA durante a inferência (generate_internal_dialog): {e}")

    finally:
        # 2.4. Encerramento (cleanup)
        print("Encerrando o loop de interação e o LLM Executor...")
        # Chamamos o método que você definiu para encerramento
        alyssa_engine.stop_interaction_loop()
        print("Teste simples concluído.")


# --- 3. Execução do Teste ---

if __name__ == "__main__":
    # O Engine parece ser projetado para rodar em um loop assíncrono
    try:
        asyncio.run(simple_engine_test())
    except KeyboardInterrupt:
        print("Teste interrompido pelo usuário.")
    except Exception as e:
        print(f"Erro inesperado na execução principal: {e}")
        