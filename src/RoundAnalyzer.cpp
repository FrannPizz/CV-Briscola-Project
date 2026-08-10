//Author: Facco Filippo
#include "../include/RoundAnalyzer.h"
#include "../include/CardRectifier.h"
#include "../include/GameRules.h"

#include <algorithm>
#include <map>
#include <iostream>

#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

/*
this module turns a round video into a structured result. The detector works on a
single frame, but the assignment asks for facts that only exist in time (who played
first), so here the detections are followed across frames, grouped into tracks, and
the labels are decided by majority vote instead of trusting any single frame
*/

// ---------------------------------------------------------------- CardTrack

//label a maggioranza fra tutte quelle viste su questa track.
//Lo 0 (non riconosciuta) e' escluso apposta: una carta vista male in meta' dei frame
//e bene nell'altra meta' deve comunque risultare riconosciuta
int CardTrack::bestLabel() const
{
    int best = 0;
    int bestCount = 0;
    for (const std::pair<const int, int>& kv : labelVotes) {
        if (kv.first != 0 && kv.second > bestCount) {
            bestCount = kv.second;
            best = kv.first;
        }
    }
    return best;
}

//quanti voti ha preso la label vincente: misura quanto e' solida la lettura
int CardTrack::bestVotes() const
{
    int bestCount = 0;
    for (const std::pair<const int, int>& kv : labelVotes) {
        if (kv.first != 0 && kv.second > bestCount)
            bestCount = kv.second;
    }
    return bestCount;
}

// ------------------------------------------------------------ collectTracks

//aggiunge una detection alla track piu' vicina, o ne crea una nuova
static void addObservation(std::vector<CardTrack>& tracks,
                           const DetectedCard& card,
                           int label,
                           int frameIndex,
                           double mergeRadius)
{
    int bestIndex = -1;
    double bestDistance = mergeRadius;

    for (size_t i = 0; i < tracks.size(); ++i) {
        const double d = cv::norm(tracks[i].centroid - card.centroid);
        if (d < bestDistance) {
            bestDistance = d;
            bestIndex = static_cast<int>(i);
        }
    }

    if (bestIndex < 0) {
        CardTrack fresh;
        fresh.centroid   = card.centroid;
        fresh.half       = card.half;
        fresh.firstFrame = frameIndex;
        fresh.lastFrame  = frameIndex;
        fresh.sightings  = 1;
        fresh.namedSightings = (label != 0) ? 1 : 0;
        fresh.labelVotes[label] = 1;
        tracks.push_back(fresh);
        return;
    }

    CardTrack& t = tracks[bestIndex];
    //media incrementale della posizione: la track si "assesta" sul centro vero
    const float n = static_cast<float>(t.sightings);
    t.centroid = (t.centroid * n + card.centroid) / (n + 1.0f);
    t.lastFrame = frameIndex;
    t.sightings += 1;
    if (label != 0)
        t.namedSightings += 1;
    t.labelVotes[label] += 1;
    //la meta' la fissa la prima detection: una carta non cambia lato dopo essere stata giocata
}

std::vector<CardTrack> collectTracks(const std::string& videoPath,
                                     const std::vector<CardTemplate>& templates,
                                     const AnalyzerParams& params)
{
    std::vector<CardTrack> tracks;

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "[RoundAnalyzer] impossibile aprire il video: " << videoPath << std::endl;
        return tracks;
    }

    cv::Mat frame;
    int frameIndex = -1;
    const int step = std::max(1, params.frameStep);

    while (cap.read(frame)) {
        ++frameIndex;
        if (frameIndex % step != 0)
            continue;
        if (frame.empty())
            continue;

        const std::vector<DetectedCard> detected = detect(frame, params.detector);

        for (const DetectedCard& card : detected) {
            const cv::Mat warped = rectify(frame, card.corners);
            if (warped.empty())
                continue;

            cv::Mat warpedGray;
            cv::cvtColor(warped, warpedGray, cv::COLOR_BGR2GRAY);

            //0 = non riconosciuta: la registro comunque, perche' la posizione e i tempi
            //servono anche per le carte a faccia in giu' (mazzo, mazzetti raccolti)
            const int label = recognize(warpedGray, templates);

            addObservation(tracks, card, label, frameIndex, params.mergeRadius);
        }
    }

    cap.release();

    //scarta le track troppo brevi: quasi sempre falsi positivi del detector o mani di passaggio
    std::vector<CardTrack> kept;
    for (const CardTrack& t : tracks) {
        if (t.sightings >= params.minSightings)
            kept.push_back(t);
    }

    if (params.verbose) {
        std::cout << "  track: " << kept.size() << " (grezze " << tracks.size() << ")" << std::endl;
        for (size_t i = 0; i < kept.size(); ++i) {
            const CardTrack& t = kept[i];
            std::cout << "    [" << i << "] " << cardName(t.card())
                      << "  half=" << (t.half == Half::North ? "N" : "S")
                      << "  centro=(" << static_cast<int>(t.centroid.x) << ","
                      << static_cast<int>(t.centroid.y) << ")"
                      << "  frame " << t.firstFrame << ".." << t.lastFrame
                      << "  viste=" << t.sightings
                      << "  nominate=" << t.namedSightings
                      << "  voti=" << t.bestVotes() << std::endl;
        }
    }

    return kept;
}

// ------------------------------------------------------ selezione delle track

int findDeckTrack(const std::vector<CardTrack>& tracks,
                  int sampledFrames,
                  const AnalyzerParams& params)
{
    const int earlyLimit = static_cast<int>(sampledFrames * params.earlyFraction);

    int bestIndex = -1;
    int bestSightings = 0;

    for (size_t i = 0; i < tracks.size(); ++i) {
        const CardTrack& t = tracks[i];

        if (t.bestLabel() != 0)        continue;  //il mazzo e' coperto: mai riconosciuto
        if (t.firstFrame > earlyLimit) continue;  //c'e' fin dall'inizio del round

        //il mazzo e' la presenza piu' costante del tavolo
        if (t.sightings > bestSightings) {
            bestSightings = t.sightings;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

/*
La briscola non passa dal detector: sta sotto il mazzo, il suo contorno non si chiude
e isCardQuad() la scarta sempre. Quindi si va di ORB diretto sui primi frame, dove la
briscola c'e' di sicuro, e si vota fra i frame.
*/
Card recognizeBriscola(const std::string& videoPath,
                       const std::vector<CardTemplate>& templates,
                       const AnalyzerParams& params,
                       const cv::Rect& roi)
{
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened())
        return Card{};

    //ORB con molti piu' keypoint del default: qui l'immagine e' il frame intero,
    //non un ritaglio di carta, e la tovaglia ne consuma parecchi
    cv::Ptr<cv::ORB> orb = cv::ORB::create(params.briscolaFeatures);

    std::map<int, int> votes;

    for (int i = 0; i < params.briscolaFrames; ++i) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty())
            break;

        //ritaglio, se richiesto: va intersecato col frame, il centro puo' stare al bordo
        const cv::Rect clipped = roi.area() > 0 ? (roi & cv::Rect(0, 0, frame.cols, frame.rows))
                                                : cv::Rect(0, 0, frame.cols, frame.rows);
        if (clipped.area() <= 0)
            continue;

        cv::Mat gray;
        cv::cvtColor(frame(clipped), gray, cv::COLOR_BGR2GRAY);

        std::vector<cv::KeyPoint> keypoints;
        cv::Mat descriptors;
        orb->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
        if (descriptors.empty())
            continue;

        //miglior template e secondo migliore, in una passata sola.
        //Inlier e non match grezzi: qui l'immagine contiene molto sfondo, e lo sfondo
        //matcha un po' con tutti i template senza mai stare in un'omografia coerente
        int bestLabel = 0, bestCount = 0, secondCount = 0;
        for (const CardTemplate& t : templates) {
            const int n = countInliers(keypoints, descriptors, t.keypoints, t.descriptors);
            if (n > bestCount) {
                secondCount = bestCount;
                bestCount   = n;
                bestLabel   = t.label;
            } else if (n > secondCount) {
                secondCount = n;
            }
        }

        //il margine sul secondo classificato e' quello che distingue una lettura
        //vera dal rumore del tessuto, che matcha un po' con tutti i template
        const bool confident = bestCount >= params.briscolaMinMatches
                            && bestCount >= params.briscolaMinRatio * std::max(1, secondCount);
        if (confident)
            votes[bestLabel] += 1;
    }

    cap.release();

    int winner = 0, winnerVotes = 0;
    for (const std::pair<const int, int>& kv : votes) {
        if (kv.second > winnerVotes) {
            winnerVotes = kv.second;
            winner = kv.first;
        }
    }

    return cardFromLabel(winner);
}

void selectPlayedTracks(const std::vector<CardTrack>& tracks,
                        int briscolaLabel,
                        int sampledFrames,
                        const AnalyzerParams& params,
                        int& northIndex,
                        int& southIndex)
{
    northIndex = -1;
    southIndex = -1;

    //una carta gia' presente qui all'inizio del round non e' stata giocata adesso
    const int earlyLimit = static_cast<int>(sampledFrames * params.earlyFraction);

    //candidate: carte riconosciute, esclusa la briscola ferma sul tavolo
    std::vector<int> candidates;
    for (size_t i = 0; i < tracks.size(); ++i) {
        const CardTrack& t = tracks[i];

        /*
        Una carta giocata COMPARE durante il round. Briscola, mazzo e mazzetti raccolti
        sono sul tavolo fin dal primo fotogramma, quindi si escludono per tempo di
        comparsa e non per identita': l'esclusione basata sulla label della briscola
        fallisce quando la briscola e' stata letta male, ed e' cosi' che in game4 la
        briscola finiva selezionata come carta di North.
        */
        if (t.firstFrame <= 2 * std::max(1, params.frameStep))
            continue;

        //la briscola ferma sul tavolo va esclusa; se invece la stessa carta compare
        //a meta' round, vuol dire che e' stata pescata e giocata, e allora vale
        if (briscolaLabel != 0 && t.bestLabel() == briscolaLabel && t.firstFrame <= earlyLimit)
            continue;
        if (t.bestLabel() == 0)                          continue;
        if (t.namedSightings < params.minNamedSightings) continue;

        candidates.push_back(static_cast<int>(i));
    }

    if (candidates.empty())
        return;

    //tengo le due su cui il recognizer e' stato sicuro piu' a lungo
    std::sort(candidates.begin(), candidates.end(),
              [&tracks](int a, int b) {
                  return tracks[a].namedSightings > tracks[b].namedSightings;
              });

    if (candidates.size() == 1) {
        //una carta sola: senza un secondo termine di paragone resta il criterio assoluto
        const int only = candidates[0];
        if (tracks[only].half == Half::North) northIndex = only;
        else                                  southIndex = only;
        return;
    }

    //due carte: vale la posizione RELATIVA, non la meta' del fotogramma
    const int a = candidates[0];
    const int b = candidates[1];
    if (tracks[a].centroid.y <= tracks[b].centroid.y) {
        northIndex = a;
        southIndex = b;
    } else {
        northIndex = b;
        southIndex = a;
    }
}

// ------------------------------------------------------------- analyzeRound

RoundResult analyzeRound(const std::string& videoPath,
                         int roundNumber,
                         const std::vector<CardTemplate>& templates,
                         const Card& carriedBriscola,
                         const AnalyzerParams& params)
{
    RoundResult result;
    result.round = roundNumber;

    const std::vector<CardTrack> tracks = collectTracks(videoPath, templates, params);
    if (tracks.empty()) {
        result.note = "nessuna track: video non letto o detector a vuoto";
        return result;
    }

    //quanti frame ho campionato davvero: mi serve per le soglie relative
    int lastFrame = 0;
    for (const CardTrack& t : tracks)
        lastFrame = std::max(lastFrame, t.lastFrame);
    const int sampledFrames = lastFrame / std::max(1, params.frameStep) + 1;

    // --- briscola: ORB attorno al mazzo, non dalle track (il suo contorno non si chiude) ---
    int frameWidth = 0;
    {
        cv::VideoCapture probe(videoPath);
        frameWidth = static_cast<int>(probe.get(cv::CAP_PROP_FRAME_WIDTH));
    }

    cv::Rect briscolaRoi;   //vuoto = tutto il frame
    const int deckIndex = findDeckTrack(tracks, sampledFrames, params);
    if (deckIndex >= 0) {
        //il ritaglio e' quadrato e centrato sul mazzo: la briscola gli sta accanto,
        //ma da che lato dipende da come e' stato apparecchiato il tavolo
        const cv::Point2f c = tracks[deckIndex].centroid;
        const int half = static_cast<int>(params.briscolaRoiRatio * frameWidth);
        briscolaRoi = cv::Rect(static_cast<int>(c.x) - half, static_cast<int>(c.y) - half,
                               2 * half, 2 * half);
    }

    //prima il ritaglio (meno sfondo, margine piu' netto), poi il frame intero come
    //ripiego: quale dei due funzioni dipende da dove sta la briscola rispetto al
    //mazzo, e su alcune partite il ritaglio la taglia via
    Card seen = recognizeBriscola(videoPath, templates, params, briscolaRoi);
    if (!seen.valid() && briscolaRoi.area() > 0)
        seen = recognizeBriscola(videoPath, templates, params, cv::Rect());
    if (seen.valid()) {
        result.briscola = seen;
    } else if (carriedBriscola.valid()) {
        //negli ultimi round la briscola e' stata pescata e non e' piu' sul tavolo
        result.briscola = carriedBriscola;
        result.briscolaFromCarry = true;
    } else {
        result.note += "briscola non determinata; ";
    }

    // --- carte giocate ---
    int northIndex = -1;
    int southIndex = -1;
    selectPlayedTracks(tracks, labelFromCard(result.briscola), sampledFrames,
                       params, northIndex, southIndex);

    if (northIndex >= 0) result.north = tracks[northIndex].card();
    else                 result.note += "carta North non trovata; ";

    if (southIndex >= 0) result.south = tracks[southIndex].card();
    else                 result.note += "carta South non trovata; ";

    result.complete = result.north.valid() && result.south.valid();

    // --- leader: chi compare prima nel tempo ---
    if (northIndex >= 0 && southIndex >= 0) {
        result.leader = (tracks[northIndex].firstFrame <= tracks[southIndex].firstFrame)
                        ? Player::North : Player::South;
    } else if (northIndex >= 0) {
        result.leader = Player::North;
    } else if (southIndex >= 0) {
        result.leader = Player::South;
    }

    // --- vincitore e punti ---
    PlayedCard first, second;
    if (result.leader == Player::North) {
        first  = PlayedCard{result.north, Player::North};
        second = PlayedCard{result.south, Player::South};
    } else {
        first  = PlayedCard{result.south, Player::South};
        second = PlayedCard{result.north, Player::North};
    }

    result.winner = roundWinner(first, second, result.briscola);
    result.points = roundPoints(result.north, result.south);

    return result;
}
