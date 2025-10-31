# toast.py
import sys
from PyQt6.QtWidgets import QApplication, QWidget, QLabel, QVBoxLayout
from PyQt6.QtCore import Qt, QTimer, QPropertyAnimation, QAbstractAnimation
from PyQt6.QtGui import QGuiApplication


class Toast(QWidget):
    def __init__(self, text: str, duration_ms=3000, parent=None):
        super().__init__(parent)
        self.duration_ms = duration_ms

        # Window flags
        self.setWindowFlags(
            Qt.WindowType.ToolTip |
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.X11BypassWindowManagerHint
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        
        # Garante que o widget será deletado ao ser fechado
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)

        # Layout & label
        layout = QVBoxLayout(self)
        lbl = QLabel(text)
        lbl.setStyleSheet("""
            color: white;
            background-color: rgba(30, 30, 30, 200);
            padding: 10px 20px;
            border-radius: 8px;
            font-size: 14pt;
        """)
        layout.addWidget(lbl)

        self.adjustSize()

        # Position bottom‑right
        screen = QGuiApplication.primaryScreen().availableGeometry()
        x = screen.width() - self.width() - 20
        y = screen.height() - self.height() - 40
        self.move(x, y)

    def show(self):
        super().show()
        self.raise_()

        # Fade‑in
        fade_in = QPropertyAnimation(self, b"windowOpacity")
        fade_in.setDuration(200)
        fade_in.setStartValue(0.0)
        fade_in.setEndValue(1.0)
        fade_in.start(QAbstractAnimation.DeletionPolicy.DeleteWhenStopped)

        # Auto‑dismiss
        QTimer.singleShot(self.duration_ms, self.fade_out)

    def fade_out(self):
        fade = QPropertyAnimation(self, b"windowOpacity")
        fade.setDuration(200)
        fade.setStartValue(1.0)
        fade.setEndValue(0.0)
        
        # --- ALTERAÇÃO PRINCIPAL AQUI ---
        # Em vez de apenas fechar (esconder), agendamos a destruição completa do objeto.
        # deleteLater() é mais seguro e garante a limpeza da memória.
        fade.finished.connect(self.deleteLater)
        
        fade.start(QAbstractAnimation.DeletionPolicy.DeleteWhenStopped)


# Demo usage
if __name__ == "__main__":
    app = QApplication(sys.argv)

    # Simula a sua aplicação mostrando uma notificação
    toast = Toast("A resposta do modelo foi gerada com sucesso!", 4000)
    toast.show()

    # O evento de exit do app só acontecerá depois que o Toast for destruído
    sys.exit(app.exec())