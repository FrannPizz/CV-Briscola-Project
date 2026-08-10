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

Il valore consigliato di `--step` è il default 2. Alzarlo velocizza ma perde accuratezza:
le carte giocate restano visibili poche decine di fotogrammi, e campionando troppo rado
non si raccolgono abbastanza conferme per distinguerle dal rumore. Misurato su game3,
passare da `--step 5` a `--step 2` vale 15 punti di `A_card`.

Con `--step 2` una partita richiede circa 20-30 minuti, a seconda della macchina.

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

## Risultati

Misurati sui ground truth `*CORRECTED*`, con i parametri di default.

| | A_card | A_player | A_briscola | A_result |
|---|---|---|---|---|
| game1 | 37/40 (92.5%) | 40/40 (100%) | 1/1 | 1/3 |
| game2 | 40/40 (100%) | 40/40 (100%) | 1/1 | 3/3 |
| game3 | 34/40 (85%) | 36/40 (90%) | 1/1 | 0/3 |
| game4 | 40/40 (100%) | 40/40 (100%) | 0/1 | 3/3 |

## Limiti noti

Gli errori residui sono di due tipi, con cause distinte.

- **Numero delle carte a pips** (3 errori, tutti su game1). Le carte numeriche dello
  stesso seme hanno le figure ripetute identiche: ORB identifica sempre il seme
  correttamente, ma fra `4 coins` e `6 coins` la differenza è *quanti* simboli ci sono,
  informazione che i descrittori locali non contengono. La verifica geometrica con RANSAC
  riduce il problema ma non lo elimina. Un rimedio sarebbe contare i pips sulla carta
  rettificata, affiancando il conteggio al matching di feature.

- **Carte non rilevate per occlusione** (5 errori, tutti su game3). Quando una mano copre
  la carta o due carte si toccano, il contorno non si chiude e il filtro sull'extent la
  scarta. È il limite strutturale di un detector basato sui contorni: un detector
  addestrato (la pipeline YOLO in `training/`) non avrebbe questo vincolo.

- **Numero della briscola su game4**. È piccola nel fotogramma e coperta per un terzo dal
  mazzo: gli inlier non si distinguono dal rumore e il sistema si astiene invece di
  indovinare. Il seme viene comunque letto correttamente, e siccome la regola del
  vincitore usa solo il seme, questo non influisce su `A_player` né su `A_result`: game4
  è al 100% su entrambe nonostante `A_briscola` sia 0/1.

- **`A_result` è una metrica tutto-o-niente**: richiede i punteggi finali esatti, quindi
  basta una carta sbagliata perché due dei tre campi risultino errati. Le partite a zero
  errori (game2, game4) sono 3/3; game1, con 3 carte sbagliate su 40, è 1/3.

## Autori

- Francesco Pizzato — detection, rettifica e riconoscimento delle carte
- Facco Filippo — analisi temporale del round, regole di gioco, output e metriche
