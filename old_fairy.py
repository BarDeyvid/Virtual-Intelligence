# ------------------------------------------------------------------
# main_test_fairy.py — versão revisada e sincronizada com AlyssaEngine
# ------------------------------------------------------------------
import os
import sys
import time
import logging
from pathlib import Path

import requests
import psutil
import GPUtil
import httpx
import cv2
import torch
import asyncio
import numpy as np
from collections import deque
from typing import Any, Dict, Optional

from PySide6.QtCore import Qt, QTimer, QPoint
# PySide6 & qasync – only the GUI part needs them at import time
from PySide6.QtGui import (
    QPixmap,
    QPainter,
    QColor,
    QLinearGradient,
    QIcon,
)
from PySide6.QtWidgets import (
    QApplication,
    QLabel,
    QVBoxLayout,
    QWidget,
    QSystemTrayIcon,
    QMenu,
)
import qasync
from config.characters import CHARACTER_CONFIGS
from inOut.voice_input import VoicePipeline
from inOut.voice_output_eleven import StreamingTTSClient as ElevenLabsTTSClient
from inOut.voice_output import JarvisTTSClient
from engine.alyssa_engine import AlyssaEngine

ASSETS = Path(__file__).parent / "assets"
ASSETS.mkdir(exist_ok=True)

FAIRY_URL = (
    "https://static.wikia.nocookie.net/zenless-zone-zero/images/7/72/NPC_Fairy.png/revision/latest"
)


# ------------------------------------------------------------------
# HUD da Fairy
# ------------------------------------------------------------------
class FairyHUD(QWidget):
    def __init__(self, character_name: str):
        super().__init__()
        self.character_name = character_name

        self.setup_window()
        self.setup_assets()
        self.setup_ui()
        self.setup_tray()

        self.stats_timer = QTimer(self)
        self.stats_timer.timeout.connect(self.update_stats)
        self.stats_timer.start(1500)

    def setup_window(self):
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.resize(400, 720)
        screen = QApplication.primaryScreen().geometry()
        self.move(screen.width() - 420, 100)

    def setup_assets(self):
        avatar_path = ASSETS / "fairy_avatar.png"
        if not avatar_path.exists():
            try:
                r = requests.get(FAIRY_URL, timeout=5)
                avatar_path.write_bytes(r.content)
            except Exception:
                pass

    def setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(30, 40, 30, 20)
        layout.setSpacing(12)

        pix = QPixmap(str(ASSETS / "fairy_avatar.png")).scaled(140, 140, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        avatar_lbl = QLabel()
        avatar_lbl.setPixmap(pix)
        avatar_lbl.setAlignment(Qt.AlignCenter)
        layout.addWidget(avatar_lbl, alignment=Qt.AlignCenter)

        name_lbl = QLabel(f"<b>{self.character_name.capitalize()}</b>")
        name_lbl.setStyleSheet("color:#00ffff; font-size:14px; background:transparent;")
        name_lbl.setAlignment(Qt.AlignCenter)
        layout.addWidget(name_lbl)

        self.stat_labels: Dict[str, QLabel] = {}
        for key in ["CPU", "RAM", "GPU", "DISK"]:
            lbl = QLabel("…")
            lbl.setStyleSheet("color:#00ddff; background:transparent;")
            self.stat_labels[key] = lbl
            layout.addWidget(lbl)

        self.user_lbl = QLabel("")
        self.user_lbl.setStyleSheet("color:#aaffaa; background:rgba(0,0,0,100); border-radius:8px; padding:6px;")
        self.user_lbl.setWordWrap(True)
        layout.addWidget(self.user_lbl)

        self.reply_lbl = QLabel("")
        self.reply_lbl.setStyleSheet("color:#00ffff; background:rgba(0,0,0,150); border-radius:8px; padding:6px;")
        self.reply_lbl.setWordWrap(True)
        layout.addWidget(self.reply_lbl)

        self.thinking_lbl = QLabel("")
        self.thinking_lbl.setStyleSheet("color:#8888ff; font-style:italic;")
        layout.addWidget(self.thinking_lbl)

    def setup_tray(self):
        self.tray = QSystemTrayIcon(QIcon(str(ASSETS / "fairy_avatar.png")))
        menu = QMenu()
        menu.addAction("Toggle HUD", self.toggleVisibility)
        menu.addAction("Quit", QApplication.quit)
        self.tray.setContextMenu(menu)
        self.tray.show()

    def update_stats(self):
        cpu = psutil.cpu_percent()
        ram = psutil.virtual_memory().percent
        disk = psutil.disk_usage("/").percent
        gpu = GPUtil.getGPUs()[0].load * 100 if GPUtil.getGPUs() else 0

        self.stat_labels["CPU"].setText(f"CPU   {self.bar(cpu)}  {cpu:5.1f}%")
        self.stat_labels["RAM"].setText(f"RAM   {self.bar(ram)}  {ram:5.1f}%")
        self.stat_labels["GPU"].setText(f"GPU   {self.bar(gpu)}  {gpu:5.1f}%")
        self.stat_labels["DISK"].setText(f"DISK  {self.bar(disk)}  {disk:5.1f}%")

    def bar(self, val, length=12):
        filled = int(val / 100 * length)
        return "█" * filled + "░" * (length - filled)

    def toggleVisibility(self):
        self.setVisible(not self.isVisible())

    def show_user(self, text: str):
        self.user_lbl.setText(f"<b>You:</b> {text}")

    def show_reply(self, text: str):
        self.reply_lbl.setText(f"<b>{self.character_name}:</b> {text}")

    def show_thinking(self, text: str):
        self.thinking_lbl.setText(f"<i>{text}</i>")
        QTimer.singleShot(3500, lambda: self.thinking_lbl.setText(""))

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        grad = QLinearGradient(0, 0, 0, self.height())
        grad.setColorAt(0, QColor(0, 200, 255, 200))
        grad.setColorAt(1, QColor(0, 100, 150, 200))
        p.setBrush(grad)
        p.setPen(Qt.NoPen)
        p.drawRoundedRect(self.rect().adjusted(8, 8, -8, -8), 30, 30)
        p.end()


# ------------------------------------------------------------------
# Loop principal assíncrono
# ------------------------------------------------------------------
async def run_main(hud: FairyHUD):
    print("Selecione um personagem para interagir:")
    for i, name in enumerate(CHARACTER_CONFIGS.keys(), start=1):
        print(f"{i}. {name.capitalize()}")

    chosen = None
    while chosen not in CHARACTER_CONFIGS:
        inp = input("Número ou nome do personagem: ").strip().lower()
        if inp.isdigit() and 1 <= int(inp) <= len(CHARACTER_CONFIGS):
            chosen = list(CHARACTER_CONFIGS.keys())[int(inp) - 1]
        elif inp in CHARACTER_CONFIGS:
            chosen = inp

    config = CHARACTER_CONFIGS[chosen]
    hud.character_name = config["name"]
    hud.update()

    engine = AlyssaEngine(config)
    run_main.engine = engine  # guarda referência global


    pipeline = VoicePipeline()
    pipeline.start()

    voice_client: Optional[Any] = None
    if config.get("voice_model_path"):
        voice_client = JarvisTTSClient()
    elif config.get("voice_id"):
        voice_client = ElevenLabsTTSClient(voice_id=config["voice_id"])

    print(config.get("initial_message", "Digite sair para encerrar.\n"))
    last_input_ts = time.time()

    try:
        while True:
            result = pipeline.get_last_result()
            if result:
                text = result.get("text", "")
                hud.show_user(text)
                print(f"🗣️ {text}")

                if text.lower() == "sair":
                    break

                last_input_ts = time.time()

                resp = await engine.enqueue_text(text, False, False)
                reply = resp.get("alyssa_voice", "…erro…")
                hud.show_reply(reply)
                print(reply)
                print("-" * 60)

                if voice_client:
                    await voice_client.speak_streamed(reply)

            else:
                if time.time() - last_input_ts > engine.idle_threshold:
                    hud.show_thinking("pensando...")
                    spont = engine.initiate_spontaneous_thought()
                    text = spont.get("internal_dialogue", "")
                    print(f"\n💭 {text}")
                    hud.show_reply(text)
                    last_input_ts = time.time()

            await asyncio.sleep(0.1)

    finally:
        pipeline.stop()
        print("✅ encerrado.")


# ------------------------------------------------------------------
# Execução integrada com qasync
# ------------------------------------------------------------------
if __name__ == "__main__":
    app = QApplication.instance() or QApplication(sys.argv)
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)

    hud = FairyHUD("alyssa")
    hud.show()

    loop.create_task(run_main(hud))

    task = loop.create_task(run_main(hud))

    def enable_vision(_):
        loop.create_task(run_main.engine.consume_vision_queue())

    # chama assim que o main_loop realmente iniciar
    QTimer.singleShot(0, enable_vision)

    with loop:
        loop.run_forever()
