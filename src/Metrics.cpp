//Author: Facco Filippo
#include "../include/Metrics.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

/*
this module reads the ground truth annotations and computes the four accuracy
metrics of the assignment: card recognition, player identification, briscola
recognition and final game result
*/

// ------------------------------------------------------------ lettura CSV

//toglie spazi e ritorni a capo ai bordi: i CSV del dataset hanno spazi sparsi
static std::string trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

//spezza una riga sui separatori; nessun campo del ground truth contiene virgole,
//quindi non serve gestire le virgolette
static std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(trim(field));
    return fields;
}

//"10" + "spades" -> carta; campi vuoti o non interpretabili -> carta non valida
static Card parseCard(const std::string& numberField, const std::string& suitField)
{
    Card card;
    if (numberField.empty() || suitField.empty())
        return card;

    try {
        card.number = std::stoi(numberField);
    } catch (const std::exception&) {
        return Card{};
    }
    if (!suitFromName(suitField, card.suit))
        return Card{};
    if (card.number < 1 || card.number > 10)
        return Card{};

    return card;
}

std::vector<GroundTruthRound> loadGroundTruth(const std::string& csvPath)
{
    std::vector<GroundTruthRound> rows;

    std::ifstream in(csvPath);
    if (!in.is_open()) {
        std::cerr << "[Metrics] impossibile aprire il ground truth: " << csvPath << std::endl;
        return rows;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (trim(line).empty())
            continue;

        const std::vector<std::string> f = splitCsvLine(line);
        if (f.size() < 10)
            continue;

        //salta l'intestazione: il primo campo di una riga dati e' un numero
        if (f[0].empty() || !std::isdigit(static_cast<unsigned char>(f[0][0])))
            continue;

        GroundTruthRound r;
        try {
            r.round  = std::stoi(f[0]);
            r.points = std::stoi(f[9]);
        } catch (const std::exception&) {
            std::cerr << "[Metrics] riga non interpretabile: " << line << std::endl;
            continue;
        }

        //il ground truth ha almeno un round con la carta non annotata (game4 round 14):
        //la riga resta valida, ma quella carta e' "sconosciuta" e non sara' mai indovinata
        r.north    = parseCard(f[1], f[2]);
        r.south    = parseCard(f[3], f[4]);
        r.briscola = parseCard(f[5], f[6]);

        if (!r.north.valid() || !r.south.valid())
            std::cerr << "[Metrics] round " << r.round
                      << ": carta non annotata nel ground truth" << std::endl;

        if (!playerFromName(f[7], r.leader))
            std::cerr << "[Metrics] leader non riconosciuto: '" << f[7] << "'" << std::endl;
        if (!playerFromName(f[8], r.winner))
            std::cerr << "[Metrics] winner non riconosciuto: '" << f[8] << "'" << std::endl;

        rows.push_back(r);
    }

    return rows;
}

// ------------------------------------------------------------ metriche

//i 3 campi del risultato finale: vincitore, punteggio North, punteggio South
int Metrics::resultFieldsCorrect() const
{
    return (winnerCorrect ? 1 : 0)
         + (northScoreCorrect ? 1 : 0)
         + (southScoreCorrect ? 1 : 0);
}

//A_card: carte con numero E seme giusti, su 40 (2 per round x 20 round)
double Metrics::cardAccuracy() const
{
    return (cardsTotal > 0) ? static_cast<double>(cardsCorrect) / cardsTotal : 0.0;
}

//A_player: etichette leader e winner corrette, su 40 (2 per round x 20 round)
double Metrics::playerAccuracy() const
{
    return (playersTotal > 0) ? static_cast<double>(playersCorrect) / playersTotal : 0.0;
}

//A_result: campi del risultato finale corretti, su 3
double Metrics::resultAccuracy() const
{
    return resultFieldsCorrect() / 3.0;
}

//confronta le predizioni col ground truth e riempie tutte e quattro le metriche
Metrics evaluate(const GameReport& report, const std::vector<GroundTruthRound>& groundTruth)
{
    Metrics m;

    //indicizzo il ground truth per numero di round: non do per scontato l'ordine
    std::map<int, GroundTruthRound> gtByRound;
    for (const GroundTruthRound& g : groundTruth)
        gtByRound[g.round] = g;

    //la briscola predetta e' la stessa per tutta la partita: prendo quella a maggioranza
    std::map<int, int> briscolaVotes;

    for (const RoundResult& r : report.rounds()) {
        const std::map<int, GroundTruthRound>::const_iterator it = gtByRound.find(r.round);
        if (it == gtByRound.end())
            continue;
        const GroundTruthRound& g = it->second;

        //2 carte per round. Il confronto e' su numero E seme insieme (operator== di Card):
        //mezza carta giusta non conta, come chiede la consegna
        m.cardsTotal += 2;
        if (r.north == g.north) ++m.cardsCorrect;
        if (r.south == g.south) ++m.cardsCorrect;

        //2 etichette giocatore per round: leader e winner
        m.playersTotal += 2;
        if (r.leader == g.leader) ++m.playersCorrect;
        if (r.winner == g.winner) ++m.playersCorrect;

        if (r.briscola.valid())
            briscolaVotes[labelFromCard(r.briscola)] += 1;
    }

    //briscola: una sola predizione per partita, confrontata con quella del ground truth
    if (!groundTruth.empty()) {
        int bestLabel = 0;
        int bestVotes = 0;
        for (const std::pair<const int, int>& kv : briscolaVotes) {
            if (kv.second > bestVotes) {
                bestVotes = kv.second;
                bestLabel = kv.first;
            }
        }
        m.briscolaCorrect = (cardFromLabel(bestLabel) == groundTruth.front().briscola);
    }

    /*
    Totali del ground truth: li ricavo sommando i punti di ogni round al suo vincitore.
    La consegna li riporta solo nei gameXoutput.txt, ma il calcolo coincide con quelli
    (verificato: game1 North 53 / South 67), quindi non serve leggere anche il txt.
    */
    for (const GroundTruthRound& g : groundTruth) {
        if (g.winner == Player::North) m.gtNorthScore += g.points;
        else                           m.gtSouthScore += g.points;
    }
    if (m.gtNorthScore > m.gtSouthScore)      m.gtOverallWinner = "North";
    else if (m.gtSouthScore > m.gtNorthScore) m.gtOverallWinner = "South";
    else                                      m.gtOverallWinner = "Draw";

    m.northScoreCorrect = (report.northScore() == m.gtNorthScore);
    m.southScoreCorrect = (report.southScore() == m.gtSouthScore);
    m.winnerCorrect     = (report.overallWinner() == m.gtOverallWinner);

    return m;
}

//riepilogo a schermo
void printMetrics(const Metrics& m)
{
    std::cout << "\n--- Metriche ---" << std::endl;
    std::cout << "A_card     : " << m.cardsCorrect   << "/" << m.cardsTotal
              << "  (" << m.cardAccuracy() * 100.0 << "%)" << std::endl;
    std::cout << "A_player   : " << m.playersCorrect << "/" << m.playersTotal
              << "  (" << m.playerAccuracy() * 100.0 << "%)" << std::endl;
    std::cout << "A_briscola : " << (m.briscolaCorrect ? 1 : 0) << "/1" << std::endl;
    std::cout << "A_result   : " << m.resultFieldsCorrect() << "/3"
              << "  (winner " << (m.winnerCorrect ? "ok" : "no")
              << ", North "   << (m.northScoreCorrect ? "ok" : "no")
              << ", South "   << (m.southScoreCorrect ? "ok" : "no") << ")" << std::endl;
    std::cout << "Ground truth: North " << m.gtNorthScore
              << " | South " << m.gtSouthScore
              << " | vincitore " << m.gtOverallWinner << std::endl;
}

//stesse metriche su file, in forma leggibile e con il contesto che le ha prodotte
bool writeMetrics(const std::string& path,
                  const std::string& gameName,
                  const std::string& groundTruthPath,
                  const GameReport& report,
                  const Metrics& m)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[Metrics] impossibile scrivere " << path << std::endl;
        return false;
    }

    //intestazione: senza sapere quale ground truth e' stato usato i numeri non sono confrontabili
    out << "Metriche - " << gameName << "\n"
        << "Ground truth: " << groundTruthPath << "\n\n";

    //le quattro metriche della sezione 6 della consegna, con numeratore e denominatore
    //espliciti come richiesto ("number of correct predictions over the total")
    out << "A_card     : " << m.cardsCorrect   << "/" << m.cardsTotal
        << "  (" << m.cardAccuracy() * 100.0 << "%)\n";
    out << "A_player   : " << m.playersCorrect << "/" << m.playersTotal
        << "  (" << m.playerAccuracy() * 100.0 << "%)\n";
    out << "A_briscola : " << (m.briscolaCorrect ? 1 : 0) << "/1\n";
    out << "A_result   : " << m.resultFieldsCorrect() << "/3"
        << "  (" << m.resultAccuracy() * 100.0 << "%)\n\n";

    //dettaglio dei 3 campi di A_result: dice quale dei tre e' sbagliato
    out << "Dettaglio A_result:\n"
        << "  vincitore      predetto " << report.overallWinner()
        << "  |  atteso " << m.gtOverallWinner
        << "  -> " << (m.winnerCorrect ? "ok" : "ERRATO") << "\n"
        << "  punteggio North  predetto " << report.northScore()
        << "  |  atteso " << m.gtNorthScore
        << "  -> " << (m.northScoreCorrect ? "ok" : "ERRATO") << "\n"
        << "  punteggio South  predetto " << report.southScore()
        << "  |  atteso " << m.gtSouthScore
        << "  -> " << (m.southScoreCorrect ? "ok" : "ERRATO") << "\n";

    return true;
}
