"""Tappa 1 - guardiamo i dati.
Estrae qualche frame da game diversi e li salva in training/samples/,
stampando le proprieta' dei video (risoluzione, fps, n. frame)."""
import cv2, os

BASE = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
OUT  = os.path.join(BASE, "training", "samples")
os.makedirs(OUT, exist_ok=True)

# un round per ogni game
vids = [
    "data/videos/game1/game1round1.mp4",
    "data/videos/game2/game2round5.mp4",
    "data/videos/game3/game3round10.mp4",
    "data/videos/game4/game4round1.mp4",
]

for vp in vids:
    full = os.path.join(BASE, vp)
    cap = cv2.VideoCapture(full)
    if not cap.isOpened():
        print(f"[SKIP] non apro {vp}")
        continue
    n   = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    w   = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h   = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    tag = os.path.basename(vp).replace(".mp4", "")
    print(f"{tag}: {w}x{h}  fps={fps:.1f}  frame={n}  durata={n/max(fps,1):.1f}s")

    # 3 frame: inizio, meta', fine (le carte cambiano durante il round)
    for pos, lbl in [(int(n*0.15), "a"), (n//2, "b"), (int(n*0.85), "c")]:
        cap.set(cv2.CAP_PROP_POS_FRAMES, max(0, pos))
        ok, frame = cap.read()
        if ok:
            cv2.imwrite(os.path.join(OUT, f"{tag}_{lbl}_f{pos}.png"), frame)
    cap.release()

print("\nFrame salvati in:", OUT)
print("File:", sorted(os.listdir(OUT)))
