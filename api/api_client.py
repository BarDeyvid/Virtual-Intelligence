# api_client.py
import httpx
from typing import Dict, Any, Optional

class ApiClient:
    """Um cliente para interagir com a API do personagem."""
    def __init__(self, base_url: str = "http://127.0.0.1:8000"):
        self.base_url = base_url
        
        # --- ALTERAÇÃO AQUI ---
        # Definimos um timeout mais longo: 10s para conectar, 30s total.
        # E o associamos diretamente ao cliente na criação.
        timeout = httpx.Timeout(30.0, connect=10.0)
        self.client = httpx.Client(base_url=self.base_url, timeout=timeout)

    def select_character(self, character_name: str) -> Optional[Dict[str, Any]]:
        """Seleciona um personagem na API."""
        try:
            # A URL relativa já é tratada pelo base_url do cliente
            response = self.client.post(f"/select-character/{character_name}")
            response.raise_for_status()
            return response.json()
        except httpx.RequestError as e:
            print(f"Erro ao selecionar personagem: {e}")
            return None

    def get_text_response(self, user_input: str) -> Optional[Dict[str, Any]]:
        """Obtém uma resposta em texto da API."""
        try:
            payload = {"user_input": user_input}
            response = self.client.post("/interact-text", json=payload, timeout=120.0)
            response.raise_for_status()
            return response.json()
        except httpx.RequestError as e:
            print(f"Erro ao obter resposta de texto: {e}")
            return None

    def get_audio_data(self, user_input: str) -> Optional[bytes]:
        """Obtém os dados de áudio da API."""
        try:
            payload = {"user_input": user_input}
            with self.client.stream("POST", "/interact", json=payload) as response:
                response.raise_for_status()
                audio_content = response.read()
                return audio_content
        except httpx.RequestError as e:
            print(f"Erro ao obter áudio: {e}")
            return None
        
    def reset_session(self) -> bool:
        """Chama o endpoint de reset no servidor."""
        try:
            response = self.client.post("/reset")
            response.raise_for_status()
            print("Sessão no servidor reiniciada com sucesso.")
            return True
        except httpx.RequestError as e:
            print(f"Erro ao reiniciar a sessão: {e}")
            return False