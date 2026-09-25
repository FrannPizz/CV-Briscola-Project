#Author: Francesco Pizzato
import cv2, os, glob, sys, re, json
import numpy as np
import ultralytics

def detect_card(game):
    #load the pretrained model
    model = ultralytics.YOLO("../best.pt")

    gamesPath = os.path.join("../data/videos", game)

    files = os.listdir(gamesPath)
    #flat list: one entry per detected card
    game_data = []
    #for every round of the game
    for j in range(len(files)):

        roundPath = os.path.join(gamesPath, f"{game}round{j+1}.mp4")

        #create a vector of frames of the round video
        if os.path.exists(roundPath):

            #open the round video
            frames = cv2.VideoCapture(roundPath)

            #sample at most MAX_FRAMES_PER_ROUND frames, evenly spread along the round
            total = int(frames.get(cv2.CAP_PROP_FRAME_COUNT))
            step = 10

            #for every 5 frames in the middle of teh round
            for frame_idx in range(int(total * 0.2), int(total * 0.8), step):

                #jump to the sampled frame and read it
                frames.set(cv2.CAP_PROP_POS_FRAMES, frame_idx)
                bool, frame = frames.read()
                if not bool:
                    break

                #use YOLO in the frame
                res = model.predict(frame, conf=0.25, imgsz=640, verbose=False)[0]

                #for every detected card, save the coordinates and the confidence
                if res.obb is not None and len(res.obb) > 0:
                    for poly, conf in zip(res.obb.xyxyxyxy.cpu().numpy(),
                                          res.obb.conf.cpu().numpy()):
                        game_data.append({
                            "round": j + 1,
                            "frame": frame_idx,
                            "conf": float(conf),
                            "corners": poly.tolist()    #[[x1,y1],...,[x4,y4]]
                        })
            frames.release()

        else:
            return print(f"Round video not found: {roundPath}")

    #write one json for the game in the json/ folder
    with open(os.path.join("../json", f"{game}.json"), "w") as f:
        json.dump(game_data, f, indent=2)

if __name__ == '__main__':
    #game passed as argument 
    game = sys.argv[1]
    detect_card(game)
