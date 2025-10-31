# screen_capture.py
import mss
import numpy as np
import cv2  # 1. Importe o OpenCV

def capture_screen(region=None):
    """
    Captura a tela inteira (ou região) e devolve um array NumPy (H, W, 3) em BGR.
    
    Args:
        region: dict com keys 'top', 'left', 'width', 'height' ou None para todo o desktop.
                
    Returns:
        np.ndarray (uint8) – imagem BGR de 3 canais
    """
    with mss.mss() as sct:
        monitor = region or sct.monitors[1]
        img_rgba = sct.grab(monitor)
        
        # 2. Converta a imagem de BGRA (4 canais) para BGR (3 canais)
        img_bgr = cv2.cvtColor(np.array(img_rgba), cv2.COLOR_BGRA2BGR)
        
        return img_bgr # 3. Retorne a imagem convertida

# Exemplo de uso
if __name__ == "__main__":
    # O teste agora mostrará 3 canais
    arr = capture_screen()
    print(arr.shape) # Saída esperada: (..., ..., 3)