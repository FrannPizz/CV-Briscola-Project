//Author: Facco Filippo
#ifndef GAMEREPORT_H_INCLUDED
#define GAMEREPORT_H_INCLUDED

#include <string>
#include <vector>

#include "RoundAnalyzer.h"

/*
Accumula i risultati dei round di una partita e produce gli output di consegna:
  - un CSV con una riga per round;
  - un .txt leggibile con lo stesso contenuto piu' punteggi finali e vincitore.
*/

class GameReport {
public:
    explicit GameReport(const std::string& gameName) : gameName_(gameName) {}

    //aggiunge un round e aggiorna il punteggio cumulativo
    void addRound(const RoundResult& round);

    /*
    La briscola e' la stessa per tutta la partita (verificato sui gameXresults.csv:
    la colonna Briscola e' costante su tutti e 20 i round). Conviene quindi decidere
    UNA volta sola a maggioranza fra i round in cui l'abbiamo vista, riscriverla su
    tutti e ricalcolare vincitori e punteggi: un round in cui la briscola era coperta
    o letta male viene cosi' recuperato dagli altri 19.
    Da chiamare dopo aver aggiunto tutti i round e prima di scrivere gli output.
    */
    void consolidateBriscola();

    int northScore() const { return northScore_; }
    int southScore() const { return southScore_; }

    //vincitore della partita; in caso di 60-60 e' pareggio (parita' -> "Draw")
    std::string overallWinner() const;

    const std::vector<RoundResult>& rounds() const { return rounds_; }

    //CSV nel formato della consegna
    bool writeCsv(const std::string& path) const;

    //testo leggibile: round per round + totali + vincitore
    bool writeTxt(const std::string& path) const;

    //riepilogo a schermo
    void printSummary() const;

private:
    std::string             gameName_;
    std::vector<RoundResult> rounds_;
    int northScore_ = 0;
    int southScore_ = 0;
};

#endif
