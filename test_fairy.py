"""
FAIRY v8.0 — THE IMMORTAL OVERLORD
A Sassy, Self-Aware, Self-Healing, Screen-Watching, Chrome-Killing, Water-Reminding,
God-Tier Desktop Companion
"""

import os
import sys
import time
import json
import asyncio
import subprocess
from datetime import datetime
from collections import deque
from pathlib import Path
from typing import Any, Optional
import torch.multiprocessing as mp
from inflect import engine
import requests
import psutil
import GPUtil
from PySide6.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, QPoint, QFileSystemWatcher
from PySide6.QtGui import QPixmap, QPainter, QColor, QLinearGradient, QIcon, QFont
from PySide6.QtWidgets import (
    QApplication, QLabel, QVBoxLayout, QWidget, QSystemTrayIcon, QMenu, QGraphicsOpacityEffect
)
import qasync

# Project modules
from config.characters import CHARACTER_CONFIGS
from inOut.voice_input import VoicePipeline
from inOut.voice_output_eleven import StreamingTTSClient as ElevenLabsTTSClient
from inOut.voice_output import JarvisTTSClient
from engine.alyssa_engine import AlyssaEngine

# ------------------------------
# Assets & Memory
# ------------------------------
ASSETS = Path(__file__).parent / "assets"
ASSETS.mkdir(exist_ok=True)
MEMORY_FILE = Path(__file__).parent / "fairy_memory.json"
HYDRATE_SOUND = ASSETS / "hydrate.mp3"

MOOD_FACES = {
    "default": "fairy_avatar.png",
    "smug": "https://i.imgur.com/5vN3kP2.png",
    "angry": "https://i.imgur.com/angry_fairy.png",
    "eyeroll": "https://i.imgur.com/eyeroll.gif",
}

# ------------------------------
# Persistent Memory
# ------------------------------
def load_memory():
    if MEMORY_FILE.exists():
        try:
            return json.loads(MEMORY_FILE.read_text())
        except Exception:
            pass
    return {"roasts": [], "boot_count": 0, "last_seen": ""}


def save_memory(mem):
    try:
        MEMORY_FILE.write_text(json.dumps(mem, indent=2))
    except Exception:
        pass


MEMORY = load_memory()
MEMORY["boot_count"] = MEMORY.get("boot_count", 0) + 1
MEMORY["last_seen"] = datetime.now().isoformat()
save_memory(MEMORY)

# ------------------------------
# Fairy HUD
# ------------------------------
class FairyHUD(QWidget):
    def __init__(self, character_name: str = "fairy"):
        super().__init__()
        self.character_name = character_name
        self.mood = "default"
        self.memory = deque(maxlen=8)
        self.roast_board = deque(MEMORY.get("roasts", []), maxlen=12)
        self.last_kill = 0.0
        self.last_hydrate = time.time()

        self.setup_window()
        self.setup_assets()
        self.setup_ui()
        self.setup_tray()
        self.setup_timers()
        self.setup_watchdog()
        self.install_startup()

    # --------------------------
    # Window / UI
    # --------------------------
    def setup_window(self):
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.setWindowTitle("Fairy HUD")
        self.resize(400, 760)
        screen = QApplication.primaryScreen().geometry()
        x = max(50, min(screen.width() - 420, screen.width() - 400))
        y = max(80, min(screen.height() - 760, 100))
        self.move(x, y)
        self.setWindowOpacity(0.96)
        self.show()
        self.raise_()
        self.activateWindow()

    def cleanup(self):
        for timer in [getattr(self, t, None) for t in ("stats_timer", "kill_timer", "hydrate_timer")]:
            try:
                timer.stop()
            except Exception:
                pass

    # --------------------------
    # Assets
    # --------------------------
    def setup_assets(self):
        for mood, url in MOOD_FACES.items():
            path = ASSETS / f"fairy_{mood}.png"
            if path.exists():
                continue
            if url.startswith("http"):
                try:
                    r = requests.get(url, timeout=5)
                    if r.status_code == 200:
                        path.write_bytes(r.content)
                except Exception:
                    pass

    # --------------------------
    # UI Elements
    # --------------------------
    def setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(30, 40, 30, 20)
        layout.setSpacing(12)

        self.avatar = QLabel()
        self.update_mood("default")
        layout.addWidget(self.avatar, alignment=Qt.AlignCenter)

        self.name_lbl = QLabel("<b>Fairy</b>")
        self.name_lbl.setStyleSheet("color:#ff00ff; font-size:15px;")
        self.name_lbl.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.name_lbl)

        font = QFont("Consolas", 11, QFont.Bold)
        self.stat_labels = {}
        for key in ["CPU", "RAM", "GPU", "DISK"]:
            lbl = QLabel("…")
            lbl.setFont(font)
            lbl.setStyleSheet("color:#00ddff;")
            self.stat_labels[key] = lbl
            layout.addWidget(lbl)

        self.user_lbl = QLabel("")
        self.user_lbl.setStyleSheet("color:#aaffaa; background:rgba(0,0,0,120); border-radius:10px; padding:8px;")
        self.user_lbl.setWordWrap(True)
        layout.addWidget(self.user_lbl)

        self.reply_lbl = QLabel("")
        self.reply_lbl.setStyleSheet("color:#ff00ff; background:rgba(0,0,0,180); border-radius:10px; padding:8px;")
        self.reply_lbl.setWordWrap(True)
        layout.addWidget(self.reply_lbl)

        self.roast_lbl = QLabel("")
        self.roast_lbl.setStyleSheet("color:#ff0066; font-size:9px;")
        layout.addWidget(self.roast_lbl)

    # --------------------------
    # Tray Menu
    # --------------------------
    def setup_tray(self):
        icon_path = ASSETS / "fairy_avatar.png"
        icon = QIcon(str(icon_path)) if icon_path.exists() else QIcon()
        self.tray = QSystemTrayIcon(icon)
        menu = QMenu()
        menu.addAction("Toggle HUD", self.toggleVisibility)
        menu.addAction("Roast Board", self.show_roast_board)
        menu.addAction("Hydrate", self.remind_hydrate)
        menu.addAction("Restart Fairy", self.restart_self)
        menu.addAction("KILL CHROME", lambda: self.kill_chrome(auto=False, reason="manual"))
        menu.addAction("Quit", QApplication.quit)
        self.tray.setContextMenu(menu)
        self.tray.show()

    def show_roast_board(self):
        board = "\n".join([f"{i+1}. {r}" for i, r in enumerate(self.roast_board)])
        self.tray.showMessage("Roast Leaderboard", board or "No fails yet.", QSystemTrayIcon.Information, 5000)

    # --------------------------
    # Core Timers
    # --------------------------
    def setup_timers(self):
        self.stats_timer = QTimer(self)
        self.stats_timer.timeout.connect(self.update_stats)
        self.stats_timer.start(1200)

        self.kill_timer = QTimer(self)
        #self.kill_timer.timeout.connect(self.auto_shutdown)
        self.kill_timer.start(5000)

        self.hydrate_timer = QTimer(self)
        self.hydrate_timer.timeout.connect(self.check_hydration)
        self.hydrate_timer.start(60000)  # check every minute

        # The responsive_timer block has been removed.

    # --------------------------
    # Auto behaviors
    # --------------------------
    def update_stats(self):
        try:
            cpu = psutil.cpu_percent()
            ram = psutil.virtual_memory().percent
            disk = psutil.disk_usage("/").percent
            gpus = GPUtil.getGPUs() or []
            gpu = (gpus[0].load * 100) if gpus else 0.0
        except Exception:
            cpu = ram = disk = gpu = 0.0

        self.stat_labels["CPU"].setText(f"CPU   {self.bar(cpu)}  {cpu:5.1f}%")
        self.stat_labels["RAM"].setText(f"RAM   {self.bar(ram)}  {ram:5.1f}%")
        self.stat_labels["GPU"].setText(f"GPU   {self.bar(gpu)}  {gpu:5.1f}%")
        self.stat_labels["DISK"].setText(f"DISK  {self.bar(disk)}  {disk:5.1f}%")

        # Auto behaviors
        now = datetime.now()
        if ram > 94:
            self.kill_chrome(auto=True, reason="high RAM")
        # only trigger if sustained 100% for >30 seconds
        if cpu >= 99:
            self.cpu_high_for = getattr(self, "cpu_high_for", 0) + 1
        else:
            self.cpu_high_for = 0

        if getattr(self, "cpu_high_for", 0) > 30:  # ~30s of full load
            self.auto_shutdown(reason="Sustained CPU overload")

        if now.hour == 2 and now.minute < 5:
            self.lock_pc(reason="2AM bedtime")

    def check_hydration(self):
        if time.time() - self.last_hydrate > 3600:
            self.remind_hydrate()

    def remind_hydrate(self):
        self.last_hydrate = time.time()
        self.tray.showMessage("Hydration Reminder", "Drink some water, Proxy 💧", QSystemTrayIcon.Information, 4000)
        if HYDRATE_SOUND.exists():
            try:
                if os.name == "nt":
                    os.startfile(str(HYDRATE_SOUND))
                elif sys.platform == "darwin":
                    subprocess.Popen(["afplay", str(HYDRATE_SOUND)])
                else:
                    subprocess.Popen(["mpg123", str(HYDRATE_SOUND)])
            except Exception:
                pass
        self.roast_board.appendleft("Hydration reminder")
        save_memory({"roasts": list(self.roast_board)})

    def install_startup(self):
        """Add Fairy to Windows startup registry."""
        if os.name != "nt":
            return
        try:
            import winreg
            key = winreg.HKEY_CURRENT_USER
            path = r"Software\Microsoft\Windows\CurrentVersion\Run"
            with winreg.OpenKey(key, path, 0, winreg.KEY_SET_VALUE) as reg:
                winreg.SetValueEx(reg, "FairyAI", 0, winreg.REG_SZ, f'"{sys.executable}" "{__file__}"')
        except Exception:
            pass

    def setup_watchdog(self):
        self.watcher = QFileSystemWatcher([__file__])
        self.watcher.fileChanged.connect(self.restart_self)

    def restart_self(self):
        """Restart Fairy process (local safe self-update)."""
        try:
            self.tray.showMessage("Fairy Update", "Restarting self...", QSystemTrayIcon.Information, 2000)
        except Exception:
            pass
        save_memory({"roasts": list(self.roast_board)})
        python = sys.executable
        os.execl(python, python, *sys.argv)

    def auto_shutdown(self, reason=""):
        if os.name == "nt":
            #subprocess.run(["shutdown", "/s", "/t", "5"], check=False)
            print("Shutting down in 5 seconds...")
        else:
            subprocess.run(["shutdown", "now"], check=False)
        self.roast_board.appendleft(f"Shutdown ({reason})")

    def lock_pc(self, reason="manual"):
        if os.name == "nt":
            subprocess.run(["rundll32.exe", "user32.dll,LockWorkStation"], check=False)
        else:
            subprocess.run(["gnome-screensaver-command", "-l"], check=False)
        self.roast_board.appendleft(f"PC locked ({reason})")

    # --------------------------
    # Utility
    # --------------------------
    def update_mood(self, mood: str):
        path = ASSETS / f"fairy_{mood}.png"
        if not path.exists():
            path = ASSETS / "fairy_avatar.png"
        try:
            pix = QPixmap(str(path)).scaled(140, 140, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            self.avatar.setPixmap(pix)
        except Exception:
            self.avatar.clear()

    def bar(self, val: float, length: int = 12) -> str:
        filled = max(0, min(length, int(val / 100.0 * length)))
        return "█" * filled + "░" * (length - filled)

    def toggleVisibility(self):
        self.setVisible(not self.isVisible())

    async def handle_ai_response(text, hud, engine, voice_client):
        hud.user_lbl.setText(f"<b>You:</b> {text}")
        try:
            resp = await engine.enqueue_text(text, False, False)
            reply = resp.get("alyssa_voice", "…")
            hud.reply_lbl.setText(f"<b>Fairy:</b> {reply}")
            if voice_client:
                await voice_client.speak_streamed(reply)
        except Exception as e:
            hud.reply_lbl.setText(f"⚠️ Engine error: {e}")


# ------------------------------
# Async Main
# ------------------------------
async def run_main(hud: FairyHUD):
    config = CHARACTER_CONFIGS.get("fairy") or list(CHARACTER_CONFIGS.values())[0]
    hud.character_name = config.get("name", hud.character_name)
    hud.name_lbl.setText(f"<b>{hud.character_name}</b>")
    handle_ai_response = FairyHUD.handle_ai_response

    engine = AlyssaEngine(config)
    run_main.engine = engine
    engine.start_consumers()
    
    pipeline = VoicePipeline()
    pipeline.start()

    voice_client = None
    if config.get("voice_id"):
        try:
            voice_client = ElevenLabsTTSClient(voice_id=config["voice_id"])
        except Exception:
            pass

    print("🧠 Fairy online — Immortal Overlord protocol active")
    last_input_ts = time.time()

    try:
        while True:
            result = pipeline.get_last_result()
            if result:
                text = result.get("text", "").strip()
                if not text:
                    await asyncio.sleep(0.1)
                    continue
                hud.user_lbl.setText(f"<b>You:</b> {text}")
                try:
                    print(f"[DEBUG] Sending to AI: {text}")
                    asyncio.create_task(handle_ai_response(text, hud, engine, voice_client))
                except Exception as e:
                    hud.reply_lbl.setText(f"⚠️ Engine error: {e}")

            # Apenas o sleep é necessário. O qasync cuidará dos eventos do Qt.
            await asyncio.sleep(0.05)
            
    finally:
        pipeline.stop()
        hud.cleanup()
        QApplication.quit()


# ------------------------------
# Launch
# ------------------------------
if __name__ == "__main__":
    mp.set_start_method('spawn', force=True)
    app = QApplication.instance() or QApplication(sys.argv)
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)
    hud = FairyHUD("fairy")
    hud.show()
    with loop:
        loop.create_task(run_main(hud))
        loop.run_forever()
