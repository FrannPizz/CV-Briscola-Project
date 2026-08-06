// Entry point unico del progetto. Quattro sotto-comandi:
//
//   game   analisi di una partita intera: 20 round -> CSV + TXT + metriche
//   round  analisi di UN round, stampando le track trovate (per tarare i parametri)
//   frame  detect + recognize su UN frame, con finestre di debug (per tarare il detector)
//   test   test delle regole di gioco, senza video
//
// Uso:
//   ./main game  <cartella_dati> <game> [--rounds N] [--step N] [--gt file.csv] [--out dir] [--verbose]
//   ./main round <video.mp4> [--step N]
//   ./main frame <video.mp4> [indice_frame]
//   ./main test
//
// Esempi:
//   ./main game ../data game1 --gt ../data/game1results.csv --out ../output
//   ./main round ../data/videos/game1/game1round1.mp4
//   ./main frame ../data/videos/game1/game1round1.mp4 150
//   ./main test

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "../include/Card.h"
#include "../include/CardDetector.h"
#include "../include/CardRecognizer.h"
#include "../include/CardRectifier.h"
#include "../include/GameReport.h"
#include "../include/Metrics.h"
#include "../include/RoundAnalyzer.h"
#include "../include/RulesTests.h"

// ----------------------------------------------------------------- comune

//carica i 40 template una volta sola; stringa vuota se qualcosa non va
static bool loadAllTemplates(std::vector<CardTemplate>& out)
{
    out = loadTemplates();
    std::cout << "Template caricati: " << out.size() << "/40" << std::endl;
    if (out.empty()) {
        std::cerr << "Nessun template caricato: lancia l'eseguibile da build/, "
                     "loadTemplates() usa il path relativo ../data/template/\n";
        return false;
    }
    return true;
}

// ------------------------------------------------------------ comando game

struct GameOptions {
    std::string dataDir;
    std::string game;
    std::string groundTruth;
    std::string outDir = ".";
    int  rounds  = 20;
    int  step    = 2;
    bool verbose = false;
};

static bool parseGameArgs(int argc, char** argv, GameOptions& opt)
{
    //argv[0]=exe, argv[1]="game", argv[2]=dataDir, argv[3]=game
    if (argc < 4)
        return false;

    opt.dataDir = argv[2];
    opt.game    = argv[3];

    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verbose") {
            opt.verbose = true;
        } else if (arg == "--rounds" && i + 1 < argc) {
            opt.rounds = std::stoi(argv[++i]);
        } else if (arg == "--step" && i + 1 < argc) {
            opt.step = std::stoi(argv[++i]);
        } else if (arg == "--gt" && i + 1 < argc) {
            opt.groundTruth = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            opt.outDir = argv[++i];
        } else {
            std::cerr << "Opzione sconosciuta: " << arg << std::endl;
            return false;
        }
    }
    return true;
}

/*
Ground truth da usare quando non e' stato passato --gt. I file CORRECTED sono la
versione buona (correggono carte scambiate, semi sbagliati e campi vuoti): se ci
sono vanno usati loro, altrimenti si misura contro annotazioni sbagliate.
*/
static std::string findGroundTruth(const std::string& dataDir, const std::string& game)
{
    const std::string corrected = dataDir + "/" + game + "resultsCORRECTED.csv";
    if (std::filesystem::exists(corrected))
        return corrected;

    const std::string plain = dataDir + "/" + game + "results.csv";
    if (std::filesystem::exists(plain))
        return plain;

    return "";
}

static int cmdGame(int argc, char** argv)
{
    GameOptions opt;
    if (!parseGameArgs(argc, argv, opt)) {
        std::cerr << "Uso: main game <cartella_dati> <game> "
                     "[--rounds N] [--step N] [--gt file.csv] [--out dir] [--verbose]\n";
        return 1;
    }

    if (opt.groundTruth.empty())
        opt.groundTruth = findGroundTruth(opt.dataDir, opt.game);

    std::vector<CardTemplate> templates;
    if (!loadAllTemplates(templates))
        return 1;

    AnalyzerParams params;
    params.frameStep = opt.step;
    params.verbose   = opt.verbose;

    GameReport report(opt.game);

    //la briscola letta si porta avanti: negli ultimi round viene pescata e sparisce dal tavolo
    Card carriedBriscola;

    for (int round = 1; round <= opt.rounds; ++round) {
        const std::string videoPath = opt.dataDir + "/videos/" + opt.game + "/"
                                    + opt.game + "round" + std::to_string(round) + ".mp4";

        std::cout << "\n[Round " << round << "] " << videoPath << std::endl;

        const RoundResult result = analyzeRound(videoPath, round, templates,
                                                carriedBriscola, params);

        if (result.briscola.valid() && !result.briscolaFromCarry)
            carriedBriscola = result.briscola;

        std::cout << "  North " << cardName(result.north)
                  << " | South " << cardName(result.south)
                  << " | Briscola " << cardName(result.briscola)
                  << (result.briscolaFromCarry ? " (ereditata)" : "")
                  << " | Leader " << playerName(result.leader)
                  << " | Winner " << playerName(result.winner)
                  << " | Punti " << result.points << std::endl;
        if (!result.note.empty())
            std::cout << "  nota: " << result.note << std::endl;

        report.addRound(result);
    }

    //la briscola e' costante per tutta la partita: la decido a maggioranza sui 20 round
    //e ricalcolo vincitori e punteggi con quella
    report.consolidateBriscola();

    const std::string csvPath = opt.outDir + "/" + opt.game + "_output.csv";
    const std::string txtPath = opt.outDir + "/" + opt.game + "_output.txt";
    report.writeCsv(csvPath);
    report.writeTxt(txtPath);
    report.printSummary();
    std::cout << "CSV: " << csvPath << "\nTXT: " << txtPath << std::endl;

    if (opt.groundTruth.empty()) {
        std::cout << "Nessun ground truth trovato: metriche saltate." << std::endl;
    } else {
        //stampato bene in vista: sbagliare file di riferimento falserebbe tutte le metriche
        std::cout << "\nGround truth: " << opt.groundTruth << std::endl;
        const std::vector<GroundTruthRound> gt = loadGroundTruth(opt.groundTruth);
        if (gt.empty()) {
            std::cerr << "Ground truth vuoto o non leggibile: metriche saltate." << std::endl;
        } else {
            const Metrics m = evaluate(report, gt);
            printMetrics(m);

            //le metriche vanno anche su file: sono materiale di consegna
            const std::string metricsPath = opt.outDir + "/" + opt.game + "_metrics.txt";
            writeMetrics(metricsPath, opt.game, opt.groundTruth, report, m);
            std::cout << "Metriche: " << metricsPath << std::endl;
        }
    }

    return 0;
}

// ----------------------------------------------------------- comando round

//analisi di un solo round: serve a vedere le track e capire come tarare AnalyzerParams
static int cmdRound(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Uso: main round <video.mp4> [--step N]\n";
        return 1;
    }
    const std::string videoPath = argv[2];

    AnalyzerParams params;
    params.verbose = true;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--step" && i + 1 < argc)
            params.frameStep = std::stoi(argv[++i]);
    }

    std::vector<CardTemplate> templates;
    if (!loadAllTemplates(templates))
        return 1;

    const Card noCarry;
    const RoundResult r = analyzeRound(videoPath, 1, templates, noCarry, params);

    std::cout << "\nRisultato:\n"
              << "  North    " << cardName(r.north)    << "\n"
              << "  South    " << cardName(r.south)    << "\n"
              << "  Briscola " << cardName(r.briscola) << "\n"
              << "  Leader   " << playerName(r.leader) << "\n"
              << "  Winner   " << playerName(r.winner) << "\n"
              << "  Punti    " << r.points << std::endl;
    if (!r.note.empty())
        std::cout << "  nota: " << r.note << std::endl;

    return 0;
}

// ----------------------------------------------------------- comando frame

//detect + rectify + recognize su un singolo frame, con finestre di debug.
//E' il vecchio main di prova: utile per tarare i filtri del detector.
static int cmdFrame(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Uso: main frame <video.mp4> [indice_frame] [--no-gui]\n";
        return 1;
    }
    const std::string videoPath = argv[2];

    //--no-gui: solo numeri sullo stdout, niente finestre (per diagnosi rapide)
    bool gui = true;
    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-gui")
            gui = false;
    }

    std::vector<CardTemplate> templates;
    if (!loadAllTemplates(templates))
        return 1;

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Impossibile aprire il video: " << videoPath << "\n";
        return 1;
    }

    const int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    const bool hasIndex = (argc > 3 && argv[3][0] != '-');
    const int targetFrame = hasIndex ? std::stoi(argv[3])
                                     : (totalFrames > 0 ? totalFrames / 2 : 0);
    cap.set(cv::CAP_PROP_POS_FRAMES, targetFrame);

    cv::Mat frame;
    if (!cap.read(frame) || frame.empty()) {
        std::cerr << "Impossibile leggere il frame " << targetFrame << "\n";
        return 1;
    }
    std::cout << "Frame " << targetFrame << " / " << totalFrames
              << "  (" << frame.cols << "x" << frame.rows << ")" << std::endl;

    Params params;
    const std::vector<DetectedCard> cards = detect(frame, params);
    std::cout << "Carte rilevate: " << cards.size() << std::endl;

    // --- imbuto del detector: quanti candidati superano ogni filtro ---
    const cv::Mat dbgGray = preprocess(frame);
    const cv::Mat dbgMask = edgeMask(dbgGray, params);
    const std::vector<std::vector<cv::Point>> dbgCand = findCandidates(dbgMask);
    std::cout << "Candidati grezzi (prima del filtro): " << dbgCand.size() << std::endl;

    const double frameArea = static_cast<double>(frame.cols) * frame.rows;
    int passArea = 0, passExtent = 0, passAspect = 0;
    for (const std::vector<cv::Point>& c : dbgCand) {
        const double area  = cv::contourArea(c);
        const double ratio = area / frameArea;
        if (ratio < params.minAreaRatio || ratio > params.maxAreaRatio) continue;
        ++passArea;
        const cv::RotatedRect rr = cv::minAreaRect(c);
        const double rectArea = rr.size.width * rr.size.height;
        if (rectArea <= 0) continue;
        const double extent = area / rectArea;
        const double asp = std::min(rr.size.width, rr.size.height)
                         / std::max(rr.size.width, rr.size.height);

        //valori reali dei candidati sopravvissuti all'area: servono per capire
        //quale soglia sta tagliando e di quanto
        std::cout << "    cand: area=" << ratio << " extent=" << extent
                  << " aspect=" << asp << std::endl;

        if (extent < params.minExtent) continue;
        ++passExtent;
        if (asp < params.minAspect || asp > params.maxAspect) continue;
        ++passAspect;
    }
    std::cout << "  -> dopo area("   << params.minAreaRatio << ".." << params.maxAreaRatio << "): " << passArea
              << " | dopo extent(>=" << params.minExtent << "): " << passExtent
              << " | dopo aspect("   << params.minAspect << ".." << params.maxAspect << "): " << passAspect
              << std::endl;

    cv::Mat candVis = frame.clone();
    cv::drawContours(candVis, dbgCand, -1, cv::Scalar(255, 0, 0), 2);

    if (gui) {
        cv::namedWindow("DEBUG edge mask", cv::WINDOW_NORMAL);
        cv::imshow("DEBUG edge mask", dbgMask);
        cv::namedWindow("DEBUG candidati (blu)", cv::WINDOW_NORMAL);
        cv::imshow("DEBUG candidati (blu)", candVis);
    } else {
        //senza finestre salvo le stesse immagini su file, per guardarle dopo
        const std::string stem = "dbg_f" + std::to_string(targetFrame);
        cv::imwrite(stem + "_frame.png", frame);
        cv::imwrite(stem + "_mask.png",  dbgMask);
        cv::imwrite(stem + "_cand.png",  candVis);
        std::cout << "  immagini di debug salvate: " << stem << "_{frame,mask,cand}.png" << std::endl;
    }

    // --- per ogni carta: raddrizza, riconosci, mostra ---
    cv::Mat vis = frame.clone();
    for (size_t i = 0; i < cards.size(); ++i) {
        const DetectedCard& card = cards[i];

        const cv::Mat warped = rectify(frame, card.corners);
        cv::Mat warpedGray;
        cv::cvtColor(warped, warpedGray, cv::COLOR_BGR2GRAY);
        const int label = recognize(warpedGray, templates);
        const std::string name = cardName(cardFromLabel(label));

        std::cout << "  Carta " << i << ": label=" << label << " (" << name << ")"
                  << "  half=" << (card.half == Half::North ? "North" : "South")
                  << "  centroid=(" << static_cast<int>(card.centroid.x)
                  << "," << static_cast<int>(card.centroid.y) << ")" << std::endl;

        if (!gui)
            continue;

        const std::vector<std::vector<cv::Point>> oneContour = { card.corners };
        cv::polylines(vis, oneContour, true, cv::Scalar(0, 255, 0), 3);
        cv::putText(vis, std::to_string(label), card.centroid,
                    cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 0, 255), 3);

        const std::string win = "Carta " + std::to_string(i) + " -> " + name;
        cv::namedWindow(win, cv::WINDOW_NORMAL);
        cv::imshow(win, warped);

        //template associato, per confronto visivo carta-vs-template
        if (label >= 1 && label <= 40) {
            const cv::Mat tpl = cv::imread("../data/template/" + std::to_string(label) + ".jpg");
            if (!tpl.empty()) {
                const std::string tplWin = "Template " + std::to_string(label) + " (" + name + ")";
                cv::namedWindow(tplWin, cv::WINDOW_NORMAL);
                cv::imshow(tplWin, tpl);
            }
        }
    }

    if (!gui)
        return 0;

    cv::namedWindow("Frame (verde=rilevato, rosso=label)", cv::WINDOW_NORMAL);
    cv::imshow("Frame (verde=rilevato, rosso=label)", vis);

    std::cout << "Premi un tasto su una finestra per uscire..." << std::endl;
    cv::waitKey(0);
    return 0;
}

// ------------------------------------------------------------------- main

static void usage(const char* exe)
{
    std::cerr
        << "Uso:\n"
        << "  " << exe << " game  <cartella_dati> <game> [--rounds N] [--step N] [--gt file.csv] [--out dir] [--verbose]\n"
        << "  " << exe << " round <video.mp4> [--step N]\n"
        << "  " << exe << " frame <video.mp4> [indice_frame]\n"
        << "  " << exe << " test\n\n"
        << "Esempi:\n"
        << "  " << exe << " game ../data game1 --gt ../data/game1results.csv --out ../output\n"
        << "  " << exe << " round ../data/videos/game1/game1round1.mp4\n"
        << "  " << exe << " frame ../data/videos/game1/game1round1.mp4 150\n";
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const std::string command = argv[1];

    if (command == "game")  return cmdGame(argc, argv);
    if (command == "round") return cmdRound(argc, argv);
    if (command == "frame") return cmdFrame(argc, argv);
    if (command == "test")  return runRulesTests();

    std::cerr << "Comando sconosciuto: " << command << "\n\n";
    usage(argv[0]);
    return 1;
}
