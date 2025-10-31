import sys
import asyncio
from PySide6.QtWidgets import QApplication, QLabel, QVBoxLayout, QWidget
from PySide6.QtCore import Qt
import qasync


class FairyHUD(QWidget):
    def __init__(self, name="Alyssa"):
        super().__init__()
        self.setWindowTitle("FairyHUD Test")
        self.setGeometry(400, 300, 400, 200)

        layout = QVBoxLayout()
        self.label = QLabel(f"HUD iniciada: {name}")
        self.label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.label)
        self.setLayout(layout)

    def update_text(self, text: str):
        self.label.setText(text)


async def run_main(hud: FairyHUD):
    counter = 0
    while True:
        hud.update_text(f"Contador: {counter}")
        counter += 1
        await asyncio.sleep(1)


if __name__ == "__main__":
    app = QApplication.instance() or QApplication(sys.argv)
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)

    hud = FairyHUD("Teste")
    hud.show()

    # roda o loop principal assíncrono sem travar a GUI
    loop.create_task(run_main(hud))

    with loop:
        try:
            loop.run_forever()
        except KeyboardInterrupt:
            pass
