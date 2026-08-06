# Computer Vision System for Briscola Round Analysis

Analisi automatica di video di partite di Briscola a due giocatori. Per ogni round il
sistema individua le carte sul tavolo, ne riconosce numero e seme, determina la briscola,
associa ogni carta al giocatore (North / South), stabilisce chi ha aperto e chi ha vinto,
e accumula il punteggio fino al risultato finale della partita.

## Requisiti

| | Versione |
|---|---|
| Compilatore C++ | C++17 |
| CMake | 3.10 o superiore |
| OpenCV | 4.x (moduli `core`, `imgproc`, `imgcodecs`, `videoio`, `features2d`, `calib3d`, `highgui`) |

Su Windows il progetto è stato sviluppato con MSYS2/UCRT64 e MinGW. Su Linux bastano
`build-essential`, `cmake` e `libopencv-dev`.

## Struttura dei dati

I video e le annotazioni **non** sono nel repository (esclusi da `.gitignore` per peso e
per non ridistribuire il dataset). Vanno scaricati e disposti così:

```
data/
├── template/                    # i 40 fronti di riferimento, 1.jpg ... 40.jpg (già nel repo)
├── videos/
│   ├── game1/game1round1.mp4 ... game1round20.mp4
│   ├── game2/ ...
│   ├── game3/ ...
│   └── game4/ ...
├── game1resultsCORRECTED.csv    # ground truth (versione corretta, da preferire)
├── game1results.csv             # ground truth (versione originale)
└── ...
```

I nomi dei video devono essere esattamente `gameXroundY.mp4`, senza zeri iniziali.

## Compilazione

```bash
mkdir build
cd build
cmake ..
cmake --build . -j 4
```

Produce un unico eseguibile `main` (`main.exe` su Windows) dentro `build/`.

**Lanciare sempre l'eseguibile dalla cartella `build/`**: il caricamento dei template usa
il percorso relativo `../data/template/`.

## Uso

L'eseguibile ha quattro sotto-comandi.

### `game` — analisi di una partita intera

```bash
./main game ../data game1 --out ../output
```

Analizza i 20 round e produce in `../output/`:

- `game1_output.csv` — una riga per round, nel formato della consegna
- `game1_output.txt` — stesso contenuto leggibile, più punteggi finali e vincitore
- `game1_metrics.txt` — le quattro metriche di accuratezza

Opzioni:

| Opzione | Default | Significato |
|---|---|---|
| `--rounds N` | 20 | quanti round analizzare |
| `--step N` | 2 | campiona 1 frame ogni N; alzarlo velocizza molto |
| `--gt <file>` | auto | ground truth per le metriche |
| `--out <dir>` | `.` | cartella di output (deve esistere) |
| `--verbose` | off | stampa le track trovate in ogni round |

Se `--gt` non viene passato, il programma cerca da solo `<game>resultsCORRECTED.csv` e
ripiega su `<game>results.csv`; stampa sempre quale file ha usato.

Con `--step 5` una partita richiede circa 5-10 minuti, a seconda della macchina.

### `round` — analisi di un solo round

```bash
./main round ../data/videos/game1/game1round1.mp4 --step 5
```

Stampa le track trovate (posizione, in quali frame compaiono, quante volte sono state
riconosciute e con quanti voti) e il risultato del round. È lo strumento per capire
perché un round non viene letto bene.

### `frame` — debug del detector su un singolo fotogramma

```bash
./main frame ../data/videos/game1/game1round1.mp4 150
```

Apre finestre con la maschera dei bordi, i contorni candidati, le carte raddrizzate e il
template abbinato. Stampa anche quanti candidati sopravvivono a ciascun filtro
geometrico. Aggiungere `--no-gui` per salvare le immagini invece di mostrarle.

### `test` — test delle regole di gioco

```bash
./main test
```

Verifica le regole di assegnazione del vincitore e dei punti. Non richiede video né
OpenCV a runtime, gira in un istante.

## Come funziona

**Detection** (`CardDetector`) — Canny con soglie ricavate dalla media del fotogramma,
chiusura morfologica, estrazione dei contorni, e filtro geometrico su area, *extent*
(quanto il contorno riempie il suo rettangolo minimo) e rapporto d'aspetto calcolato in
modo invariante alla rotazione. I quattro angoli vengono da `minAreaRect`.

**Rettifica** (`CardRectifier`) — omografia dai quattro angoli a un rettangolo assiale,
per ottenere la carta "scannerizzata".

**Riconoscimento** (`CardRecognizer`) — feature ORB confrontate con i 40 template, ratio
test di Lowe e verifica geometrica con RANSAC: si contano solo i match che stanno in una
singola omografia. La verifica geometrica è necessaria perché le carte numeriche dello
stesso seme hanno le figure ripetute identiche, che i soli descrittori non distinguono.

**Analisi temporale** (`RoundAnalyzer`) — il detector lavora su un fotogramma, ma sapere
chi ha aperto richiede il tempo. Le detection vicine nello spazio vengono raggruppate in
*track*, ogni track accumula un istogramma di etichette e vince la maggioranza. Chi ha
aperto è la carta giocata che compare prima. La briscola non passa dal detector (sotto il
mazzo il suo contorno non si chiude): viene letta con ORB in un ritaglio attorno al mazzo,
ed essendo costante per tutta la partita viene decisa a maggioranza sui 20 round.

**Regole e output** (`GameRules`, `GameReport`, `Metrics`) — moduli senza dipendenze da
OpenCV, quindi verificabili senza video.

## Limiti noti

- **game3** è il sottoinsieme difficile: mani e sovrapposizioni impediscono al contorno
  della carta di chiudersi, e il detector geometrico la perde. Circa un terzo dei round
  viene letto per intero.
- **La briscola di game4** non viene riconosciuta: è piccola nel fotogramma e coperta per
  un terzo dal mazzo, e gli inlier non si distinguono dal rumore. Il sistema in questo
  caso si astiene invece di indovinare, perché una briscola sbagliata falserebbe il
  vincitore di tutti i round.

## Autori

- Francesco Pizzato — detection, rettifica e riconoscimento delle carte
- Facco Filippo — analisi temporale del round, regole di gioco, output e metriche
