# vision_module.py
"""
Vision module – combines object detection, hand‑gesture detection and OCR.
Designed to be lazy‑loaded and lightweight. The public API is simple:
    vm = VisionModule(device='cpu')
    result = vm.process_frame(frame)   # frame: np.ndarray (BGR)
"""

import cv2
import numpy as np
from typing import List, Dict, Any

# YOLOv8
try:
    from ultralytics import YOLO  # pip install ultralytics==8.*
except Exception as exc:
    raise ImportError("ultralytics not installed") from exc

# MediaPipe hands
try:
    import mediapipe as mp
except Exception as exc:
    raise ImportError("mediapipe not installed") from exc

# OCR – tesseract (pytesseract)
try:
    import pytesseract
except Exception as exc:
    raise ImportError("pytesseract not installed") from exc


class VisionModule:
    def __init__(self, device: str = "cpu"):
        """
        Parameters
        ----------
        device : str
            'cuda' if GPU is available else 'cpu'
        """
        self.device = device

        # ----------------- YOLOv8 -----------------
        self._yolo = None  # lazy‑load

        # ----------------- MediaPipe Hands -----------------
        self.mp_hands = mp.solutions.hands.Hands(
            static_image_mode=False,
            max_num_hands=2,
            min_detection_confidence=0.7,
            min_tracking_confidence=0.5,
        )

    # ------------------------------------------------------------------
    @property
    def yolo(self):
        if self._yolo is None:
            self._yolo = YOLO("ultralytics/yolov8n.pt")  # small, fast
            self._yolo.to(self.device)
        return self._yolo

    # ------------------------------------------------------------------
    def _detect_objects(self, frame: np.ndarray) -> List[Dict[str, Any]]:
        """Return list of dicts with label, confidence and bbox."""
        results = self.yolo(frame)[0]
        objects = []
        for r in results.boxes:
            x1, y1, x2, y2 = map(int, r.xyxy.tolist()[0])
            conf = float(r.conf)
            cls_id = int(r.cls[0].item())
            label = results.names[cls_id]
            objects.append(
                {"label": label, "confidence": conf, "bbox": (x1, y1, x2, y2)}
            )
        return objects

    # ------------------------------------------------------------------
    def _detect_gestures(self, frame: np.ndarray) -> List[str]:
        """Very naïve gesture recognizer – only 'open' vs 'closed' hand."""
        h, w = frame.shape[:2]
        results = self.mp_hands.process(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
        gestures = []

        if not results.multi_hand_landmarks:
            return gestures

        for hand_lms in results.multi_hand_landmarks:
            # Count how many fingertips are above the wrist (simple heuristic)
            fingertip_ids = [4, 8, 12, 16, 20]
            count_open = 0
            wrist_y = hand_lms.landmark[mp.solutions.hands.HandLandmark.WRIST].y * h

            for fid in fingertip_ids:
                tip_y = hand_lms.landmark[fid].y * h
                if tip_y < wrist_y:          # finger extended upwards
                    count_open += 1

            gestures.append("open" if count_open >= 4 else "closed")

        return gestures

    # ------------------------------------------------------------------
    def _read_ocr(self, frame: np.ndarray) -> List[str]:
        """Return list of detected text snippets."""
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        # Optional pre‑processing
        _, thresh = cv2.threshold(gray, 150, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)
        texts = pytesseract.image_to_string(thresh, lang="eng")
        return [t.strip() for t in texts.splitlines() if t.strip()]

    # ------------------------------------------------------------------
    def process_frame(self, frame: np.ndarray) -> Dict[str, Any]:
        """
        Main entry point – returns a dict with:
            objects   : list[dict]
            gestures  : list[str]
            ocr_texts : list[str]
        """
        results = {
            "objects": self._detect_objects(frame),
            "gestures": self._detect_gestures(frame),
            "texts": self._read_ocr(frame),
        }
        return results
