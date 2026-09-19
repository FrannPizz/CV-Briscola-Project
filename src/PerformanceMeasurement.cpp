//Author: Filippo Facco
#include "../include/PerformanceMeasurement.h"

/*
this module compares the predicted csv of a game with the ground truth csv and computes the metrics
of the project: card recognition, player identification, briscola recognition and game result accuracy
*/

//remove spaces / \r and lowercase: "  Spades\r" -> "spades"
static std::string normalize(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (!std::isspace((unsigned char) c)) //return true for space, \r, \n, \t, etc.
            out += (char) std::tolower((unsigned char) c);
    }
    return out;
}

//split a csv line on the commas
static std::vector<std::string> splitLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(normalize(field));
    return fields;
}

std::vector<RoundRow> readResultsCSV(const std::string& csvPath)
{
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + csvPath);
    }

    std::vector<RoundRow> rows;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> f = splitLine(line);

        //skip empty and incomplete lines
        if (f.size() < 10)
            continue;

        //the header (or a broken line) does not have numbers: skip it
        try {
            RoundRow row;
            row.round          = std::stoi(f[0]);
            row.northNumber    = std::stoi(f[1]);
            row.northSuit      = f[2];
            row.southNumber    = std::stoi(f[3]);
            row.southSuit      = f[4];
            row.briscolaNumber = std::stoi(f[5]);
            row.briscolaSuit   = f[6];
            row.leader         = f[7];
            row.winner         = f[8];
            row.points         = std::stoi(f[9]);
            rows.push_back(row);
        } catch (const std::exception&) {
            continue;
        }
    }
    return rows;
}

//final scores and winner of the game: the winner of every round takes its points
GameResult computeGameResult(const std::vector<RoundRow>& rows)
{
    GameResult result;
    result.northScore = 0;
    result.southScore = 0;

    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i].winner == "north")
            result.northScore += rows[i].points;
        else if (rows[i].winner == "south")
            result.southScore += rows[i].points;
    }

    if (result.northScore > result.southScore)
        result.winner = "north";
    else if (result.southScore > result.northScore)
        result.winner = "south";
    else
        result.winner = "draw";

    return result;
}

//print every round, the final scores and the winner of the game
//(the same computeGameResult used for the metrics)
void printGame(const std::string& game, const std::vector<RoundRow>& rows, std::ostream& out)
{
    for (int i = 0; i < rows.size(); ++i) {
        const RoundRow& r = rows[i];
        out << "=== ROUND " << r.round << " ===" << std::endl;
        out << "Round " << r.round
            << " | North " << r.northNumber << " " << r.northSuit
            << " | South " << r.southNumber << " " << r.southSuit
            << " | Briscola " << r.briscolaNumber << " " << r.briscolaSuit
            << " | Leader " << r.leader
            << " | Winner " << r.winner
            << " | Points " << r.points << std::endl;
        out << std::endl;
    }

    GameResult result = computeGameResult(rows);
    out << "=== RESULTS ===" << std::endl;
    out << "North: " << result.northScore << "   South: " << result.southScore << std::endl;
    out << "Winner: " << result.winner << std::endl;
}

//true if number and suit are both correct
static bool sameCard(int numberA, const std::string& suitA, int numberB, const std::string& suitB)
{
    return numberA == numberB && suitA == suitB;
}

Metrics evaluateGame(const std::vector<RoundRow>& predicted, const std::vector<RoundRow>& groundTruth, int nRounds)
{
    Metrics m;
    m.correctCards    = 0;
    m.correctPlayers  = 0;
    m.correctBriscola = 0;
    m.correctResult   = 0;

    //the denominators are fixed by the project: a missing round counts as wrong
    m.totalCards    = 2 * nRounds;
    m.totalPlayers  = 2 * nRounds;
    m.totalBriscola = 1;
    m.totalResult   = 3;

    //predicted rows by round number
    std::map<int, const RoundRow*> predByRound;
    for (int i = 0; i < predicted.size(); ++i)
        predByRound[predicted[i].round] = &predicted[i];

    //for every round of the ground truth
    for (int i = 0; i < groundTruth.size(); ++i) {
        const RoundRow& gt = groundTruth[i];
        std::string r = "Round " + std::to_string(gt.round) + ": ";

        if (predByRound.find(gt.round) == predByRound.end()) {
            m.errors.push_back(r + "missing in the prediction");
            continue;
        }
        const RoundRow& pr = *predByRound[gt.round];

        //card recognition: the North card and the South card (number and suit)
        if (sameCard(pr.northNumber, pr.northSuit, gt.northNumber, gt.northSuit))
            m.correctCards++;
        else
            m.errors.push_back(r + "North card " + std::to_string(pr.northNumber) + " " + pr.northSuit
                             + " (gt " + std::to_string(gt.northNumber) + " " + gt.northSuit + ")");

        if (sameCard(pr.southNumber, pr.southSuit, gt.southNumber, gt.southSuit))
            m.correctCards++;
        else
            m.errors.push_back(r + "South card " + std::to_string(pr.southNumber) + " " + pr.southSuit
                             + " (gt " + std::to_string(gt.southNumber) + " " + gt.southSuit + ")");

        //player identification: leader and winner
        if (pr.leader == gt.leader)
            m.correctPlayers++;
        else
            m.errors.push_back(r + "leader " + pr.leader + " (gt " + gt.leader + ")");

        if (pr.winner == gt.winner)
            m.correctPlayers++;
        else
            m.errors.push_back(r + "winner " + pr.winner + " (gt " + gt.winner + ")");
    }

    //briscola: the same for the whole game, compare the one of the first round
    if (!predicted.empty() && !groundTruth.empty()) {
        const RoundRow& pr = predicted[0];
        const RoundRow& gt = groundTruth[0];
        if (sameCard(pr.briscolaNumber, pr.briscolaSuit, gt.briscolaNumber, gt.briscolaSuit))
            m.correctBriscola++;
        else
            m.errors.push_back("Briscola " + std::to_string(pr.briscolaNumber) + " " + pr.briscolaSuit
                             + " (gt " + std::to_string(gt.briscolaNumber) + " " + gt.briscolaSuit + ")");
    }

    //game result: winner, North score, South score
    m.predicted   = computeGameResult(predicted);
    m.groundTruth = computeGameResult(groundTruth);

    if (m.predicted.winner == m.groundTruth.winner)
        m.correctResult++;
    else
        m.errors.push_back("Game winner " + m.predicted.winner + " (gt " + m.groundTruth.winner + ")");

    if (m.predicted.northScore == m.groundTruth.northScore)
        m.correctResult++;
    else
        m.errors.push_back("North score " + std::to_string(m.predicted.northScore)
                         + " (gt " + std::to_string(m.groundTruth.northScore) + ")");

    if (m.predicted.southScore == m.groundTruth.southScore)
        m.correctResult++;
    else
        m.errors.push_back("South score " + std::to_string(m.predicted.southScore)
                         + " (gt " + std::to_string(m.groundTruth.southScore) + ")");

    return m;
}

//one line of a metric: "name correct/total = accuracy"
static void printMetric(std::ostream& out, const std::string& name, int correct, int total)
{
    double accuracy = total > 0 ? (double) correct / total : 0.0;
    out << name << " " << correct << "/" << total << " = " << accuracy << std::endl;
}

void printMetrics(const std::string& game, const Metrics& m, std::ostream& out)
{
    out << "=== Metrics " << game << " ===" << std::endl;
    printMetric(out, "Card recognition accuracy:     ", m.correctCards, m.totalCards);
    printMetric(out, "Player identification accuracy:", m.correctPlayers, m.totalPlayers);
    printMetric(out, "Briscola recognition accuracy: ", m.correctBriscola, m.totalBriscola);
    printMetric(out, "Game result accuracy:          ", m.correctResult, m.totalResult);

    out << "\nPredicted:    North " << m.predicted.northScore << "  South " << m.predicted.southScore
        << "  Winner " << m.predicted.winner << std::endl;
    out << "Ground truth: North " << m.groundTruth.northScore << "  South " << m.groundTruth.southScore
        << "  Winner " << m.groundTruth.winner << std::endl;

    //list of the wrong fields, useful to find where the pipeline fails
    out << "\nErrors (" << m.errors.size() << "):" << std::endl;
    for (int i = 0; i < m.errors.size(); ++i)
        out << "  " << m.errors[i] << std::endl;
}

bool writeMetrics(const std::string& game, const Metrics& m, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    printMetrics(game, m, file);
    return true;
}