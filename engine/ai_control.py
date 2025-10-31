# ai_control.py  – versão “real”

import asyncio
from typing import Dict, Any, Optional

from engine.vision_module import VisionModule
from engine.pc_controller import PCController
from models.alyssa_brain import Brain      # importa a classe do LLM


class AIControl:
    def __init__(self, brain: Brain, device: str = "cpu"):
        """
        `brain`  – instância já inicializada do seu modelo LLM (consciente + subconsciente)
        """
        self.vision = VisionModule(device=device)
        self.pc     = PCController()
        self.brain  = brain

    # ------------------------------------------------------------------
    async def handle_input(
        self,
        user_text: str,
        frame: Optional[Any] = None,          # np.ndarray da webcam
        *,
        allow_pc_actions: bool = True,
    ) -> Dict[str, Any]:
        """
        Retorna:
            {
                "response": str,           # fala gerada pelo LLM
                "vision": dict,            # resultado do VisionModule (pode ser vazio)
                "pc_action": dict | None  # saída de PCController se executado
            }
        """
        vision_res = {}
        pc_result  = None

        # 1️⃣ Se tivermos frame, faça a visão
        if frame is not None:
            vision_res = self.vision.process_frame(frame)

        # ------------------------------------------------------------------
        # 2️⃣ Montar o prompt para o LLM
        #    • Usuário + visão (objetos/gestos/OCR)
        #    • Contexto de memória pode ser adicionado aqui se quiser.
        #
        #    Exemplo simples:
        user_prompt = f"Usuário disse: {user_text}\n"

        if vision_res:
            objs   = ", ".join([f"{o['label']} ({o['confidence']:.2f})"
                                 for o in vision_res.get("objects", [])])
            gests  = ", ".join(vision_res.get("gestures", []))
            texts  = " | ".join(vision_res.get("texts", []))

            user_prompt += f"Visão detectada:\n- Objetos: {objs}\n"
            if gests:
                user_prompt += f"- Gestos: {gests}\n"
            if texts:
                user_prompt += f"- Texto OCR: {texts}\n"

        # 3️⃣ Chamar o LLM (assumindo que `brain.generate_conscious_response`
        #     já exista e retorne um dicionário com os campos desejados)
        try:
            llm_output = await self.brain.async_generate_conscious_response(
                user_input=user_text,
                vision_context=vision_res,   # pode ser ignorado pelo LLM se não precisar
                system_prompt=user_prompt
            )
        except Exception as exc:
            return {
                "response": f"Erro interno do LLM: {exc}",
                "vision": vision_res,
                "pc_action": None,
            }

        # 4️⃣ Extrair a resposta textual e, se houver, a ação de PC
        response_text = llm_output.get("Voice", "") or ""
        action_cmd    = llm_output.get("action")          # <--- campo opcional

        if allow_pc_actions and action_cmd:
            pc_result = await self.pc.execute(action_cmd)

        return {
            "response": response_text,
            "vision": vision_res,
            "pc_action": pc_result,
        }
