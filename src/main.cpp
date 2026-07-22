// MAIN TEMPORANEO di test — riconoscimento carte su un singolo frame.
// NON e' il main finale: serve solo a vedere se detect+rectify+recognize funzionano.
//
// Uso:  ./main <video.mp4> [indice_frame]
//   - senza argomenti: game1round1, frame centrale
//   - con [indice_frame]: usa quel frame specifico

#include <iostream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

#include "../include/CardDetector.h"
#include "../include/CardRectifier.h"
#include "../include/CardRecognizer.h"

// label 1..40 -> stringa leggibile (es. "7 di Coppe").
static std::string labelToName(int label)
{
    if (label < 1 || label > 40) return "??";
    const char* suits[] = { "Spade", "Bastoni", "Coppe", "Denari" };
    int value = (label - 1) % 10 + 1;
    int suit  = (label - 1) / 10;
    return std::to_string(value) + " di " + suits[suit];
}

int main(int argc, char** argv)
{
    // Path assoluto hardcoded per il video; i template hanno il path dentro loadTemplates().
    const std::string BASE = "C:/Users/frann/OneDrive/Desktop/Lab ComputerVision/Final Project/";
    std::string videoPath   = (argc > 1) ? argv[1] : BASE + "data/videos/game1/game1round1.mp4";

    // 1. Carica i 40 template (una volta).
    std::vector<CardTemplate> templates = loadTemplates();
    std::cout << "Template caricati: " << templates.size() << "/40" << std::endl;
    if (templates.empty()) {
        std::cerr << "Nessun template caricato.\n";
        return 1;
    }

    // 2. Apri il video e prendi un frame.
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Impossibile aprire il video: " << videoPath << "\n";
        return 1;
    }

    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    int targetFrame = (argc > 2) ? std::stoi(argv[2])
                                 : (totalFrames > 0 ? totalFrames / 2 : 0);
    cap.set(cv::CAP_PROP_POS_FRAMES, targetFrame);

    cv::Mat frame;
    if (!cap.read(frame) || frame.empty()) {
        std::cerr << "Impossibile leggere il frame " << targetFrame << "\n";
        return 1;
    }
    std::cout << "Frame " << targetFrame << " / " << totalFrames
              << "  (" << frame.cols << "x" << frame.rows << ")" << std::endl;

    // 3. Rileva le carte nel frame (modulo CardDetector).
    Params params;
    std::vector<DetectedCard> cards = detect(frame, params);
    std::cout << "Carte rilevate: " << cards.size() << std::endl;

    // --- DEBUG detector: cosa vede prima del filtro ---
    cv::Mat dbgGray = preprocess(frame);
    cv::Mat dbgMask = edgeMask(dbgGray, params);
    std::vector<std::vector<cv::Point>> dbgCand = findCandidates(dbgMask);
    std::cout << "DEBUG candidati grezzi (prima del filtro): " << dbgCand.size() << std::endl;

    // Imbuto: quanti candidati superano ogni filtro in sequenza.
    double frameArea = static_cast<double>(frame.cols) * frame.rows;
    int passArea = 0, passExtent = 0, passAspect = 0;
    for (const std::vector<cv::Point>& c : dbgCand) {
        double area = cv::contourArea(c);
        double ratio = area / frameArea;
        if (ratio < params.minAreaRatio || ratio > params.maxAreaRatio) continue;
        passArea++;
        cv::RotatedRect rr = cv::minAreaRect(c);
        double rectArea = rr.size.width * rr.size.height;
        if (rectArea <= 0) continue;
        double extent = area / rectArea;
        if (extent < params.minExtent) continue;
        passExtent++;
        double asp = std::min(rr.size.width, rr.size.height) / std::max(rr.size.width, rr.size.height);
        if (asp < params.minAspect || asp > params.maxAspect) continue;
        passAspect++;
    }
    std::cout << "  -> dopo area("  << params.minAreaRatio << ".." << params.maxAreaRatio << "): " << passArea
              << " | dopo extent(>=" << params.minExtent << "): " << passExtent
              << " | dopo aspect(" << params.minAspect << ".." << params.maxAspect << "): " << passAspect << std::endl;
    cv::Mat candVis = frame.clone();
    cv::drawContours(candVis, dbgCand, -1, cv::Scalar(255, 0, 0), 2);
    cv::namedWindow("DEBUG edge mask", cv::WINDOW_NORMAL);
    cv::imshow("DEBUG edge mask", dbgMask);
    cv::namedWindow("DEBUG candidati (blu)", cv::WINDOW_NORMAL);
    cv::imshow("DEBUG candidati (blu)", candVis);
    cv::imwrite("C:/Users/frann/AppData/Local/Temp/claude/C--Users-frann-OneDrive-Desktop-Lab-ComputerVision-Lab8/0924489f-b106-44c1-a010-3e975ee38ef7/scratchpad/dbg_mask.png", dbgMask);
    cv::imwrite("C:/Users/frann/AppData/Local/Temp/claude/C--Users-frann-OneDrive-Desktop-Lab-ComputerVision-Lab8/0924489f-b106-44c1-a010-3e975ee38ef7/scratchpad/dbg_cand.png", candVis);

    // 4. Per ogni carta: raddrizza + riconosci, e mostra il risultato.
    cv::Mat vis = frame.clone();
    for (size_t i = 0; i < cards.size(); ++i) {
        const DetectedCard& card = cards[i];

        cv::Mat warped = rectify(frame, card.corners);

        cv::Mat warpedGray;
        cv::cvtColor(warped, warpedGray, cv::COLOR_BGR2GRAY);
        int label = recognize(warpedGray, templates);

        std::string half = (card.half == Half::North) ? "North" : "South";
        std::cout << "  Carta " << i << ": label=" << label
                  << " (" << labelToName(label) << ")"
                  << "  half=" << half
                  << "  centroid=(" << static_cast<int>(card.centroid.x)
                  << "," << static_cast<int>(card.centroid.y) << ")" << std::endl;

        // Disegna contorno + label sul frame per controllo visivo.
        std::vector<std::vector<cv::Point>> oneContour = { card.corners };
        cv::polylines(vis, oneContour, true, cv::Scalar(0, 255, 0), 3);
        cv::putText(vis, std::to_string(label), card.centroid,
                    cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 0, 255), 3);

        // Mostra la carta raddrizzata in una finestra.
        std::string win = "Carta " + std::to_string(i) + " -> " + labelToName(label);
        cv::namedWindow(win, cv::WINDOW_NORMAL);
        cv::imshow(win, warped);

        // Mostra il TEMPLATE associato, per confronto visivo carta-vs-template.
        if (label >= 1 && label <= 40) {
            cv::Mat tpl = cv::imread("../data/template/" + std::to_string(label) + ".jpg");
            if (!tpl.empty()) {
                std::string tplWin = "Template " + std::to_string(label) + " (" + labelToName(label) + ")";
                cv::namedWindow(tplWin, cv::WINDOW_NORMAL);
                cv::imshow(tplWin, tpl);
            }
        }
    }

    // Mostra il frame annotato.
    cv::namedWindow("Frame (verde=rilevato, rosso=label)", cv::WINDOW_NORMAL);
    cv::imshow("Frame (verde=rilevato, rosso=label)", vis);

    std::cout << "Premi un tasto su una finestra per uscire..." << std::endl;
    cv::waitKey(0);
    return 0;
}
