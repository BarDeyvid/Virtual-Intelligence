# alyssa_engine.py

import logging
import time
from typing import Optional, Dict, List, Any
import torch
import asyncio
import json
import cv2
import httpx
import base64
# ------------------------------------------------------------------
# 3rd‑party imports (lazy‑loaded where possible)
# ------------------------------------------------------------------
try:
    from transformers import pipeline
except Exception:  # pragma: no cover – just to keep the file importable without HF libs
    pipeline = None  # type: ignore[assignment]

# ------------------------------------------------------------------
# Local imports
# ------------------------------------------------------------------
from models.alyssa_brain import Brain      # classe que encapsula o LLM
from memory.alyssa_memory import MemorySystem

# TODO: Atualmente se eu falo enquanto a Alyssa está processando a ultima entrada, ela ignora minha fala. preciso corrigir isso futuramente. Talvez fazer uma fila de inputs. e ela cancelar o processamento antigo.
class AlyssaEngine:
    """
    Núcleo de execução da IA.
    Responsável por orquestrar entradas, processamentos com o AlyssaBrain e saídas controladas.
    """
    # --------------------------------------------------------------
    # 1 – Construtor (carrega tudo que não depende de GPU)
    # --------------------------------------------------------------
    def __init__(self, character_config: Dict[str, Any], vision_queue: Optional[asyncio.Queue] = None):
        """
        Inicializa o Engine com a configuração de um personagem específico.
        """
        self.name = character_config.get("name", "Unknown Character")
        self.vision_queue = vision_queue
        # no __init__ – antes de criar os objetos já existentes
        self.ctx_limit = character_config.get("context_token_limit", 4000)
        conscious_model_path = character_config["conscious_model_path"]
        subconscious_model_path = character_config["subconscious_model_path"]
        system_prompt = character_config["system_prompt"]
        subconscious_prompt = character_config["subconscious_prompt"]
        db_path = character_config["db_path"]
        
        # Brain: carrega os modelos LLM
        self.brain = Brain(
            conscious_model_path,
            subconscious_model_path,
            system_prompt,
            character_config,
            subconscious_prompt,
        )

        # Nova linha
        self.memory = MemorySystem(db_path=db_path)

        self.input_queue: asyncio.Queue[str] = asyncio.Queue()
        #asyncio.create_task(self._consume_input_queue())

        # Logger – apenas erros e críticos no nível padrão, mas
        # podemos mudar dinamicamente se precisarmos de mais verbosidade.
        logging.basicConfig(
            level=logging.CRITICAL,
            format="%(asctime)s - %(levelname)s - %(message)s",
        )

        # Estado interno
        self.last_input_time = time.time()
        self.dominant_emotion: Dict[str, float] = {"neutral": 1.0}
        self.emotion_history: List[Dict[str, float]] = []
        self.emotion_history_length = 5
        self.idle_threshold = 300
        self.internal_dialogue_iterations = 3

        # Lazy‑load do pipeline de emoção (Twitter RoBERTa)
        self._emotion_pipe = None


    def start_consumers(self):
        """
        Inicia todos os consumidores de fila em background (texto e visão).
        """
        logging.info("Iniciando consumidor da fila de input...")
        asyncio.create_task(self._consume_input_queue())
        
        if self.vision_queue:
            logging.info("Iniciando consumidor da fila de visão...")
            asyncio.create_task(self.consume_vision_queue())

    # --------------------------------------------------------------
    # 2 – Propriedade “emotion_pipe” (lazy‑loading)
    # --------------------------------------------------------------
    @property
    def emotion_pipe(self):
        """
        Cria o pipeline de classificação de emoções apenas na primeira chamada.
        Se a biblioteca transformers não estiver instalada, lança uma exceção clara.
        """
        if self._emotion_pipe is None:
            if pipeline is None:  # pragma: no cover
                raise ImportError(
                    "transformers não está instalado – instale 'pip install transformers' para usar o modelo de emoções."
                )
            logging.info("Carregando pipeline de emoções Twitter RoBERTa…")
            self._emotion_pipe = pipeline(
                "text-classification",
                model="cardiffnlp/twitter-roberta-base-emotion",
                tokenizer="cardiffnlp/twitter-roberta-base-emotion",
                return_all_scores=True,
                device=0 if torch.cuda.is_available() else -1,  # GPU se houver
            )
        return self._emotion_pipe
    
    # ------------------------------------------------------------------
    #  NEW: Public helper that callers use to send text into the engine
    # ------------------------------------------------------------------
    async def enqueue_text(
        self,
        user_input: str,
        include_internal_dialogue: bool,
        roberta_input: bool
    ) -> Dict[str, Any]:
        """
        Send a user utterance to the engine **and** wait for the response.
        """
        # Create a Future that will hold the result
        future = asyncio.get_running_loop().create_future()
        # Put (user_input, flags, future) into the queue
        await self.input_queue.put((user_input, include_internal_dialogue,
                                    roberta_input, future))
        return await future   # wait until consumer sets the result


    async def _consume_input_queue(self):
        """
        Background coroutine that keeps pulling from the queue and invoking
        `_process_text` for each item.
        """
        while True:
            user_input, inc_int, rob_inp, fut = await self.input_queue.get()
            try:
                response = await self._process_text(
                    user_input,
                    include_internal_dialogue=inc_int,
                    roberta_input=rob_inp
                )
                fut.set_result(response)
            except Exception as exc:
                logging.error(f"[Engine] Error processing input: {exc}")
                fut.set_exception(exc)
            finally:
                self.input_queue.task_done()


    
#    async def _capture_frame(self):
#        """
#        Captura um frame da webcam OU da tela, dependendo de self.vision_source.
#        Retorna um frame ou None se `use_vision` for False.
#        """
#        if not self.use_vision:
#            return None
#
#        loop = asyncio.get_running_loop()
#
#        if self.vision_source == "screen":
#            # Roda a captura de tela (síncrona) em um executor para não bloquear
#            return await loop.run_in_executor(None, capture_screen)
#        else: # Padrão é a câmera
#            # A captura da câmera já é relativamente rápida
#            cap = cv2.VideoCapture(0)
#            success, frame = cap.read()
#            cap.release()
#            return frame if success else None

    # --------------------------------------------------------------
    # 3 – Métodos auxiliares
    # --------------------------------------------------------------
    def update_dominant_emotion(self, current_emotion: Dict[str, float]):
        """Atualiza a lista de histórico e recalcula a emoção dominante ponderada."""
        if not current_emotion:
            return
        self.emotion_history.append(current_emotion)
        if len(self.emotion_history) > self.emotion_history_length:
            self.emotion_history.pop(0)

        weighted_sum: Dict[str, float] = {}
        total_weight = 0.0

        for i, emo_dict in enumerate(self.emotion_history):
            weight = (i + 1) / self.emotion_history_length
            total_weight += weight
            for e, s in emo_dict.items():
                weighted_sum[e] = weighted_sum.get(e, 0.0) + s * weight

        if total_weight > 0:
            self.dominant_emotion = {
                e: score / total_weight for e, score in weighted_sum.items()
            }
        else:
            self.dominant_emotion = {"neutral": 1.0}

        logging.info(f"🎭 Emoção dominante atualizada: {self.dominant_emotion}")


    def initiate_spontaneous_thought(self) -> Dict[str, str]:
        """
        Gera um pensamento espontâneo quando a IA está ociosa.
        O resultado é salvo na memória e retornado para quem chamou.
        """
        logging.info("🧠IA está ociosa. Iniciando pensamento espontâneo…")
        trigger = f"Estou me sentindo {max(self.dominant_emotion, key=self.dominant_emotion.get)} e pensando sobre…"

        internal_history: List[str] = []
        current = trigger
        for i in range(self.internal_dialogue_iterations):
            logging.info(f"🧠 Iteração {i+1} de diálogo interno espontâneo…")
            chunk = self.brain.internal_dialogue_with_subconscious(
                trigger_text=current,
                emotion_state=self.dominant_emotion,
                context_info="\n".join(internal_history) if internal_history else None,
            )
            internal_history.append(chunk)
            current = chunk

        dialogue = "\n".join(internal_history)

        self.memory.save_memory(
            user_input="[Pensamento Espontâneo]",
            alyssa_response="",
            dialogue=dialogue,
            emotion_score=self.dominant_emotion,
            tipo="pensamento_ocioso",
            decision="Reflexão",
            actions=[],
            motor_output="nenhum",
        )

        return {
            "alyssa_voice": "",
            "internal_dialogue": dialogue,
            "emotion": self.dominant_emotion,
            "raw_response": "",
            "decision": "Reflexão",
            "actions": [],
            "motor_output": "nenhum",
        }

    # --------------------------------------------------------------
    # --------------------------------------------------------------
    # 4 – CORE: handle_input (com hot‑reload automático)
    # --------------------------------------------------------------
    # --------------------------------------------------------------

    async def _process_text(
        self,
        user_input: str,
        emotion_state: Optional[Dict[str, float]] = None,
        context_info: Optional[str] = None,
        include_internal_dialogue: bool = True,
        roberta_input: bool = True,
        capture_screen_now: bool = False,
    ) -> Dict[str, Any]:
        """
        Combina visão, PC‑control, emoções, memória e diálogo interno.
        - Conta tokens antes de enviar ao LLM; se > 70 % do limite troca
        para um modelo “pequeno” (ou truncará a memória).
        - Mede tempo de inferência; se > X s recarrega para evitar sobrecarga da GPU.
        """
        self.last_input_time = time.time()

        # ------------------------------------------------------------------
        # 1️⃣ Captura frame (se habilitado)
        # ------------------------------------------------------------------
        #frame = await self._capture_frame() if not capture_screen_now else capture_screen()
        #vision_desc = self.multimodal_description or ""

        # ------------------------------------------------------------------
        # 3️⃣ Inferência de emoção (opcional)
        # ------------------------------------------------------------------
        if roberta_input and not emotion_state:
            logging.info("🎭 Inferindo emoção com twitter‑roberta‑base‑emotion…")
            try:
                raw = self.emotion_pipe(user_input)          # lazy‑loaded
                if raw and isinstance(raw, list):
                    dominant = max(raw, key=lambda x: x["score"])
                    emotion_state = {
                        "nome": dominant["label"],
                        "intensidade": float(dominant["score"]),
                    }
            except Exception as e:
                logging.error(f"❌ Falha ao inferir emoção: {e}")

        # ------------------------------------------------------------------
        # 4️⃣ Monta o *prompt* completo (para contagem de tokens)
        # ------------------------------------------------------------------
        emotion_prefix = "; ".join(
            f"{k}: {round(v, 2)}"
            for k, v in (emotion_state or {}).items()
        )

        # ---- a) Contexto da memória ------------------------------------
        contexto_memoria = self.memory.retrieve_memories(
            query=user_input,
            search_type="keyword",
            limit=3,
        )
        context_parts: List[str] = []
        if contexto_memoria:
            context_parts.append(
                "Memórias passadas relevantes:\n"
                + "\n".join(m.conteudo for m in contexto_memoria)
            )

        # ---- c) Construção de contexto completo -------------------------
        contexto_completo = "\n\n".join(context_parts) if context_parts else None

        # ------------------------------------------------------------------
        # 5️⃣ Diálogo interno (iterativo)
        # ------------------------------------------------------------------
        internal_history: List[str] = []
        if include_internal_dialogue:
            logging.info("🧠 Iniciando diálogo interno iterativo…")
            current_thought = user_input

            for i in range(self.internal_dialogue_iterations):
                logging.info(f"🧠 Iteração {i+1} de diálogo interno…")

                # Contexto que será passado para o subconsciente:
                #  • Se já temos contexto completo (memória + visão), usamos ele.
                #  • Caso contrário, concatenamos a informação original
                #    (`context_info`) com tudo que já geramos no diálogo interno.
                context_for_subconscious = (
                    contexto_completo if contexto_completo else
                    (context_info or "") +
                    ("\n" + "\n".join(internal_history) if internal_history else "")
                )

                chunk = self.brain.internal_dialogue_with_subconscious(
                    trigger_text=current_thought,
                    emotion_state=emotion_state or self.dominant_emotion,
                    context_info=context_for_subconscious,
                )
                internal_history.append(chunk)
                current_thought = chunk

            # Adiciona o diálogo interno ao contexto completo (para ser
            # usado pelo LLM na fase de geração consciente).
            if internal_history:
                context_parts.append(
                    "Diálogo Interno:\n" + "\n".join(internal_history)
                )
                contexto_completo = "\n\n".join(context_parts)

            logging.info("🧠 Diálogo interno gerado.")
        else:
            logging.info("🧠 Diálogo interno ignorado conforme configuração.")

        # ------------------------------------------------------------------
        # 6️⃣ Monta a lista de mensagens (para LLM)
        # ------------------------------------------------------------------
        messages = [
            {
                "role": "system",
                "content": self.brain.system_prompt,
            },
            {
                "role": "user",
                "content": (
                    f"{contexto_completo or ''}\n<start_of_turn>user\n"
                    f"{emotion_prefix}; {contexto_completo or ''}; {user_input}\n<end_of_turn>\n<start_of_turn>model\n"
                ),
            },
        ]

        # ------------------------------------------------------------------
        # 7️⃣ Contagem de tokens – se > 70 % do limite, recarrega
        # ------------------------------------------------------------------
        prompt_text = "".join(m["content"] for m in messages)
        tokens_used = self.brain._token_count(str(prompt_text))

        if tokens_used > float(self.ctx_limit) * 0.7:
            logging.warning(
                f"⚠️ Uso de token alto ({tokens_used}/{self.ctx_limit}). "
                "Recarregando modelo pequeno…"
            )
            #await self._reload_conscious_model_async("models/conscious_small.gguf")

            # Re‑monta mensagens (mesma string) após reload
            prompt_text = "".join(m["content"] for m in messages)
            tokens_used = self.brain._token_count(prompt_text)

        # ------------------------------------------------------------------
        # 8️⃣ Geração consciente – roda em thread para não bloquear
        # ------------------------------------------------------------------
        start_time = time.time()
        try:
            conscious_output: Dict[str, Any] = await asyncio.to_thread(
                self.brain.generate_conscious_response,
                user_input=str(user_input),
                structured_history=None,
                emotion_state=emotion_state or self.dominant_emotion,
                subconscious_suggestion=None,   # será preenchido depois
                context_info=contexto_completo,
                system_directives=" ",
            )
        except Exception as e:
            logging.error(f"❌ Falha ao chamar LLM: {e}")
            return {
                "alyssa_voice": ("Desculpe, algo deu errado na geração da resposta…"),
                "internal_dialogue": "(Falha ao tentar refletir internamente)",
                "emotion": emotion_state or self.dominant_emotion,
                "raw_response": "",
                "decision": "Falha na execução",
                "actions": [],
                "motor_output": "nenhum",
            }
        elapsed = time.time() - start_time

        # ------------------------------------------------------------------
        # 9️⃣ Se a inferência demorou > X segundos, recarrega novamente
        # ------------------------------------------------------------------
        if elapsed > 12:   # ajuste conforme sua GPU / modelo
            logging.warning(f"Resposta demorada ({elapsed:.1f}s). Recarregando modelo pequeno…")
            #await self._reload_conscious_model_async("models/conscious_small.gguf")

        # ------------------------------------------------------------------
        # 10️⃣ Processa o resultado do LLM (JSON já parseado)
        # ------------------------------------------------------------------
        alyssa_voice = conscious_output.get("Voice", "(erro de texto)")
        emotion_alyssa = conscious_output.get(
            "estado_emocional", emotion_state or self.dominant_emotion
        )
        decision = conscious_output.get("decisao", "resposta_generica")
        actions = conscious_output.get("actions", [])
        motor_output = conscious_output.get("motor_output", "nenhum")

        # ------------------------------------------------------------------
        # 11️⃣ Registra o estado emocional (se existir) no DB
        # ------------------------------------------------------------------
        if emotion_alyssa and emotion_alyssa.get("nome"):
            nome_emocao = emotion_alyssa["nome"]
            intensidade = float(emotion_alyssa.get("intensidade", 0.5))
            sess = self.memory.handler._get_session()
            estado_obj = (
                sess.query(self.memory.handler.EstadoEmocional)
                .filter_by(nome=nome_emocao)
                .first()
            )
            if not estado_obj:
                id_estado = self.memory.handler.create_estado_emocional(
                    nome=nome_emocao, intensidade=intensidade
                )
            else:
                id_estado = estado_obj.id_estado
            sess.close()

            if id_estado:
                self.memory.handler.registrar_estado_emocional_atual(id_estado)
                logging.info(f"🎭 Estado emocional registrado: {nome_emocao} (ID:{id_estado})")
            else:
                logging.warning(
                    f"⚠️ Não foi possível registrar estado emocional: {nome_emocao}"
                )

            # Atualiza a emoção dominante ponderada
            self.update_dominant_emotion(emotion_alyssa)

        # ------------------------------------------------------------------
        # 12️⃣ Salva a interação completa na memória
        # ------------------------------------------------------------------
        logging.info("💾 Salvando interação na memória…")
        self.memory.save_memory(
            user_input=user_input,
            alyssa_response=alyssa_voice,
            dialogue="",   # já foi salvo no diálogo interno acima
            emotion_score=emotion_alyssa,
            tipo="interacao",
            decision=decision,
            actions=actions,
            motor_output=motor_output,
        )

        # ------------------------------------------------------------------
        # 13️⃣ Monta resposta final (inclui tudo)
        # ------------------------------------------------------------------
        response: Dict[str, Any] = {
            "alyssa_voice": alyssa_voice,
            "internal_dialogue": "\n".join(internal_history),
            "emotion": emotion_alyssa,
            "raw_response": conscious_output.get("raw_response", ""),
            "decision": decision,
            "actions": actions,
            "motor_output": motor_output,
        }

        return response


    async def handle_input(
        self,
        user_input: str,
        emotion_state: Optional[Dict[str, float]] = None,
        context_info: Optional[str] = None,
        include_internal_dialogue: bool = True,
        roberta_input: bool = True,
        capture_screen_now: bool = False,
    ) -> Dict[str, Any]:
        """
        Combina visão, PC‑control, emoções, memória e diálogo interno.
        - Conta tokens antes de enviar ao LLM; se > 70 % do limite troca
        para um modelo “pequeno” (ou truncará a memória).
        - Mede tempo de inferência; se > X s recarrega para evitar sobrecarga da GPU.
        """
        self.last_input_time = time.time()

        # ------------------------------------------------------------------
        # 3️⃣ Inferência de emoção (opcional)
        # ------------------------------------------------------------------
        if roberta_input and not emotion_state:
            logging.info("🎭 Inferindo emoção com twitter‑roberta‑base‑emotion…")
            try:
                raw = self.emotion_pipe(user_input)          # lazy‑loaded
                if raw and isinstance(raw, list):
                    dominant = max(raw, key=lambda x: x["score"])
                    emotion_state = {
                        "nome": dominant["label"],
                        "intensidade": float(dominant["score"]),
                    }
            except Exception as e:
                logging.error(f"❌ Falha ao inferir emoção: {e}")

        # ------------------------------------------------------------------
        # 4️⃣ Monta o *prompt* completo (para contagem de tokens)
        # ------------------------------------------------------------------
        emotion_prefix = "; ".join(
            f"{k}: {round(v, 2)}"
            for k, v in (emotion_state or {}).items()
        )

        # ---- a) Contexto da memória ------------------------------------
        contexto_memoria = self.memory.retrieve_memories(
            query=user_input,
            search_type="keyword",
            limit=3,
        )
        context_parts: List[str] = []
        if contexto_memoria:
            context_parts.append(
                "Memórias passadas relevantes:\n"
                + "\n".join(m.conteudo for m in contexto_memoria)
            )

        # ---- b) Pergunta sobre visão ------------------------------------
        user_asks_about_vision = any(word in user_input.lower()
                                    for word in ["vê", "tela", "olha", "isso"])

        if user_asks_about_vision and self.last_vision_meta:
            logging.info("👁️ Pergunta sobre visão detectada. Gerando descrição rica…")
            enriched_desc = await self._call_multimodal(self.last_vision_meta)
            context_parts.append(
                f"Contexto visual atual da tela do usuário: {enriched_desc}"
            )

        # ---- c) Construção de contexto completo -------------------------
        contexto_completo = "\n\n".join(context_parts) if context_parts else None

        # ------------------------------------------------------------------
        # 5️⃣ Diálogo interno (iterativo)
        # ------------------------------------------------------------------
        internal_history: List[str] = []
        if include_internal_dialogue:
            logging.info("🧠 Iniciando diálogo interno iterativo…")
            current_thought = user_input

            for i in range(self.internal_dialogue_iterations):
                logging.info(f"🧠 Iteração {i+1} de diálogo interno…")

                # Contexto que será passado para o subconsciente:
                #  • Se já temos contexto completo (memória + visão), usamos ele.
                #  • Caso contrário, concatenamos a informação original
                #    (`context_info`) com tudo que já geramos no diálogo interno.
                context_for_subconscious = (
                    contexto_completo if contexto_completo else
                    (context_info or "") +
                    ("\n" + "\n".join(internal_history) if internal_history else "")
                )

                chunk = self.brain.internal_dialogue_with_subconscious(
                    trigger_text=current_thought,
                    emotion_state=emotion_state or self.dominant_emotion,
                    context_info=context_for_subconscious,
                )
                internal_history.append(chunk)
                current_thought = chunk

            # Adiciona o diálogo interno ao contexto completo (para ser
            # usado pelo LLM na fase de geração consciente).
            if internal_history:
                context_parts.append(
                    "Diálogo Interno:\n" + "\n".join(internal_history)
                )
                contexto_completo = "\n\n".join(context_parts)

            logging.info("🧠 Diálogo interno gerado.")
        else:
            logging.info("🧠 Diálogo interno ignorado conforme configuração.")

        # ------------------------------------------------------------------
        # 6️⃣ Monta a lista de mensagens (para LLM)
        # ------------------------------------------------------------------
        messages = [
            {
                "role": "system",
                "content": vision_desc or self.brain.system_prompt,
            },
            {
                "role": "user",
                "content": (
                    f"{contexto_completo or ''}\n<start_of_turn>user\n"
                    f"{emotion_prefix}; {contexto_completo or ''}; {user_input}\n<end_of_turn>\n<start_of_turn>model\n"
                ),
            },
        ]

        # ------------------------------------------------------------------
        # 7️⃣ Contagem de tokens – se > 70 % do limite, recarrega
        # ------------------------------------------------------------------
        prompt_text = "".join(m["content"] for m in messages)
        tokens_used = self.brain._token_count(str(prompt_text))

        if tokens_used > float(self.ctx_limit) * 0.7:
            logging.warning(
                f"⚠️ Uso de token alto ({tokens_used}/{self.ctx_limit}). "
                "Recarregando modelo pequeno…"
            )
            #await self._reload_conscious_model_async("models/conscious_small.gguf")

            # Re‑monta mensagens (mesma string) após reload
            prompt_text = "".join(m["content"] for m in messages)
            tokens_used = self.brain._token_count(prompt_text)

        # ------------------------------------------------------------------
        # 8️⃣ Geração consciente – roda em thread para não bloquear
        # ------------------------------------------------------------------
        start_time = time.time()
        try:
            conscious_output: Dict[str, Any] = await asyncio.to_thread(
                self.brain.generate_conscious_response,
                user_input=str(user_input),
                structured_history=None,
                emotion_state=emotion_state or self.dominant_emotion,
                subconscious_suggestion=None,   # será preenchido depois
                context_info=contexto_completo,
                system_directives=vision_desc,
            )
        except Exception as e:
            logging.error(f"❌ Falha ao chamar LLM: {e}")
            return {
                "alyssa_voice": ("Desculpe, algo deu errado na geração da resposta…"),
                "internal_dialogue": "(Falha ao tentar refletir internamente)",
                "emotion": emotion_state or self.dominant_emotion,
                "raw_response": "",
                "decision": "Falha na execução",
                "actions": [],
                "motor_output": "nenhum",
            }
        elapsed = time.time() - start_time

        # ------------------------------------------------------------------
        # 9️⃣ Se a inferência demorou > X segundos, recarrega novamente
        # ------------------------------------------------------------------
        if elapsed > 12:   # ajuste conforme sua GPU / modelo
            logging.warning(f"Resposta demorada ({elapsed:.1f}s). Recarregando modelo pequeno…")
            #await self._reload_conscious_model_async("models/conscious_small.gguf")

        # ------------------------------------------------------------------
        # 10️⃣ Processa o resultado do LLM (JSON já parseado)
        # ------------------------------------------------------------------
        alyssa_voice = conscious_output.get("Voice", "(erro de texto)")
        emotion_alyssa = conscious_output.get(
            "estado_emocional", emotion_state or self.dominant_emotion
        )
        decision = conscious_output.get("decisao", "resposta_generica")
        actions = conscious_output.get("actions", [])
        motor_output = conscious_output.get("motor_output", "nenhum")

        # ------------------------------------------------------------------
        # 11️⃣ Registra o estado emocional (se existir) no DB
        # ------------------------------------------------------------------
        if emotion_alyssa and emotion_alyssa.get("nome"):
            nome_emocao = emotion_alyssa["nome"]
            intensidade = float(emotion_alyssa.get("intensidade", 0.5))
            sess = self.memory.handler._get_session()
            estado_obj = (
                sess.query(self.memory.handler.EstadoEmocional)
                .filter_by(nome=nome_emocao)
                .first()
            )
            if not estado_obj:
                id_estado = self.memory.handler.create_estado_emocional(
                    nome=nome_emocao, intensidade=intensidade
                )
            else:
                id_estado = estado_obj.id_estado
            sess.close()

            if id_estado:
                self.memory.handler.registrar_estado_emocional_atual(id_estado)
                logging.info(f"🎭 Estado emocional registrado: {nome_emocao} (ID:{id_estado})")
            else:
                logging.warning(
                    f"⚠️ Não foi possível registrar estado emocional: {nome_emocao}"
                )

            # Atualiza a emoção dominante ponderada
            self.update_dominant_emotion(emotion_alyssa)

        # ------------------------------------------------------------------
        # 12️⃣ Salva a interação completa na memória
        # ------------------------------------------------------------------
        logging.info("💾 Salvando interação na memória…")
        self.memory.save_memory(
            user_input=user_input,
            alyssa_response=alyssa_voice,
            dialogue="",   # já foi salvo no diálogo interno acima
            emotion_score=emotion_alyssa,
            tipo="interacao",
            decision=decision,
            actions=actions,
            motor_output=motor_output,
        )

        # ------------------------------------------------------------------
        # 13️⃣ Monta resposta final (inclui tudo)
        # ------------------------------------------------------------------
        response: Dict[str, Any] = {
            "alyssa_voice": alyssa_voice,
            "internal_dialogue": "\n".join(internal_history),
            "emotion": emotion_alyssa,
            "raw_response": conscious_output.get("raw_response", ""),
            "decision": decision,
            "actions": actions,
            "motor_output": motor_output,
        }

        return response


    # --------------------------------------------------------------
    # 5 – Funções auxiliares de manutenção da memória
    # --------------------------------------------------------------
    def run_memory_maintenance_tasks(self):
        """
        Executa tarefas de manutenção que não precisam ser feitas a cada handle_input.
        Ideal para chamar periodicamente (ex.: por um cron ou loop assíncrono).
        """
        logging.info("⚙️ Iniciando tarefas de manutenção da memória…")
        self.memory.decay_all_memories()
        self.memory.generate_reflections()
        logging.info("✅ Tarefas de manutenção concluídas.")