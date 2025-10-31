"""
Testa o módulo completo (Vision + PC) em modo standalone.

Requisitos:
    - Python 3.10+
    - Webcam conectada ao seu computador
    - As dependências do projeto instaladas (veja README)
"""

import asyncio
import cv2
import sys
from pathlib import Path
import torch
# Ajuste o PYTHONPATH se os módulos não estiverem no mesmo diretório.
# Se você copiou os arquivos acima para uma pasta chamada `ai`, faça:
#   sys.path.append(str(Path(__file__).parent / "ai"))
# Para este exemplo, assumimos que eles estão no mesmo diretório.

from ai_control import AIControl

# ------------------------------------------------------------------
async def capture_one_frame() -> cv2.Mat | None:
    """Captura um único frame da webcam (ou retorna None)."""
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("⚠️  Webcam não disponível.")
        return None
    ret, frame = cap.read()
    cap.release()
    if not ret:
        print("⚠️  Falha ao capturar o frame.")
        return None
    return frame


# ------------------------------------------------------------------
async def main() -> None:
    ctrl = AIControl(device="cuda" if torch.cuda.is_available() else "cpu")

    # 1️⃣ Capture um frame da câmera
    frame = await capture_one_frame()
    if frame is None:
        sys.exit(1)

    # 2️⃣ Pergunte algo ao usuário (ou use texto fixo)
    user_text = input("\n💬 Digite o que você quer dizer à IA (ex.: 'apagar'):\n> ")

    # 3️⃣ Chame a API do controle
    result = await ctrl.handle_input(
        user_text=user_text,
        frame=frame,          # pode ser None se não quiser usar visão
        allow_pc_actions=True # permite que a IA execute comandos de PC
    )

    # 4️⃣ Exibe tudo que aconteceu
    print("\n=== Resultado ===")
    print("Resposta da IA:", result["response"])
    if result["vision"]:
        print("\n🔍 Visão detectada:")
        for k, v in result["vision"].items():
            print(f"   {k}: {v}")

    if result["pc_action"]:
        print("\n🖥️ Ação de PC executada:", result["pc_action"])


# ------------------------------------------------------------------
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nEncerrado pelo usuário.")
