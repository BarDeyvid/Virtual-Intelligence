from llama_cpp import Llama
from typing import Optional, Dict, Any
from transformers import AutoTokenizer
import ctypes
import logging
import json


class Brain:
    """
    Núcleo cognitivo da IA. Gerencia o processamento de entradas e respostas
    através dos modelos consciente e subconsciente. Cada um com propósito distinto:
    
    - Consciente: gera respostas sensíveis, empáticas e ricas.
    - Subconsciente: gera impulsos, ideias espontâneas, emoções não mediadas.
    """

    def __init__(self, conscious_model_path: str, subconscious_model_path: str,
                 system_prompt: str, character_config: Dict[str, Any], subconscious_prompt: str): # Add new parameters
        self.conscious_model_path = conscious_model_path
        self.subconscious_model_path = subconscious_model_path
        self.system_prompt = system_prompt # Store the character-specific system prompt
        self.subconscious_prompt = subconscious_prompt # Store the character-specific subconscious prompt
        self.tokenizer = AutoTokenizer.from_pretrained("GPT2")  # Usar tokenizer GPT-2 para contagem de tokens
        self.ctx_limit = character_config.get("context_token_limit", 4000)  # Default context limit
        try:
            self.conscious_model = Llama(
                model_path=self.conscious_model_path,
                n_gpu_layers=-1,
                n_ctx=self.ctx_limit, # Use the context limit from the character config
                n_batch=512,
                use_mlock=True,
                flash_attn=True,
                verbose=True
            )
            logging.info("Conscious model loaded successfully.")
        except Exception as e:
            logging.error(f"Failed to load conscious model: {e}")
            raise

        try:
            self.subconscious_model = Llama( 
                model_path=self.subconscious_model_path,
                n_gpu_layers=16, 
                n_ctx=512,
                n_batch=256,
                use_mlock=False,
                flash_attn=True,
                verbose=True
            )
            logging.info("Subconscious model loaded successfully.")
        except Exception as e:
            logging.error(f"Failed to load subconscious model: {e}")
            raise
        
    def generate_conscious_response(
        self,
        user_input: str,
        structured_history: Optional[str] = None,
        emotion_state: Optional[Dict[str, float]] = None,
        subconscious_suggestion: Optional[str] = None,
        context_info: Optional[str] = None,
        system_directives: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Gera a resposta consciente, agora esperando uma saída JSON do modelo
        para extrair ações, voz e decisões de visão.
        """

        # 1️⃣ Montagem de prefixo emocional
        emotion_prefix = "; ".join(f"{k}: {round(v, 2)}" for k, v in (emotion_state or {}).items())

        # 2️⃣ Montagem do prompt completo
        messages = [
            {"role": "system", "content": system_directives or self.system_prompt},
            {"role": "user",
             "content": f"{structured_history or ''}\n<start_of_turn>user\n{emotion_prefix}; {subconscious_suggestion or ''}; {context_info or ''}; {user_input}\n<end_of_turn>\n<start_of_turn>model\n"}
        ]

         # 3️⃣ Chamada ao modelo, agora com response_format para JSON
        response_obj = self.conscious_model.create_chat_completion(
            messages=messages,
            temperature=0.8,
            top_p=0.9,
            stop=["<|eot_id|>", "```"], # Adiciona ``` como stop token para evitar JSON mal formado
            response_format={"type": "json_object"} # Força a saída em JSON
        )
        content = response_obj['choices'][0]['message']['content'].strip()

        # 4️⃣ Fazer o parsing da resposta JSON
        try:
            # Tenta limpar o conteúdo de markdown de código se houver
            if content.startswith("```json"):
                content = content[7:]
            if content.endswith("```"):
                content = content[:-3]
            
            parsed_response = json.loads(content)
            
            # Garante que a estrutura básica exista
            return {
                "Voice": parsed_response.get("Voice", content),
                "estado_emocional": parsed_response.get("estado_emocional", emotion_state),
                "decisao": parsed_response.get("decisao", "resposta_generica"),
                "reasoning": parsed_response.get("reasoning", "unstructured_response"),
                "vision_action": parsed_response.get("vision_action", "none"), # <--- CHAVE IMPORTANTE
                "memory_to_store": parsed_response.get("memory_to_store"),
                "actions": parsed_response.get("actions"),
                "motor_output": parsed_response.get("motor_output"),
                "raw_response": content
            }
        except json.JSONDecodeError:
            # Se o LLM não retornar um JSON válido, retorna a resposta como texto simples
            logging.warning("Falha ao decodificar JSON do LLM. Usando resposta bruta.")
            return {
                "Voice": content,
                "estado_emocional": emotion_state,
                "decisao": "falha_json",
                "reasoning": "LLM não gerou JSON válido.",
                "vision_action": "none",
                "raw_response": content,
            }

    def generate_subconscious_suggestion(
        self,
        trigger_text: str,
        emotion_state: Optional[Dict[str, float]] = None,
        context_info: Optional[str] = None
    ) -> Dict[str, str]:
        try:
            subconscious_prompt_content = self.subconscious_prompt

            # Prepare the prompt
            prompt = [
                {"role": "system", "content": subconscious_prompt_content},
                {"role": "user", "content": trigger_text}
            ]

            response = self.subconscious_model.create_chat_completion(
                messages=prompt,
                temperature=0.5, # Increase randomness for creativity
                max_tokens=60,
                response_format={"type": "json_object"}  
            )

            raw_content = ""
            if isinstance(response, dict):
                choices = response.get("choices", [])
                if isinstance(choices, list) and len(choices) > 0:
                    first_choice = choices[0]
                    if isinstance(first_choice, dict):
                        message = first_choice.get("message", {})
                        if isinstance(message, dict):
                            raw_content = message.get("content", "")
                        elif isinstance(message, str):
                            raw_content = message
                    elif isinstance(first_choice, str):
                        raw_content = first_choice

            parsed = {}
            if raw_content:
                try:
                    parsed = json.loads(raw_content)
                    if not isinstance(parsed, dict):
                        parsed = {"subconscious_suggestion": str(raw_content)}
                except (json.JSONDecodeError, TypeError):
                    parsed = {"subconscious_suggestion": str(raw_content)}

            # Ensure we have the required fields
            result = {
                "subconscious_suggestion": parsed.get("subconscious_suggestion", "").strip(),
                "emotional_impulse": parsed.get("emotional_impulse", "neutro").strip(),
                "action_urge": parsed.get("action_urge", "nenhuma").strip()
            }

            return result

        except Exception as e:
            logging.error(f"Erro ao gerar resposta subconsciente: {str(e)}", exc_info=True)
            return {
                "subconscious_suggestion": f"Erro interno: {type(e).__name__}",
                "emotional_impulse": "confusa",
                "action_urge": "nenhuma"
            }
        finally:
            pass

    def internal_dialogue_with_subconscious(
        self,
        trigger_text: str,
        emotion_state: Optional[Dict[str, float]] = None,
        context_info: Optional[str] = None
    ) -> str:
        try:
            log = []
            impulse = self.generate_subconscious_suggestion(trigger_text, emotion_state, context_info)
            log.append(f"🤔 Subconsciente: {impulse['subconscious_suggestion']}")

            reflection_prompt = (
                f"O subconsciente sugeriu: '{impulse['subconscious_suggestion']}'.\n"
                f"Qual o sentido por trás disso? Isso faz sentido para mim?\n"
                f"Impulso emocional: {impulse['emotional_impulse']}. Ação sugerida: {impulse['action_urge']}"
            )
            # This calls generate_subconscious_suggestion again, which uses self.subconscious_prompt
            reply = self.generate_subconscious_suggestion(reflection_prompt, emotion_state, context_info)
            log.append(f"🧠(pensamento): {reply['subconscious_suggestion']}")

            return "\n".join(log)
        except Exception as e:
            logging.error(f"Erro no diálogo interno: {e}")
            return "(Falha no diálogo interno)"
    
    def _token_count(self, text: str) -> int:
        if not isinstance(text, str):
            raise TypeError(f"_token_count expects a string, got {type(text).__name__}")
        tokens = self.tokenizer.encode(text)
        logging.debug(f"Token count for given text: {len(tokens)}, token data type: {type(tokens)}")
        print(f"Token count for given text: {len(tokens)}, token data type: {type(tokens)}")
        return len(tokens)