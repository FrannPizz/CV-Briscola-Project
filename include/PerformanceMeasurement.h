#ifndef PERFORMANCEMEASUREMENT_H_INCLUDED
#define PERFORMANCEMEASUREMENT_H_INCLUDED

#include <string>
#include <vector>
#include <ostream>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <map>
#include <stdexcept>

//one row of a results csv (predicted or ground truth):
//Round,North_Number,North_Suit,South_Number,South_Suit,Briscola_Number,Briscola_Suit,Leader,Winner,Points
struct RoundRow {
    int         round;
    int         northNumber;
    std::string northSuit;
    int         southNumber;
    std::string southSuit;
    int         briscolaNumber;
    std::string briscolaSuit;
    std::string leader;
    std::string winner;
    int         points;
};

//final result of a game, computed from the rows
struct GameResult {
    int         northScore;
    int         southScore;
    std::string winner;        //"north", "south" or "draw"
};

//the four metrics of the project, as correct / total
struct Metrics {
    int correctCards;          //over 40 (2 cards x 20 rounds)
    int totalCards;
    int correctPlayers;        //over 40 (leader + winner x 20 rounds)
    int totalPlayers;
    int correctBriscola;       //over 1
    int totalBriscola;
    int correctResult;         //over 3 (winner, North score, South score)
    int totalResult;

    GameResult predicted;
    GameResult groundTruth;
    std::vector<std::string> errors;   //readable list of the wrong fields
};

std::vector<RoundRow> readResultsCSV(const std::string& csvPath);

GameResult computeGameResult(const std::vector<RoundRow>& rows);

void printGame(const std::string& game, const std::vector<RoundRow>& rows, std::ostream& out);

Metrics evaluateGame(const std::vector<RoundRow>& predicted, const std::vector<RoundRow>& groundTruth, int nRounds = 20);

void printMetrics(const std::string& game, const Metrics& m, std::ostream& out);

bool writeMetrics(const std::string& game, const Metrics& m, const std::string& path);

#endif