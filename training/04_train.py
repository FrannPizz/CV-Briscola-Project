"""Tappa 3 - addestra il detector di carte (YOLOv8n-OBB, 1 classe).
Pesi di partenza: yolov8n-obb.pt UFFICIALE Ultralytics (fonte fidata).

Uso:
  python 04_train.py smoke     # 1 epoca, prova rapida che tutto giri
  python 04_train.py train     # training vero (60 epoche)
"""
import sys
from pathlib import Path
from ultralytics import YOLO

BASE = Path(__file__).resolve().parents[1]
DATA = BASE / "training" / "dataset" / "data.yaml"
PROJ = BASE / "training" / "runs"

def run(mode):
    model = YOLO(str(BASE / "yolov8n-obb.pt"))          # ufficiale, scaricato da Ultralytics
    common = dict(
        data=str(DATA),
        imgsz=640,
        device=0,                            # GPU
        project=str(PROJ),
        workers=4,
        plots=True,
    )
    if mode == "smoke":
        model.train(epochs=1, batch=4, name="smoke", exist_ok=True, **common)
    else:
        model.train(epochs=60, batch=8, name="card_obb", patience=20,
                    # augmentation: i frame reali aggiungono variazioni che il sintetico non ha
                    hsv_h=0.02, hsv_s=0.5, hsv_v=0.5, degrees=0.0,
                    translate=0.1, scale=0.3, fliplr=0.5, mosaic=1.0,
                    exist_ok=True, **common)

if __name__ == "__main__":
    run(sys.argv[1] if len(sys.argv) > 1 else "train")
