"""Sub-step 2a - raccoglie patch di SFONDO (tavolo senza carte) dai video.
Le carte stanno quasi sempre al centro: ritaglio strisce di bordo (alto/sinistra/destra)
che di solito sono solo stoffa/legno. Salva le patch e crea un montaggio per il controllo visivo.
"""
import cv2, os, glob, random
import numpy as np

BASE = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
OUT  = os.path.join(BASE, "training", "assets", "backgrounds")
os.makedirs(OUT, exist_ok=True)
random.seed(0)

PATCH = 256  # patch quadrate 256x256

def grab_patches(frame):
    """Ritaglia patch quadrate dalle fasce di bordo (poche carte li')."""
    h, w = frame.shape[:2]
    patches = []
    # fascia ALTA (y in 0..top), tutta larghezza
    top = int(h * 0.18)
    # fasce LATERALI (x nei primi/ultimi side px), tutta altezza
    side = int(w * 0.16)
    regions = [
        (0, 0, w, top),                 # alto
        (0, 0, side, h),                # sinistra
        (w - side, 0, side, h),         # destra
    ]
    for (rx, ry, rw, rh) in regions:
        if rw < PATCH or rh < PATCH:
            continue
        for _ in range(3):
            x = rx + random.randint(0, rw - PATCH)
            y = ry + random.randint(0, rh - PATCH)
            patches.append(frame[y:y+PATCH, x:x+PATCH].copy())
    return patches

games = ["game1", "game2", "game3", "game4"]
saved = []
for g in games:
    vids = sorted(glob.glob(os.path.join(BASE, "data/videos", g, "*.mp4")))
    random.shuffle(vids)
    count = 0
    for vp in vids[:4]:                  # 4 round per game
        cap = cv2.VideoCapture(vp)
        n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        for pos in [int(n*0.1), int(n*0.9)]:
            cap.set(cv2.CAP_PROP_POS_FRAMES, max(0, pos))
            ok, fr = cap.read()
            if not ok:
                continue
            for p in grab_patches(fr):
                fn = os.path.join(OUT, f"{g}_{count:03d}.png")
                cv2.imwrite(fn, p)
                saved.append(fn)
                count += 1
        cap.release()
    print(f"{g}: {count} patch di sfondo")

# montaggio di controllo: 5x5 patch a caso
random.shuffle(saved)
grid = saved[:25]
thumbs = [cv2.resize(cv2.imread(f), (128, 128)) for f in grid]
while len(thumbs) < 25:
    thumbs.append(np.zeros((128, 128, 3), np.uint8))
rows = [np.hstack(thumbs[r*5:r*5+5]) for r in range(5)]
montage = np.vstack(rows)
mpath = os.path.join(BASE, "training", "bg_montage.png")
cv2.imwrite(mpath, montage)
print(f"\nTotale patch: {len(saved)}")
print(f"Montaggio di controllo: {mpath}")
