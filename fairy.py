import os
import sys
import time
import asyncio
import subprocess
from collections import deque
from pathlib import Path
from typing import Any, Optional

import requests
import psutil
import GPUtil
from PySide6.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, QPoint
from PySide6.QtGui import QPixmap, QPainter, QColor, QLinearGradient, QIcon, QFont
from PySide6.QtWidgets import (
    QApplication, QLabel, QVBoxLayout, QWidget, QSystemTrayIcon, QMenu, QGraphicsOpacityEffect
)
import qasync

# Project modules (assumes same package layout as your repo)
from config.characters import CHARACTER_CONFIGS
from inOut.voice_input import VoicePipeline
from inOut.voice_output_eleven import StreamingTTSClient as ElevenLabsTTSClient
from inOut.voice_output import JarvisTTSClient
from engine.alyssa_engine import AlyssaEngine

# ------------------------------
# Assets & mood faces
# ------------------------------
ASSETS = Path(__file__).parent / "assets"
ASSETS.mkdir(exist_ok=True)
FAIRY_URL = "https://static.wikia.nocookie.net/zenless-zone-zero/images/7/72/NPC_Fairy.png/revision/latest"
MOOD_FACES = {
    "default": "fairy_avatar.png",
    "smug": "https://i.imgur.com/5vN3kP2.png",
    "angry": "https://i.imgur.com/angry_fairy.png",
    "eyeroll": "https://i.imgur.com/eyeroll.gif"
}


# ------------------------------
# Fairy HUD (modernized)
# ------------------------------
class FairyHUD(QWidget):
    def __init__(self, character_name: str = "fairy"):
        super().__init__()
        self.character_name = character_name
        self.mood = "default"
        self.memory = deque(maxlen=8)
        self.roast_board = deque(maxlen=8)
        self.last_kill = 0.0

        self.setup_window()
        self.setup_assets()
        self.setup_ui()
        self.setup_tray()
        self.setup_timers()

    def setup_window(self):
        # Safe, always-visible window setup
        self.setWindowFlags(
            Qt.FramelessWindowHint
            | Qt.WindowStaysOnTopHint
            | Qt.Tool            # make it appear above, but not steal focus
        )
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.setWindowTitle("Fairy HUD")

        # Window size
        self.resize(400, 760)
        screen = QApplication.primaryScreen().geometry()

        # Ensure it's visible on screen (fallback to 100,100 if needed)
        x = max(50, min(screen.width() - 420, screen.width() - 400))
        y = max(80, min(screen.height() - 760, 100))
        self.move(x, y)

        # Show with guaranteed visibility
        self.setWindowOpacity(0.96)
        self.show()
        self.raise_()
        self.activateWindow()

    
    def cleanup(self):
        """Stop all QTimers before quitting to avoid qasync timerEvent KeyError."""
        try:
            self.stats_timer.stop()
            self.kill_timer.stop()
        except Exception:
            pass

    def setup_assets(self):
        # Download mood images if urls provided
        for mood, url in MOOD_FACES.items():
            path = ASSETS / f"fairy_{mood}.png"
            if path.exists():
                continue
            # local default for "default" is fairy_avatar.png
            if mood == "default":
                default = ASSETS / "fairy_avatar.png"
                if default.exists():
                    continue
            if url.startswith("http"):
                try:
                    r = requests.get(url, timeout=5)
                    if r.status_code == 200 and r.content:
                        path.write_bytes(r.content)
                except Exception:
                    # ignore download errors, continue gracefully
                    pass

        # ensure default avatar exists
        avatar = ASSETS / "fairy_avatar.png"
        if not avatar.exists():
            try:
                r = requests.get(FAIRY_URL, timeout=5)
                if r.status_code == 200 and r.content:
                    avatar.write_bytes(r.content)
            except Exception:
                # silent fail — UI will handle missing pixmap
                pass

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

        self.thinking_lbl = QLabel("")
        self.thinking_lbl.setStyleSheet("color:#ff66ff; font-style:italic; font-size:10px;")
        layout.addWidget(self.thinking_lbl)

        self.roast_lbl = QLabel("")
        self.roast_lbl.setStyleSheet("color:#ff0066; font-size:9px;")
        layout.addWidget(self.roast_lbl)

        self.fade_in()

    def update_mood(self, mood: str):
        self.mood = mood
        path = ASSETS / f"fairy_{mood}.png"
        if not path.exists():
            path = ASSETS / "fairy_avatar.png"
        try:
            pix = QPixmap(str(path)).scaled(140, 140, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            self.avatar.setPixmap(pix)
        except Exception:
            # leave blank if pixmap fails
            self.avatar.clear()

    def fade_in(self):
        """Cinematic fade-in: Fairy materializes with a glow pulse."""
        # Start fully transparent and slightly scaled
        self.setWindowOpacity(0.0)
        self.show()
        # Slide up from below a little bit
        start_pos = self.pos() + QPoint(0, 60)
        self.move(start_pos)
        slide = QPropertyAnimation(self, b"pos", self)
        slide.setDuration(1500)
        slide.setStartValue(start_pos)
        slide.setEndValue(self.pos() - QPoint(0, 60))
        slide.setEasingCurve(QEasingCurve.OutCubic)
        slide.start(QPropertyAnimation.DeleteWhenStopped)

        self.raise_()

        # First stage: soft fade from 0 → 1.0
        anim = QPropertyAnimation(self, b"windowOpacity", self)
        anim.setDuration(2200)
        anim.setStartValue(0.0)
        anim.setEndValue(1.0)
        anim.setEasingCurve(QEasingCurve.OutCubic)

        # Second stage: brief pulse glow using QGraphicsOpacityEffect
        effect = QGraphicsOpacityEffect(self)
        self.setGraphicsEffect(effect)
        pulse = QPropertyAnimation(effect, b"opacity", self)
        pulse.setDuration(1400)
        pulse.setStartValue(0.8)
        pulse.setEndValue(1.0)
        pulse.setEasingCurve(QEasingCurve.OutQuad)

        # Chain them with timers (so she fades, then glows)
        QTimer.singleShot(500, pulse.start)
        anim.start(QPropertyAnimation.DeleteWhenStopped)

        # After fade completes, normalize opacity
        def settle():
            self.setWindowOpacity(0.96)
            self.setGraphicsEffect(None)

        QTimer.singleShot(2600, settle)


    def setup_tray(self):
        icon_path = ASSETS / "fairy_avatar.png"
        icon = QIcon(str(icon_path)) if icon_path.exists() else QIcon()
        self.tray = QSystemTrayIcon(icon)
        menu = QMenu()
        menu.addAction("Toggle HUD", self.toggleVisibility)
        menu.addAction("Roast Board", self.show_roast_board)
        menu.addAction("KILL CHROME", lambda: self.kill_chrome(auto=False, reason="manual"))
        menu.addAction("Quit", QApplication.quit)
        self.tray.setContextMenu(menu)
        self.tray.show()

    def show_roast_board(self):
        board = "\n".join([f"{i+1}. {r}" for i, r in enumerate(self.roast_board)])
        self.tray.showMessage("Roast Leaderboard", board or "No fails yet.", QSystemTrayIcon.Information, 5000)

    def setup_timers(self):
        self.stats_timer = QTimer(self)
        self.stats_timer.timeout.connect(self.update_stats)
        self.stats_timer.start(1200)

        self.kill_timer = QTimer(self)
        self.kill_timer.timeout.connect(self.auto_kill)
        self.kill_timer.start(5000)

        # keep UI responsive even under heavy async load
        self.responsive_timer = QTimer(self)
        self.responsive_timer.timeout.connect(QApplication.processEvents)
        self.responsive_timer.start(100)
        print("✅ Stats timer active:", self.stats_timer.isActive())

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

        # auto kill chrome if RAM critically high (throttle to once per 30s)
        if ram > 92.0 and (time.time() - self.last_kill) > 30.0:
            self.kill_chrome(auto=True, reason="high RAM")

    def auto_kill(self):
        # if too many chrome processes, kill
        try:
            procs = [p for p in psutil.process_iter(['name']) if p.info['name'] and 'chrome' in p.info['name'].lower()]
            if len(procs) > 20 and (time.time() - self.last_kill) > 10:
                self.kill_chrome(auto=True, reason="20+ Chrome processes")
        except Exception:
            pass

    def kill_chrome(self, auto: bool = False, reason: str = "manual"):
        """Attempt to terminate Chrome processes. Safe: fallbacks and throttling."""
        now = time.time()
        if now - self.last_kill < 10.0:
            return
        self.last_kill = now

        killed = False
        try:
            if os.name == "nt":
                # windows
                subprocess.run(["taskkill", "/F", "/IM", "chrome.exe"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                killed = True
            else:
                # try to terminate chrome/chromium processes via psutil
                for p in psutil.process_iter(['name']):
                    try:
                        name = (p.info.get('name') or "").lower()
                        if 'chrome' in name or 'chromium' in name:
                            p.kill()
                            killed = True
                    except Exception:
                        continue
        except Exception:
            # ignore errors; we don't want the UI to crash
            killed = False

        if killed:
            self.roast_board.appendleft(f"Chrome killed ({reason})")
            self.update_mood("smug")
            QTimer.singleShot(2000, lambda: self.update_mood("default"))
            msg = f"Chrome terminated ({reason})." if auto else f"Chrome terminated. {reason}."
            try:
                self.tray.showMessage("EXECUTED", msg, QSystemTrayIcon.Critical, 3000)
            except Exception:
                pass
        else:
            # add a little roast indicating failure
            self.roast_board.appendleft("Failed to kill Chrome")
            try:
                self.tray.showMessage("FAILED", "Could not terminate Chrome processes.", QSystemTrayIcon.Warning, 2500)
            except Exception:
                pass

    def bar(self, val: float, length: int = 12) -> str:
        filled = max(0, min(length, int(val / 100.0 * length))) if isinstance(val, (float, int)) else 0
        return "█" * filled + "░" * (length - filled)

    def show_user(self, text: str):
        clean = (text or "").strip()
        if not clean:
            return
        if clean in self.memory:
            self.roast_board.appendleft("Repeated input")
            self.update_mood("eyeroll")
            QTimer.singleShot(1500, lambda: self.update_mood("default"))
        else:
            self.memory.append(clean)
        self.user_lbl.setText(f"<b>You:</b> {clean}")

    def show_reply(self, text: str):
        self.reply_lbl.setText(f"<b>Fairy:</b> {text}")
        self.reply_lbl.adjustSize()
        QTimer.singleShot(15000, self.reply_lbl.clear)

    def show_thinking(self, text: str):
        self.thinking_lbl.setText(f"<i>{text}</i>")
        QTimer.singleShot(4000, self.thinking_lbl.clear)

    def toggleVisibility(self):
        self.setVisible(not self.isVisible())

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        grad = QLinearGradient(0, 0, 0, self.height())
        grad.setColorAt(0, QColor(255, 0, 255, 220))
        grad.setColorAt(1, QColor(100, 0, 150, 220))
        p.setBrush(grad)
        p.setPen(Qt.NoPen)
        p.drawRoundedRect(self.rect().adjusted(8, 8, -8, -8), 30, 30)
        p.setBrush(QColor(20, 0, 40, 240))
        p.drawRoundedRect(self.rect().adjusted(20, 20, -20, -20), 25, 25)


# ------------------------------
# Main async loop
# ------------------------------
async def run_main(hud: FairyHUD):
    config = CHARACTER_CONFIGS.get("fairy") or list(CHARACTER_CONFIGS.values())[0]
    # if config has a name, use it
    hud.character_name = config.get("name", hud.character_name)
    hud.name_lbl.setText(f"<b>{hud.character_name}</b>")
    hud.update()

    engine = AlyssaEngine(config)
    run_main.engine = engine
    await engine.start_consumers()

    pipeline = VoicePipeline()
    pipeline.start()

    voice_client: Optional[Any] = None
    if config.get("voice_model_path"):
        voice_client = JarvisTTSClient()
    elif config.get("voice_id"):
        try:
            voice_client = ElevenLabsTTSClient(voice_id=config["voice_id"])
        except Exception:
            voice_client = None

    print("FAIRY — modern loop started\n")
    last_input_ts = time.time()

    try:
        while True:
            result = pipeline.get_last_result()
            if result:
                text = result.get("text", "").strip()
                if not text:
                    await asyncio.sleep(0.1)
                    continue

                hud.show_user(text)
                print(f"You: {text}")

                if text.lower() == "sair":
                    break

                last_input_ts = time.time()
                hud.show_thinking("executing...")
                try:
                    resp = await engine.enqueue_text(text, False, False)
                except Exception as e:
                    resp = {"alyssa_voice": f"[engine error: {e}]"}
                reply = resp.get("alyssa_voice", "…")

                if isinstance(reply, dict):
                    reply = " ".join(str(v) for v in reply.values() if v)
                reply = str(reply).strip()

                # if the AI explicitly asks to kill chrome, do it
                if "kill chrome" in reply.lower():
                    hud.kill_chrome(auto=True, reason="AI command")

                hud.show_reply(reply)
                print(reply)

                if voice_client:
                    try:
                        await voice_client.speak_streamed(reply)
                    except Exception as e:
                        print(f"[TTS error: {e}]")
                        # don't let TTS failures stop the loop
                        pass

            else:
                # idle spontaneous thought
                if time.time() - last_input_ts > getattr(engine, "idle_threshold", 12.0):
                    hud.show_thinking("scanning threats...")
                    try:
                        spont = engine.initiate_spontaneous_thought()
                        thought = spont.get("internal_dialogue", "") if isinstance(spont, dict) else ""
                    except Exception:
                        thought = ""
                    if thought:
                        print(f"[THOUGHT] {thought}")
                        hud.show_reply(thought)
                        last_input_ts = time.time()

            await asyncio.sleep(0)
            QApplication.processEvents()
            await asyncio.sleep(0.05)


    finally:
        try:
            pipeline.stop()
        except Exception:
            pass
        try:
            hud.cleanup()
        except Exception:
            pass
        try:
            loop = asyncio.get_event_loop()
            for task in asyncio.all_tasks(loop):
                task.cancel()
        except Exception:
            pass
        try:
            QApplication.quit()
        except Exception:
            pass



# ------------------------------
# Launch
# ------------------------------
if __name__ == "__main__":
    app = QApplication.instance() or QApplication(sys.argv)
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)

    hud = FairyHUD("fairy")
    hud.show()
    print("HUD position:", hud.pos(), "size:", hud.size(), "opacity:", hud.windowOpacity())

    main_task = loop.create_task(run_main(hud))

    def enable_vision():
        try:
            if hasattr(run_main, "engine") and hasattr(run_main.engine, "consume_vision_queue"):
                asyncio.create_task(run_main.engine.consume_vision_queue())
                print("✅ consume_vision_queue iniciado.")
        except Exception as e:
            print("⚠️ Falha ao iniciar consume_vision_queue:", e)

    QTimer.singleShot(100, enable_vision)

    with loop:
        loop.run_forever()