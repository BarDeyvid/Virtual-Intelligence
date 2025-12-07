import requests
import json

BASE_URL = "http://localhost:8181"

def test_api():
    print("=== Testando Alyssa API ===\n")
    
    # 1. Health Check
    print("1. Health Check:")
    response = requests.get(f"{BASE_URL}/health")
    print(f"Status: {response.status_code}")
    print(f"Resposta: {response.json()}\n")
    
    # 2. Inicializar (ajuste o path do modelo)
    print("2. Inicializando modelo:")
    init_data = {"base_model_path": "models/gemma-3-1b-it-q4_0.gguf"}
    try:
        response = requests.post(f"{BASE_URL}/initialize", json=init_data)
        print(f"Status: {response.status_code}")
        print(f"Resposta: {response.json()}\n")
    except Exception as e:
        print(f"Erro na inicialização: {e}\n")
    
    # 3. Think simples
    print("3. Think simples:")
    think_data = {"input": "Olá, qual é o seu nome?"}
    response = requests.post(f"{BASE_URL}/think", json=think_data)
    print(f"Status: {response.status_code}")
    print(f"Resposta: {response.json()}\n")
    
    # 4. Think com fusão
    print("4. Think com fusão:")
    fusion_data = {"input": "Explique machine learning de forma simples"}
    response = requests.post(f"{BASE_URL}/think/fusion", json=fusion_data)
    print(f"Status: {response.status_code}")
    print(f"Resposta: {response.json()}\n")
    
    # 5. Think com expert específico
    print("5. Think com expert emocional:")
    expert_data = {
        "input": "Estou muito feliz hoje!",
        "expert_id": "emotionalModel"
    }
    response = requests.post(f"{BASE_URL}/think", json=expert_data)
    print(f"Status: {response.status_code}")
    print(f"Resposta: {response.json()}\n")

if __name__ == "__main__":
    test_api()