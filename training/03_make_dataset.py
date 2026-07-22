"""Tappa 2/3 - generatore di dataset SINTETICO per il detector di carte (1 classe).
Incolla SOLO i 40 fronti reali (niente dorsi/carte girate) su sfondi reali, con
rotazione/scala/prospettiva/ombra/luce casuali. Una parte dei frame e' solo
sfondo (negativi) e una parte delle carte e' tagliata di netto dal bordo
(~70% della figura visibile). Conosce gli angoli esatti ->
etichette perfette in formato YOLO-OBB (class x1 y1 x2 y2 x3 y3 x4 y4 normalizzati)
e anche detection assiale (class cx cy w h), cosi' possiamo scegliere il modello dopo.

Uso:
  python 03_make_dataset.py sample      # 12 immagini di prova + montaggio
  python 03_make_dataset.py full 3000    # dataset completo (train+val 90/10)
"""
import cv2, os, glob, random, sys
import numpy as np

BASE = r"C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project"
TPL  = os.path.join(BASE, "data", "template")
BG   = os.path.join(BASE, "training", "assets", "backgrounds")
CW, CH = 720, 1280   # canvas verticale, stesso aspect dei frame reali (1080x1920)
random.seed(42); np.random.seed(42)

# ---------- politiche di generazione ----------
P_BACKGROUND_ONLY = 0.12   # frazione di frame con SOLO sfondo (negativi, label vuota)
P_CARD_CUT        = 0.20   # prob. che una carta sia tagliata di netto dal bordo
CUT_VISIBLE       = 0.70   # frazione di carta visibile quando e' tagliata

# ---------- assets ----------
def load_fronts():
    cards = []
    for i in range(1, 41):
        im = cv2.imread(os.path.join(TPL, f"{i}.jpg"))
        if im is None:
            continue
        # i template sono orizzontali: li porto verticali (come sul tavolo)
        im = cv2.rotate(im, cv2.ROTATE_90_CLOCKWISE)
        cards.append(im)
    return cards

def load_bgs():
    return [cv2.imread(f) for f in glob.glob(os.path.join(BG, "*.png"))]

# ---------- compositing ----------
def card_with_alpha(card):
    """Aggiunge alpha con angoli arrotondati."""
    h, w = card.shape[:2]
    alpha = np.full((h, w), 255, np.uint8)
    r = int(min(w, h) * 0.06)
    cv2.rectangle(alpha, (0,0), (w-1,h-1), 0, 0)
    # smussa i 4 angoli
    for cx, cy in [(0,0),(w,0),(0,h),(w,h)]:
        cv2.circle(alpha, (cx,cy), r, 0, -1)
    cv2.rectangle(alpha, (r,0),(w-r,h),255,-1)
    cv2.rectangle(alpha, (0,r),(w,h-r),255,255 if False else -1)
    for cx,cy in [(r,r),(w-r,r),(r,h-r),(w-r,h-r)]:
        cv2.circle(alpha,(cx,cy),r,255,-1)
    return alpha

def paste_card(canvas, card, occupied, cut=False):
    H, W = canvas.shape[:2]
    h0, w0 = card.shape[:2]
    # scala: altezza carta realistica, tra 12% e 30% dell'altezza canvas
    target_h = random.uniform(0.12, 0.30) * H
    s = target_h / h0
    card = cv2.resize(card, (int(w0*s), int(h0*s)))
    h, w = card.shape[:2]
    alpha = card_with_alpha(card)

    # corner sorgente
    src = np.float32([[0,0],[w,0],[w,h],[0,h]])
    # rotazione + leggera prospettiva
    ang = random.uniform(0, 360)
    M = cv2.getRotationMatrix2D((w/2,h/2), ang, 1.0)
    pad = int(max(w,h))
    persp = random.uniform(0, 0.10)
    jit = np.float32([[random.uniform(-persp,persp)*w, random.uniform(-persp,persp)*h] for _ in range(4)])
    dst = cv2.transform(src.reshape(-1,1,2), M).reshape(-1,2) + jit + pad
    Hmat = cv2.getPerspectiveTransform(src, dst.astype(np.float32))

    big = pad*2 + max(w,h)*2
    warp  = cv2.warpPerspective(card,  Hmat, (big,big))
    walpha= cv2.warpPerspective(alpha, Hmat, (big,big))
    ys,xs = np.where(walpha>10)
    if len(xs)==0: return None
    x0,x1,y0,y1 = xs.min(),xs.max(),ys.min(),ys.max()
    cw,ch = x1-x0, y1-y0
    if cw<10 or ch<10: return None
    warp=warp[y0:y1,x0:x1]; walpha=walpha[y0:y1,x0:x1]
    corners = dst - [x0,y0]   # angoli relativi al ritaglio

    # ---- posizione sul canvas ----
    if cut:
        # carta tagliata di netto da un bordo: ~CUT_VISIBLE della figura resta visibile
        edge = random.choice(["l", "r", "t", "b"])
        off = 1.0 - CUT_VISIBLE
        if edge == "r":
            px = int(W - CUT_VISIBLE * cw); py = random.randint(0, max(1, H - ch))
        elif edge == "l":
            px = int(-off * cw);            py = random.randint(0, max(1, H - ch))
        elif edge == "b":
            px = random.randint(0, max(1, W - cw)); py = int(H - CUT_VISIBLE * ch)
        else:  # "t"
            px = random.randint(0, max(1, W - cw)); py = int(-off * ch)
        box = (px, py, px + cw, py + ch)
    else:
        # consenti un po' di overlap, evita troppi sovrapposti
        for _ in range(20):
            px = random.randint(0, max(1, W - cw)); py = random.randint(0, max(1, H - ch))
            box = (px, py, px + cw, py + ch)
            if sum(_iou(box, o) > 0.35 for o in occupied) == 0:
                break
        else:
            return None

    # ---- intersezione carta/canvas (gestisce le carte tagliate dal bordo) ----
    xa, ya = max(0, px), max(0, py)
    xb, yb = min(W, px + cw), min(H, py + ch)
    if xb - xa < 5 or yb - ya < 5:
        return None
    sxa, sya = xa - px, ya - py
    warp_v   = warp[sya:sya + (yb - ya), sxa:sxa + (xb - xa)]
    walpha_v = walpha[sya:sya + (yb - ya), sxa:sxa + (xb - xa)]
    a = (walpha_v.astype(np.float32) / 255.0)[..., None]

    # ombra (solo per carte interamente nel canvas)
    if not cut:
        sh = cv2.GaussianBlur(walpha, (0, 0), 7)
        so = (sh.astype(np.float32) / 255.0 * 0.4)[..., None]
        sx, sy = 6, 6
        if py + sy + ch <= H and px + sx + cw <= W:
            r2 = canvas[py + sy:py + sy + ch, px + sx:px + sx + cw]
            canvas[py + sy:py + sy + ch, px + sx:px + sx + cw] = (r2 * (1 - so)).astype(np.uint8)

    roi = canvas[ya:yb, xa:xb]
    canvas[ya:yb, xa:xb] = (roi * (1 - a) + warp_v * a).astype(np.uint8)

    pts = (corners + [px, py])
    return box, pts

def _iou(a,b):
    ix0,iy0=max(a[0],b[0]),max(a[1],b[1]); ix1,iy1=min(a[2],b[2]),min(a[3],b[3])
    iw,ih=max(0,ix1-ix0),max(0,iy1-iy0); inter=iw*ih
    ua=(a[2]-a[0])*(a[3]-a[1])+(b[2]-b[0])*(b[3]-b[1])-inter
    return inter/ua if ua>0 else 0

def make_image(fronts, bgs):
    bg = random.choice(bgs)
    bg = cv2.resize(bg, (CW, CH))
    canvas = cv2.convertScaleAbs(bg, alpha=random.uniform(0.8,1.2), beta=random.uniform(-20,20))
    # una parte dei frame e' SOLO sfondo (negativi): nessuna carta, label vuota
    if random.random() < P_BACKGROUND_ONLY:
        if random.random() < 0.5:
            canvas = cv2.GaussianBlur(canvas, (3,3), 0)
        return canvas, []
    n = random.randint(1,6)
    occupied=[]; labels=[]
    for _ in range(n):
        card = random.choice(fronts).copy()        # solo fronti, niente carte girate
        cut  = random.random() < P_CARD_CUT         # alcune carte tagliate dal bordo
        res  = paste_card(canvas, card, occupied, cut=cut)
        if res is None: continue
        box, pts = res
        occupied.append(box); labels.append(pts)
    # leggero blur/rumore globale
    if random.random()<0.5:
        canvas=cv2.GaussianBlur(canvas,(3,3),0)
    return canvas, labels

def to_yolo_obb(pts, W, H):
    # pts: (4,2) angoli in pixel -> "0 x1 y1 x2 y2 x3 y3 x4 y4" normalizzati
    p = np.clip((pts / [W, H]).reshape(-1), 0, 1)
    return "0 " + " ".join(f"{v:.6f}" for v in p)

# ---------- main ----------
def gen_sample():
    fronts,bgs=load_fronts(),load_bgs()
    out=os.path.join(BASE,"training","samples_synth"); os.makedirs(out,exist_ok=True)
    thumbs=[]
    for k in range(12):
        img,labels=make_image(fronts,bgs)
        vis=img.copy()
        for pts in labels:
            cv2.polylines(vis,[pts.astype(np.int32)],True,(0,255,0),2)
        cv2.imwrite(os.path.join(out,f"s{k:02d}.png"),vis)
        thumbs.append(cv2.resize(vis,(150,260)))
    rows=[np.hstack(thumbs[r*4:r*4+4]) for r in range(3)]
    m=os.path.join(BASE,"training","synth_montage.png")
    cv2.imwrite(m,np.vstack(rows))
    print("Montaggio di prova:",m)

def gen_full(n_total):
    fronts, bgs = load_fronts(), load_bgs()
    root = os.path.join(BASE, "training", "dataset")
    for sub in ["images/train","images/val","labels/train","labels/val"]:
        os.makedirs(os.path.join(root, sub), exist_ok=True)
    n_val = max(1, int(n_total*0.1))
    for k in range(n_total):
        split = "val" if k < n_val else "train"
        img, labels = make_image(fronts, bgs)
        H, W = img.shape[:2]
        stem = f"{k:05d}"
        cv2.imwrite(os.path.join(root, f"images/{split}/{stem}.jpg"), img,
                    [cv2.IMWRITE_JPEG_QUALITY, 92])
        with open(os.path.join(root, f"labels/{split}/{stem}.txt"), "w") as f:
            for pts in labels:
                f.write(to_yolo_obb(pts, W, H) + "\n")
        if (k+1) % 500 == 0:
            print(f"  {k+1}/{n_total}")
    # data.yaml per ultralytics OBB
    root_p = root.replace(os.sep, "/")
    yaml = (f"path: {root_p}\n"
            "train: images/train\n"
            "val: images/val\n"
            "names:\n  0: carta\n")
    with open(os.path.join(root, "data.yaml"), "w") as f:
        f.write(yaml)
    print(f"\nDataset pronto in {root}")
    print(f"  train: {n_total-n_val}  |  val: {n_val}")
    print(f"  config: {os.path.join(root,'data.yaml')}")

if __name__=="__main__":
    mode=sys.argv[1] if len(sys.argv)>1 else "sample"
    if mode=="sample":
        gen_sample()
    elif mode=="full":
        n = int(sys.argv[2]) if len(sys.argv)>2 else 2500
        gen_full(n)
    else:
        print("uso: python 03_make_dataset.py [sample | full N]")
