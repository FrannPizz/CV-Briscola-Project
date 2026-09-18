//Author: Filippo Facco
#include "../include/Print.h"

/*
this module contains functions to print the results of the game in a human-readable format and to write them to a CSV file
*/

//readable name of a label, e.g. "7 of Cups"
std::string cardName(int label)
{
    if (label < 1 || label > 40)
        return "??";

    std::string suits[4] = { "Spades", "Clubs", "Cups", "Coins" };
    return std::to_string(cardNumber(label)) + " of " + suits[cardSuit(label)];
}

//explanation of the pipeline, printed at the start of the program
void printIntro(const std::string& game)
{
    std::cout << "==================================================================" << std::endl;
    std::cout << "  BRISCOLA ROUND ANALYSIS  -  " << game << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "How it works:" << std::endl;
    std::cout << "  1. Detection    YOLO (CardDetector.py) finds the cards in the videos" << std::endl;
    std::cout << "                  and saves their corners in json/" << game << ".json" << std::endl;
    std::cout << "  2. Recognition  every card is rectified and compared with the 40" << std::endl;
    std::cout << "                  templates (SIFT)" << std::endl;
    std::cout << "  3. Game         briscola = card seen in more rounds; the two cards of" << std::endl;
    std::cout << "                  every round are chosen with comparison and hungarian algorithm" << std::endl;
    std::cout << "                  (every card is played once); the higher card is North;" << std::endl;
    std::cout << "                  then leader, winner and points of every round" << std::endl;
    std::cout << "  4. Evaluation   the csv in output/ is compared with the ground truth" << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << std::endl;
}

//csv in the same format of the ground truth
bool writeCSV(const std::vector<RoundResult>& results, const std::string& csvPath)
{
    std::ofstream file(csvPath);
    if (!file.is_open())
        return false;

    //suit names used by the ground truth
    std::string suits[4] = { "spades", "clubs", "cups", "coins" };

    file << "Round,North_Number,North_Suit,South_Number,South_Suit,Briscola_Number,Briscola_Suit,Leader,Winner,Points" << std::endl;

    for (int i = 0; i < results.size(); ++i) {
        const RoundResult& r = results[i];

        //one line of the csv
        std::string line = std::to_string(r.round) + ","
                         + std::to_string(cardNumber(r.northLabel)) + "," + suits[cardSuit(r.northLabel)] + ","
                         + std::to_string(cardNumber(r.southLabel)) + "," + suits[cardSuit(r.southLabel)] + ","
                         + std::to_string(cardNumber(r.briscolaLabel)) + "," + suits[cardSuit(r.briscolaLabel)] + ","
                         + r.leader + "," + r.winner + "," + std::to_string(r.points);

        file << line << std::endl;
    }
    return true;
}
