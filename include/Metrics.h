//Author: <il tuo nome>
#ifndef METRICS_H_INCLUDED
#define METRICS_H_INCLUDED

#include <string>
#include <vector>

#include "Card.h"
#include "GameReport.h"

/*
Confronto con il ground truth e calcolo delle 4 metriche della sezione 6 della consegna.
*/

//una riga di gameXresults.csv
struct GroundTruthRound {
    int    round = 0;
    Card   north;
    Card   south;
    Card   briscola;
    Player leader = Player::North;
    Player winner = Player::North;
    int    points = 0;
};

/*
Legge il CSV del ground truth. I campi sono letti PER POSIZIONE nell'ordine della
consegna (Round, North_Number, North_Suit, South_Number, South_Suit,
Briscola_Number, Briscola_Suit, Leader, Winner, Points), cosi' non dipende dalla
grafia esatta dell'intestazione. L'eventuale riga di intestazione viene saltata.
*/
std::vector<GroundTruthRound> loadGroundTruth(const std::string& csvPath);

struct Metrics {
    int  cardsCorrect   = 0;   //su cardsTotal (2 per round)
    int  cardsTotal     = 0;
    int  playersCorrect = 0;   //su playersTotal (leader + winner per round)
    int  playersTotal   = 0;
    bool briscolaCorrect = false;

    bool winnerCorrect     = false;
    bool northScoreCorrect = false;
    bool southScoreCorrect = false;

    int  gtNorthScore = 0;
    int  gtSouthScore = 0;
    std::string gtOverallWinner;

    int  resultFieldsCorrect() const;   //0..3
    double cardAccuracy() const;
    double playerAccuracy() const;
    double resultAccuracy() const;
};

//confronta le predizioni con il ground truth (accoppiando i round per numero)
Metrics evaluate(const GameReport& report, const std::vector<GroundTruthRound>& groundTruth);

void printMetrics(const Metrics& metrics);

#endif
