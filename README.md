# Briscola Game Recognizer

A computer vision system that watches video recordings of a Briscola game and rebuilds it:
it recognizes the cards played in every round, the briscola (trump) card, who played each card,
who led and who won each round, the points of each round and the final score.

**Authors:** Filippo Facco, Francesco Pizzato

The full description of the approach and of the results is in the report
`report_final_project_Facco_Pizzato.pdf`.

The project was developed on Windows (MSYS2) and tested on Linux (VLab). The instructions below
are for Linux.

> **⚠️ The videos are NOT included in this repository.**
> Moodle does not accept submissions larger than 20 MB, so the game videos could not be uploaded.
> Before running the program you **must download all the videos** from the Google Drive folder
> linked in the assignment submission and copy them, for every game, into the already existing
> structure `data/videos/<game>/` with the names `<game>round<N>.mp4`
> (e.g. `data/videos/game1/game1round1.mp4` ... `data/videos/game4/game4round20.mp4`).
> Without the videos the program cannot run, even though the YOLO detections are already cached
> in `json/`.

## Pipeline

The program runs in four stages:

1. **Detection (Python):** a YOLOv8n-OBB model (`best.pt`, one class: *card*) finds the cards
   in the videos, sampling one frame every 10 in the central 20%–80% of each round. It only
   localizes the cards: for each one it saves round, frame, confidence and the 4 corners of the
   oriented box in `json/<game>.json`.
2. **Recognition (C++):** the C++ program reads the JSON, takes the same frame from the video,
   rectifies each card with a perspective transform and matches it with SIFT (Lowe ratio test
   0.75, at least 15 good matches) against 41 templates: the 40 cards plus `0.jpg`, the
   **card back**, so that the deck and the piles of won cards are discarded instead of being read
   as a card.
3. **Game reconstruction (C++):**
   - **briscola:** the card seen in the most rounds *in the same place* (close to its median
     position) during rounds 1–17, when it lies under the deck. A candidate seen twice in the
     same frame was actually played and is rejected.
   - **cards of each round:** every frame votes the cards for the North/South slot of its round,
     then the **Hungarian algorithm** assigns a different card to each of the 40 slots (every card
     is played exactly once), which removes duplicates and fills in cards that were never
     recognized.
   - **players and leader:** the boxes of each round are grouped in **position clusters**
     (ignoring the labels); the first cluster in time is the first card, the second is the one
     placed above/below it. The higher card is North's, the lower South's. Then the Briscola rules
     give the winner and the points.
4. **Evaluation:** the reconstructed game is saved as CSV and compared with the ground truth.

Python and C++ communicate through the JSON file: if `json/<game>.json` is missing, `main`
runs `python3 ../src/CardDetector.py <game>` with `std::system` and then parses the JSON with
`nlohmann/json`. The JSON works as a cache, so YOLO runs only once per game.

## Project structure

```
├── CMakeLists.txt
├── best.pt                                # trained YOLOv8n-OBB weights
├── report_final_project_Facco_Pizzato.pdf # project report
├── include/                               # headers (+ nlohmann/json)
├── src/
│   ├── main.cpp             # runs the whole pipeline
│   ├── CardDetector.py      # YOLO detection -> json/<game>.json
│   ├── readJSON.cpp         # reads the detections
│   ├── CardRectifier.cpp    # perspective rectification of a card
│   ├── CardRecognizer.cpp   # SIFT matching against the templates
│   ├── FixedMatch.cpp       # frame votes + Hungarian algorithm
│   ├── MatchTheory.cpp      # briscola, clusters, players, winner, points
│   ├── Print.cpp            # console output and CSV writing
│   └── PerformanceMeasurement.cpp  # metrics against the ground truth
├── data/
│   ├── videos/<game>/       # <game>round<N>.mp4 (NOT included, must be added)
│   ├── template/            # 0.jpg (card back) ... 40.jpg
│   └── ground_truth/        # <game>resultsCORRECTED.csv
├── json/                    # cached YOLO detections of the 4 games
└── output/                  # generated CSV and metrics
```

Card labels: 1–10 Spades, 11–20 Clubs, 21–30 Cups, 31–40 Coins, 0 = card back / no match.

## Requirements

- C++17 compiler, CMake ≥ 3.10
- OpenCV 4 with SIFT (OpenCV ≥ 4.4).
- Python is **not** required: the YOLO detections of all four games are already included in
  `json/`. Python 3 with `ultralytics` and `opencv-python` is needed only to regenerate them.
  The easiest way is a virtual environment (CPU-only PyTorch, smaller download):
  ```bash
  python3 -m venv ~/yolo-env
  source ~/yolo-env/bin/activate
  pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu
  pip install ultralytics opencv-python-headless
  ```

CMake finds OpenCV automatically, no extra configuration is needed.

## Build

From the project root:

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j4
```

`rm -rf build` removes any old CMake cache, which would make CMake fail if the project was
configured in a different folder.

## Run

**The videos are missing (Moodle's 20 MB upload limit) and must be added by hand**, taking all of
them from the Google Drive folder linked in the submission. They must be in
`data/videos/<game>/` with the names `<game>round<N>.mp4`
(e.g. `data/videos/game1/game1round1.mp4`). Copy them there before running.
They are needed also when the JSON exists, because the cards are cut from the original frames.

The program uses paths relative to the build folder, so run it from there:

```bash
cd build
./main game1
```

Usage: `./main [game] [ground_truth.csv]`. The game defaults to `game1`; the ground truth
defaults to `../data/ground_truth/<game>resultsCORRECTED.csv`.

If `json/<game>.json` is missing, the program runs `python3 ../src/CardDetector.py <game>` to
create it (this takes a few minutes). Delete the JSON to force YOLO to run again.
To run YOLO, activate the virtual environment first (`source ~/yolo-env/bin/activate`), then run
`main`. The detector can also be run by hand from the `src` folder:
`python3 CardDetector.py <game>`.

## Output

- `output/<game>_output.csv`: the reconstructed game, in the same format as the ground truth
- `output/<game>_metrics.txt`: evaluation metrics and the list of wrong fields

## Results

| Game  | Cards  | Players | Briscola | Result | Score (N–S), predicted / ground truth |
|-------|--------|---------|----------|--------|---------------------------------------|
| game1 | 40/40  | 40/40   | 1/1      | 3/3    | 53–67 / 53–67                         |
| game2 | 37/40  | 40/40   | 1/1      | 3/3    | 59–61 / 59–61                         |
| game3 | 38/40  | 39/40   | 1/1      | 1/3    | 74–46 / 70–50                         |
| game4 | 40/40  | 40/40   | 1/1      | 3/3    | 63–57 / 63–57                         |
| Total | 155/160 | 159/160 | 4/4     | 10/12  |                                       |

The winner of the game is correct in all four games. In game 2 the only errors are 0-point cards
(7/6 of Coins and 7 of Cups), so the score is still exact; in game 3 (the realistic game) the two
cards of round 4 are assigned to the wrong players, which moves 4 points from South to North.
