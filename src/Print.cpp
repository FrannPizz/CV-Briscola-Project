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
    std::cout << "Briscola round analysis - " << game << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "What the program does:" << std::endl;
    std::cout << " 1. YOLO finds the cards in the videos (json/" << game << ".json)" << std::endl;
    std::cout << " 2. each card is straightened and matched with the templates using SIFT" << std::endl;
    std::cout << " 3. we rebuild the game: the briscola is the card we see in most rounds," << std::endl;
    std::cout << "    the two cards of each round come from the votes and the hungarian" << std::endl;
    std::cout << "    algorithm, the card on top is North's, then leader, winner and points" << std::endl;
    std::cout << " 4. the result is saved in output/ and compared with the ground truth" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
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
