"""Tappa 4 - esporta il modello addestrato in ONNX (per cv::dnn in C++).
ONNX non e' pickle: nessun codice arbitrario, formato sicuro.
"""
import os
from ultralytics import YOLO

BASE = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
BEST = os.path.join(BASE, "training", "runs", "card_obb", "weights", "best.pt")

model = YOLO(BEST)
path = model.export(format="onnx", opset=12, imgsz=640, simplify=True)
print("ONNX esportato in:", path)
