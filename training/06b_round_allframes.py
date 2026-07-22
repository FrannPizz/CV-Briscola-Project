"""Tappa 3b - test "tutti i frame di un round": l'idea e' che la briscola sta
ferma tutta la partita, quindi basta UN frame buono per game per leggerne il
seme. Qui si scorrono TUTTI i frame di un round (default round 3) di ogni game,
si fa detection e si guarda in quanti frame la briscola viene presa.

Output per game:
  - video annotato (real_test/roundN/gameX_det.mp4) con OBB e conteggio
  - frame migliore (max carte) come PNG, per un'occhiata veloce
  - statistiche a schermo: frame totali, % con >=1 detection, max carte, ecc.

Uso: python 06b_round_allframes.py [round=3] [weights.pt]
"""
import cv2, os, glob, sys, re
import numpy as np
from ultralytics import YOLO

BASE  = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
RND   = int(sys.argv[1]) if len(sys.argv) > 1 else 3
W     = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
         BASE, "training", "runs", "card_obb", "weights", "best.pt")
OUT   = os.path.join(BASE, "training", "real_test", f"round{RND}")
CONF  = 0.25
os.makedirs(OUT, exist_ok=True)

model = YOLO(W)
for g in ["game1", "game2", "game3", "game4"]:
    vp = os.path.join(BASE, "data/videos", g, f"{g}round{RND}.mp4")
    if not os.path.exists(vp):
        print(f"{g}: {os.path.basename(vp)} non trovato"); continue
    cap = cv2.VideoCapture(vp)
    fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
    w   = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h   = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    wr  = cv2.VideoWriter(os.path.join(OUT, f"{g}_det.mp4"),
                          cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
    counts = []; best = (-1, None, -1)   # (n_carte, frame_vis, idx)
    fi = 0
    while True:
        ok, fr = cap.read()
        if not ok:
            break
        res = model.predict(fr, conf=CONF, imgsz=640, verbose=False)[0]
        vis = fr.copy(); nb = 0
        if res.obb is not None and len(res.obb) > 0:
            for poly in res.obb.xyxyxyxy.cpu().numpy():
                cv2.polylines(vis, [poly.astype(np.int32)], True, (0, 255, 0), 3)
                nb += 1
        counts.append(nb)
        cv2.putText(vis, f"{g}round{RND}  f{fi}  carte:{nb}", (20, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.4, (0, 0, 255), 4)
        wr.write(vis)
        if nb > best[0]:
            best = (nb, vis.copy(), fi)
        fi += 1
    cap.release(); wr.release()
    if best[1] is not None:
        cv2.imwrite(os.path.join(OUT, f"{g}_best_f{best[2]}_{best[0]}carte.png"), best[1])
    nz = sum(1 for c in counts if c > 0)
    print(f"{g}round{RND}: {fi} frame | max {best[0]} carte (frame {best[2]}) | "
          f"media {sum(counts)/max(1,fi):.2f} | frame con >=1: {nz}/{fi} "
          f"({nz/max(1,fi)*100:.0f}%)")

print(f"\nVideo + frame migliori in: {OUT}")
