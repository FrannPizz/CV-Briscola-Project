# Briscola Game Reconstruction from Video

Computer Vision, University of Padua.

A system that watches the video recordings of a two-player **Briscola** game and rebuilds it round by round: the two cards played, the briscola (trump) card, which player played each card, who led, who won the round and with how many points, and finally the score and the winner of the whole game.

Cards are localized by a **YOLOv8-OBB** detector in Python and recognized in C++ with **OpenCV** by **SIFT** matching against the 40 reference cards. Single-frame recognition on real video is noisy, so the game is not rebuilt frame by frame: the evidence of the whole game is combined with frame voting, a global one-to-one assignment solved with the **Hungarian algorithm**, label-free spatial clustering and a persistence criterion for the briscola.

On the four games of the dataset the system recognizes **155/160 cards**, **159/160 player labels** and **4/4 briscola cards**, and reconstructs the final score exactly in three games out of four.

**Authors:** Filippo Facco, Francesco Pizzato.
The full description of the method and of the experiments is in [`Report_Final_Project_FaccoPizzato.pdf`](Report_Final_Project_FaccoPizzato.pdf).

## The problem

Each game is given as 20 short clips, one per round, recorded by a fixed camera above the table. The system has to produce one row per round,

```
Round,North_Number,North_Suit,South_Number,South_Suit,Briscola_Number,Briscola_Suit,Leader,Winner,Points
```

plus the final totals of the two players and the overall winner.

What makes it hard is that the table is not a clean scene: cards are rotated and partially overlapping, hands enter and cover them while they are being placed, the tablecloth is patterned, the lighting changes between games, and most of the cards in frame are not in play at all (the deck, the briscola lying under it, and the growing piles of cards already won by each player).

## Approach

The problem is split in two parts, and the second one is where most of the accuracy comes from.

**Perception** answers "where are the cards, and which card is each box" for a single frame. It is deliberately kept as classical as possible: a learned detector is used only as a localizer, on a single class `card`, and the actual identity of a card is decided by SIFT descriptors against reference templates. The first version of the detector was fully classical (Canny edges, morphological closing, contour filtering by area and aspect ratio) but it missed too many cards on the real videos, so it was replaced by YOLO. YOLO says *where*, never *which*.

**Reasoning** turns those noisy per-frame observations into a consistent game, using what is known about Briscola itself:

- every card of the deck is played exactly once in a 20-round game;
- the briscola never moves until the deck is exhausted;
- North plays above the centre line, South below it;
- the second card of a round is placed next to the first one, shifted up or down.

These constraints link frames and rounds together, so the evidence of one round can correct the mistakes of another. The practical consequence is that single-frame recognition does not have to be perfect.

## Pipeline

```
Detection            Recognition                Game logic                 Output
YOLOv8-OBB     ->    rectification    ->    briscola, frame votes,   ->   CSV +
(Python)             + SIFT (C++)           Hungarian, clusters           metrics
                 json/<game>.json
```

### 1. Detection

A YOLOv8n-OBB model (`best.pt`, one class: *card*) finds the cards and returns **oriented** boxes, whose four corners are exactly what the rectification step needs. The training set was generated synthetically from the 40 reference cards over backgrounds taken from the game videos, so every image comes with exact oriented labels and no manual annotation was needed; the model was then fine-tuned from the public `yolov8n-obb.pt` weights with colour jitter, random translation and scaling, flips, mosaic and random erasing to simulate occlusion by hands.

At inference the detector samples one frame every 10, only in the central 20%-80% of each clip, since the beginning and the end show the table being set up and the cards being collected.

Python and C++ communicate through a JSON file, one entry per detected card:

```json
{ "round": 1, "frame": 60, "conf": 0.914,
  "corners": [[427.7,1563.1],[592.2,1503.6],[483.1,1201.7],[318.6,1261.1]] }
```

The JSON also works as a cache: if it is missing, the C++ program launches the detector as an external process and then parses the result with `nlohmann/json`, so YOLO runs only once per game.

### 2. Recognition

The four corners come back in arbitrary order, so they are sorted by angle around their centroid (which works at any rotation) and rotated to start from the top-left one, giving `TL, TR, BR, BL`. A perspective transform then produces a straight, portrait-oriented image of the card, rotating it by 90 degrees if it comes out landscape, as happens with the briscola lying sideways under the deck.

SIFT descriptors of the templates are computed once at start-up. For each detected card the descriptors are matched against every template with a brute-force L2 matcher, keeping the matches that pass Lowe's ratio test at 0.75; the card takes the label of the template with the most good matches, and is labelled `0` if the best template has fewer than 15. SIFT being invariant to rotation and scale is what makes an upside-down card, or one further from the camera, still match.

**The 41st template is the card back.** The deck, the piles of won cards and the cards under a hand are all correctly detected as cards, and without an explicit model of the back SIFT would assign each of them the least bad face card, producing more false observations than real ones. Adding `0.jpg` with label `0`, and accepting a face card only on a strictly greater number of matches (ties go to the back), removes them from the game logic entirely.

### 3. Game reconstruction

**The briscola** is the card seen in the largest number of *different rounds in the same place*. For each label its usual position is the median of its boxes over rounds 1 to 17 (after round 17 the deck is exhausted and the briscola can be in a hand), and the number of distinct rounds in which it is recognized within 150 px of that position is counted. Counting rounds rather than detections is what makes this robust: a played card appears in one round only, and a misread pile appears in scattered positions. A candidate seen twice in the same frame in two distant boxes is rejected, since the real briscola cannot be under the deck and on the table at once. Once found, its boxes are filtered out of the evidence about the played cards.

**Which two cards were played** is decided by voting rather than by trusting any single frame. A score matrix of 40 slots (North and South of each round) by 40 cards is filled frame by frame: when two or more cards are visible, the highest one scores in the North slot and the lowest in the South slot; when only one is visible, it scores in both, since it is not yet known who played it.

**The global assignment** is then solved with the Hungarian algorithm. Picking the best-scoring card independently for each slot would break the one constraint that ties the whole game together, and it breaks it in both directions: a card confused with another in some frames could be predicted twice, and a card that is always hidden by a hand collects no votes and never gets chosen, leaving its slot to whatever noise scored highest. Treating it as a perfect bipartite matching between slots and cards fixes both at once, and the Hungarian algorithm returns the global optimum in *O(n^3)*, which for n = 40 costs nothing next to YOLO and SIFT. A greedy pass over the best pairs would depend on the order and could not undo an early mistake.

**Who played what** is decided from positions, not labels. The boxes of a round are grouped into clusters by proximity alone, because when the second card is placed on top of the first the covered card is often read with the label of the card above it, while its position does not change. The first cluster in time that lasts at least 3 frames is the leader's card, which discards the one- or two-frame clusters produced by a hand passing over the table; the second is a later cluster in the same column and shifted vertically. The higher cluster belongs to North and the lower to South, with the border between the two halves estimated from the data instead of being fixed at half the frame height.

The rest is the rulebook: a single briscola-suit card wins, otherwise same suit means the stronger card wins with the order `1 > 3 > 10 > 9 > 8 > 7 > 6 > 5 > 4 > 2`, otherwise the leader wins; points are `1 -> 11, 3 -> 10, 10 -> 4, 9 -> 3, 8 -> 2` and zero for the rest.

### 4. Evaluation

The reconstructed game is written as CSV in the ground-truth format and read back, so what is printed, what is saved and what is measured are the same rows. Four metrics are computed against the ground truth: card recognition (40 fields, rank and suit must both match), player identification (40), briscola (1) and final result (3: the winner and the two scores). Every wrong field is listed in `output/<game>_metrics.txt`.

## Results

| Game                         | Cards               | Players             | Briscola | Result    | Score N-S, predicted / truth |
|------------------------------|---------------------|---------------------|----------|-----------|------------------------------|
| game1 (controlled)           | 40/40               | 40/40               | 1/1      | 3/3       | 53-67 / 53-67                |
| game2 (different lighting)   | 37/40               | 40/40               | 1/1      | 3/3       | 59-61 / 59-61                |
| game3 (realistic play)       | 38/40               | 39/40               | 1/1      | 1/3       | 74-46 / 70-50                |
| game4 (different background) | 40/40               | 40/40               | 1/1      | 3/3       | 63-57 / 63-57                |
| **Total**                    | **155/160 (96.9%)** | **159/160 (99.4%)** | **4/4**  | **10/12** |                              |

The winner of the game is correct in all four games. Games 1 and 4 are reconstructed without a single error, including game 4, whose different background produces more than twice the detections of the other games (1250 against 355-591).

Two failure cases are worth reporting, because they show the limits of the method:

- **game2** shows the other side of the global assignment. The 7 of Coins is read as the 6 of Coins, two cards with a very similar layout of pips. The 7 then stays available and the Hungarian algorithm places it in another round, so one systematic confusion moves an error into a second round instead of absorbing it. All the cards involved are worth 0 points, so leader, winner and final score stay correct.
- **game3** is the realistic game, with hands over the cards and no clean pause before they are collected. In round 4 both cards are recognized correctly but attributed to the wrong players: the 6 of Coins was covered while it was being placed, so it was recognized *after* the 10 and the order of play was reversed. One swap in one round costs the round winner and 4 points, which is enough to lose two of the three result fields even though the overall winner is still right.

## Project structure

```
├── CMakeLists.txt
├── best.pt                 # trained YOLOv8n-OBB weights
├── include/                # headers (+ nlohmann/json)
├── src/
│   ├── main.cpp                    # runs the whole pipeline
│   ├── CardDetector.py             # YOLO detection -> json/<game>.json
│   ├── readJSON.cpp                # reads the detections
│   ├── CardRectifier.cpp           # perspective rectification of a card
│   ├── CardRecognizer.cpp          # SIFT matching against the templates
│   ├── FixedMatch.cpp              # frame voting + Hungarian algorithm
│   ├── MatchTheory.cpp             # briscola, clusters, players, winner, points
│   ├── Print.cpp                   # console output and CSV writing
│   └── PerformanceMeasurement.cpp  # metrics against the ground truth
├── data/
│   ├── videos/<game>/      # <game>round<N>.mp4 (not included)
│   ├── template/           # 0.jpg (card back) ... 40.jpg
│   └── ground_truth/       # <game>resultsCORRECTED.csv
├── json/                   # cached YOLO detections of the 4 games
├── output/                 # generated CSV and metrics
└── training/               # synthetic dataset generation and YOLO fine-tuning
```

Card labels are integers: 1-10 Spades, 11-20 Clubs, 21-30 Cups, 31-40 Coins, and 0 means "not a card face" (card back or no reliable match).

## Training the detector

The `training/` folder holds the pipeline that produced `best.pt`, kept in the repository so the detector can be reproduced from scratch. No frame was ever annotated by hand.

| Script | Step |
|--------|------|
| `01_sample_frames.py` | samples frames from the videos and prints their properties, to look at the data first |
| `02_collect_backgrounds.py` | cuts background patches from the border bands of the frames, where cards almost never are, giving real tablecloths and lighting to paste on |
| `03_make_dataset.py` | generates the synthetic dataset: the 40 reference faces pasted on those backgrounds with random rotation, scale, perspective, shadow and lighting, some frames with background only as negatives and some cards cut by the image border, writing exact YOLO-OBB labels |
| `04_train.py` | fine-tunes `yolov8n-obb.pt` on the synthetic set |
| `05_export_onnx.py` | exports the trained model to ONNX |
| `06_test_real.py`, `06b_round_allframes.py` | check on the real videos: one frame per round of every game, and every frame of a single round |

The interesting part is step 06: the detector is trained only on synthetic images and is never shown a labelled real frame, so this is where it becomes clear whether the synthetic data transfers to the actual videos.

The generated dataset, the training runs and the test outputs are not committed; the scripts regenerate them.

## Building and running

Requirements: a C++17 compiler, CMake 3.10 or newer, and OpenCV 4.4 or newer (for SIFT). Python is needed only to regenerate the detections, since the YOLO output of all four games is already cached in `json/`.

```bash
cmake -S . -B build
cmake --build build -j4
cd build && ./main game1
```

Usage is `./main [game] [ground_truth.csv]`; paths are relative to the build folder. Results are written to `output/<game>_output.csv` and `output/<game>_metrics.txt`.

> **The game videos are not included in this repository.** They are part of the course dataset and exceed the size limits, so `data/videos/<game>/` is empty. The program needs them even when the JSON cache is present, because the cards are cut out of the original frames.

Developed on Windows (MSYS2) and tested on Linux.

## Authors

Filippo Facco and Francesco Pizzato, University of Padua.
