"""Augment the dataset with extra synthetic images on the GAME3 background only,
with more cut cards (game3 is where the detector struggles). Adds images to BOTH
train and val. Reuses the generator functions from 03_make_dataset.py.

Usage:  python augment_game3.py [n_train=700] [n_val=80]
"""
import importlib.util, os, glob, random, sys
import cv2
import numpy as np

TR_DIR = os.path.dirname(os.path.abspath(__file__))   # .../Prova Final Project/training

#load the generator module (its name starts with a digit -> importlib)
spec = importlib.util.spec_from_file_location("mkds", os.path.join(TR_DIR, "03_make_dataset.py"))
mkds = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mkds)

#policy for this batch: more cut cards, fewer empty frames (we want game3 cards)
mkds.P_CARD_CUT = 0.40
mkds.P_BACKGROUND_ONLY = 0.08

#different seed so these images differ from the existing dataset (which used seed 42)
random.seed(2024); np.random.seed(2024)

N_TRAIN = int(sys.argv[1]) if len(sys.argv) > 1 else 700
N_VAL   = int(sys.argv[2]) if len(sys.argv) > 2 else 80

fronts = mkds.load_fronts()
#ONLY game3 backgrounds, from this copy's assets
bg_files = sorted(glob.glob(os.path.join(TR_DIR, "assets", "backgrounds", "game3_*.png")))
bgs = [cv2.imread(f) for f in bg_files]
bgs = [b for b in bgs if b is not None]
print(f"fronts: {len(fronts)} | game3 backgrounds: {len(bgs)}")
if not fronts or not bgs:
    raise RuntimeError("missing fronts or game3 backgrounds")

root = os.path.join(TR_DIR, "dataset")

def gen_into(split, n):
    img_dir = os.path.join(root, "images", split)
    lbl_dir = os.path.join(root, "labels", split)
    for k in range(n):
        img, labels = mkds.make_image(fronts, bgs)
        H, W = img.shape[:2]
        stem = f"game3_{k:05d}"
        cv2.imwrite(os.path.join(img_dir, f"{stem}.jpg"), img, [cv2.IMWRITE_JPEG_QUALITY, 92])
        with open(os.path.join(lbl_dir, f"{stem}.txt"), "w") as f:
            for pts in labels:
                f.write(mkds.to_yolo_obb(pts, W, H) + "\n")
    print(f"  {split}: +{n} game3 images")

gen_into("train", N_TRAIN)
gen_into("val", N_VAL)

#clear stale ultralytics caches so the new images are picked up
for c in ["train.cache", "val.cache"]:
    p = os.path.join(root, "labels", c)
    if os.path.exists(p):
        os.remove(p)

print(f"\nDone: added {N_TRAIN} train + {N_VAL} val game3 images.")
