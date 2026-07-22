"""Tappa 3b - IL MOMENTO DELLA VERITA': il modello addestrato sul SINTETICO
funziona sui frame VERI? Prende UN frame per OGNI round di OGNI game (20x4=80),
fa detection, disegna gli OBB e salva ogni immagine singola (NON in montaggio)
in una cartella, cosi' le sfogli una per una.

Uso: python 06_test_real.py [weights.pt]  (default: best.pt del run card_obb)
"""
import cv2, os, glob, sys, re
import numpy as np
from ultralytics import YOLO

BASE = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
W    = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        BASE, "training", "runs", "card_obb", "weights", "best.pt")
OUT  = os.path.join(BASE, "training", "real_test", "frames")
CONF = 0.25
os.makedirs(OUT, exist_ok=True)

def round_num(p):
    """numero di round dal nome (gameXroundY.mp4) per ordinare 1,2,3.. e non 1,10,11.."""
    m = re.search(r"round(\d+)", os.path.basename(p))
    return int(m.group(1)) if m else 0

model = YOLO(W)
tot = 0
for g in ["game1", "game2", "game3", "game4"]:
    vids = sorted(glob.glob(os.path.join(BASE, "data/videos", g, "*.mp4")), key=round_num)
    for vp in vids:
        name = os.path.basename(vp).replace(".mp4", "")
        cap = cv2.VideoCapture(vp)
        n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        cap.set(cv2.CAP_PROP_POS_FRAMES, n // 2)     # frame centrale del round
        ok, fr = cap.read()
        cap.release()
        if not ok:
            print(f"{name}: frame non leggibile"); continue
        res = model.predict(fr, conf=CONF, imgsz=640, verbose=False)[0]
        vis = fr.copy(); nb = 0
        if res.obb is not None and len(res.obb) > 0:
            for poly, c in zip(res.obb.xyxyxyxy.cpu().numpy(),
                               res.obb.conf.cpu().numpy()):
                cv2.polylines(vis, [poly.astype(np.int32)], True, (0, 255, 0), 3)
                nb += 1
        cv2.putText(vis, f"{name}: {nb}", (20, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 0, 255), 4)
        cv2.imwrite(os.path.join(OUT, f"{name}.png"), vis)
        tot += 1
        print(f"{name}: {nb} carte rilevate")

print(f"\n{tot} immagini salvate (singole, con riquadro) in: {OUT}")
