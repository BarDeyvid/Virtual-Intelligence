# main_frontend.py
import sys
import time
from PyQt6.QtWidgets import (QApplication, QWidget, QLabel, QVBoxLayout,
                             QPushButton, QLineEdit, QMainWindow)
from PyQt6.QtCore import QObject, QThread, pyqtSignal

from toast import Toast

# Importe o cliente da API
from api_client import ApiClient


# --- Worker Thread para evitar que a GUI congele ---
class Worker(QObject):
    """
    Executa uma tarefa em uma thread separada.
    """
    finished = pyqtSignal(object)  # Sinal emitido com o resultado da tarefa

    def __init__(self, func, *args, **kwargs):
        super().__init__()
        self.func = func
        self.args = args
        self.kwargs = kwargs

    def run(self):
        result = self.func(*self.args, **self.kwargs)
        self.finished.emit(result)


# --- Janela Principal da Aplicação ---
class ChatWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Alyssa Chat - Texto")
        self.setGeometry(100, 100, 400, 200)

        self.api_client = ApiClient()
        self.character_selected = False
        self._running = [] # List to hold references to running threads/workers

        # --- Widgets da Interface ---
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        layout = QVBoxLayout(self.central_widget)

        self.label = QLabel("Digite sua mensagem e pressione Enter ou clique em Enviar.")
        self.input_field = QLineEdit()
        self.input_field.returnPressed.connect(self.send_message)
        self.send_button = QPushButton("Enviar")
        self.send_button.clicked.connect(self.send_message)

        layout.addWidget(self.label)
        layout.addWidget(self.input_field)
        layout.addWidget(self.send_button)
        
        # Start the initialization process
        self.start_session()

    # --- REFACTORED: Unified method to start any worker ---
    def _start_worker(self, fn, on_finish, *args, **kwargs):
        """
        Creates, starts, and stores a QThread and Worker for any function.
        
        Args:
            fn: The function to run in the worker thread (e.g., api_client call).
            on_finish: The callback method to execute with the result when done.
            *args: Positional arguments for fn.
            **kwargs: Keyword arguments for fn.
        """
        thread = QThread()
        worker = Worker(fn, *args, **kwargs)
        worker.moveToThread(thread)

        # Connect the worker's finished signal to our cleanup/callback method
        worker.finished.connect(lambda result: self._on_worker_finished(worker, thread, on_finish, result))
        
        thread.started.connect(worker.run)
        thread.start()

        # Keep a reference to prevent garbage collection
        self._running.append((thread, worker))

    # --- NEW: Unified cleanup and callback handler ---
    def _on_worker_finished(self, worker, thread, on_finish_callback, result):
        """
        Handles cleanup and forwards the result to the specific callback.
        """
        # Let the thread finish cleanly
        thread.quit()
        thread.wait()

        # Call the specific callback function with the result
        if on_finish_callback:
            on_finish_callback(result)
        
        # Now that it's finished, remove it from the list of running threads
        self._running = [(t, w) for t, w in self._running if w is not worker]

        # Optional: Explicitly schedule deletion to be tidy
        worker.deleteLater()
        thread.deleteLater()

    # --- UPDATED to use _start_worker ---
    def start_session(self):
        """Inicia a sessão: primeiro reseta, depois seleciona o personagem."""
        self.send_button.setEnabled(False)
        self.send_button.setText("Conectando...")
        self._start_worker(self.api_client.reset_session, self.on_reset_complete)

    def on_reset_complete(self, success: bool):
        """Callback chamado após a tentativa de reset."""
        if success:
            # If reset worked, proceed to select the character
            self.select_initial_character("Alyssa")
        else:
            self.show_toast("Falha ao conectar com o servidor.", 5000)
            self.send_button.setText("Erro de conexão")

    # --- UPDATED to use _start_worker ---
    def select_initial_character(self, name: str):
        """Seleciona o personagem inicial (chamado após o reset)."""
        self.send_button.setText("Carregando personagem...")
        self._start_worker(self.api_client.select_character, self.on_character_selected, name)

    def on_character_selected(self, result: dict):
        """Callback para quando a seleção do personagem terminar."""
        if result and "character_name" in result:
            self.character_selected = True
            self.show_toast(f"Personagem '{result['character_name']}' carregado!", 3000)
            self.clear_toast()
            self.send_button.setText("Enviar")
            self.send_button.setEnabled(True)
        else:
            self.show_toast("Falha ao carregar personagem.", 5000)
            self.send_button.setText("Erro")

    # --- UPDATED to use _start_worker ---
    def send_message(self):
        user_text = self.input_field.text()
        if not user_text or not self.character_selected:
            return

        self.input_field.clear()
        self.send_button.setEnabled(False)
        self.show_toast("Gerando resposta...", 2000)

        # Call the worker using the unified method
        self._start_worker(self.api_client.get_text_response, self.handle_text_response, user_text)

    def handle_text_response(self, result: dict):
        """Recebe o dicionário JSON da API e exibe o texto no Toast."""
        if result and "response_text" in result:
            response_text = result["response_text"]
            self.show_toast(response_text, duration=6000)
        else:
            self.show_toast("Erro: Não foi possível obter a resposta de texto.", 4000)

        self.send_button.setEnabled(True)
        
    def show_toast(self, message, duration=3000):
        """Exibe uma notificação Toast na tela."""
        start_time = time.time()
        toast = Toast(text=message, duration_ms=duration, parent=self)
        toast.show()
        finish_time = time.time() - start_time
        #clear previous toast
        if finish_time > duration:
            self.clear_toast()

    def clear_toast(self):
        """Limpa qualquer notificação Toast existente."""
        # Implementação simples: cria um toast vazio que desaparece rapidamente
        toast = Toast(text="", duration_ms=100, parent=self)
        toast.show()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ChatWindow()
    window.show()
    sys.exit(app.exec())
