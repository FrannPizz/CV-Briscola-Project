//Author: Facco Filippo
#include "../include/GameReport.h"
#include "../include/GameRules.h"

#include <fstream>
#include <iostream>
#include <map>

/*
this module accumulates the rounds of a game and writes the deliverables: the
per-round CSV in the format of the assignment and a readable TXT with the final
scores and the overall winner
*/

//aggiunge un round e accredita i suoi punti al vincitore
void GameReport::addRound(const RoundResult& round)
{
    rounds_.push_back(round);

    //in Briscola i punti della mano vanno tutti a chi la vince: nessuna divisione
    if (round.winner == Player::North)
        northScore_ += round.points;
    else
        southScore_ += round.points;
}

//"numero,seme" per il CSV, con la grafia del ground truth; carta non riconosciuta
//-> due campi vuoti, cosi' si distingue a colpo d'occhio da una predizione sbagliata
static std::string csvCard(const Card& card)
{
    if (!card.valid())
        return ",";
    return std::to_string(card.number) + "," + suitCsvName(card.suit);
}

void GameReport::consolidateBriscola()
{
    //maggioranza fra le briscole lette direttamente dai video
    std::map<int, int> votes;
    for (const RoundResult& r : rounds_) {
        if (r.briscola.valid() && !r.briscolaFromCarry)
            votes[labelFromCard(r.briscola)] += 1;
    }
    if (votes.empty())
        return;

    //la label piu' votata vince; basta un round letto bene per correggere gli altri 19
    int bestLabel = 0;
    int bestVotes = 0;
    for (const std::pair<const int, int>& kv : votes) {
        if (kv.second > bestVotes) {
            bestVotes = kv.second;
            bestLabel = kv.first;
        }
    }

    //stampato per tenere d'occhio quanto e' solido il voto: 1 round su 20 e' fragile
    const Card briscola = cardFromLabel(bestLabel);
    std::cout << "Briscola della partita: " << cardName(briscola)
              << " (" << bestVotes << " round su " << rounds_.size() << ")" << std::endl;

    //riscrive la briscola su tutti i round e ricalcola vincitore, punti e totali
    northScore_ = 0;
    southScore_ = 0;

    for (RoundResult& r : rounds_) {
        //flag di provenienza: true se in quel round la briscola non era stata letta cosi'
        r.briscolaFromCarry = (r.briscola != briscola);
        r.briscola = briscola;

        //roundWinner vuole le carte in ordine di GIOCO, non di posizione sul tavolo
        PlayedCard first, second;
        if (r.leader == Player::North) {
            first  = PlayedCard{r.north, Player::North};
            second = PlayedCard{r.south, Player::South};
        } else {
            first  = PlayedCard{r.south, Player::South};
            second = PlayedCard{r.north, Player::North};
        }

        r.winner = roundWinner(first, second, briscola);
        r.points = roundPoints(r.north, r.south);

        if (r.winner == Player::North) northScore_ += r.points;
        else                           southScore_ += r.points;
    }
}

//vincitore della partita; i punti in gioco sono 120, quindi il 60-60 e' possibile
std::string GameReport::overallWinner() const
{
    if (northScore_ > southScore_) return "North";
    if (southScore_ > northScore_) return "South";
    return "Draw";
}

//CSV di consegna: una riga per round, stesso tracciato dei gameXresults.csv
bool GameReport::writeCsv(const std::string& path) const
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[GameReport] impossibile scrivere " << path << std::endl;
        return false;
    }

    //intestazione identica a quella dei gameXresults.csv del dataset
    out << "Round,North_Number,North_Suit,South_Number,South_Suit,"
           "Briscola_Number,Briscola_Suit,Leader,Winner,Points\n";

    for (const RoundResult& r : rounds_) {
        out << r.round << ','
            << csvCard(r.north)    << ','
            << csvCard(r.south)    << ','
            << csvCard(r.briscola) << ','
            << playerName(r.leader) << ','
            << playerName(r.winner) << ','
            << r.points << '\n';
    }

    return true;
}

//TXT leggibile: stesso contenuto del CSV piu' totali e vincitore, come i gameXoutput.txt
bool GameReport::writeTxt(const std::string& path) const
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[GameReport] impossibile scrivere " << path << std::endl;
        return false;
    }

    out << "Partita: " << gameName_ << "\n\n";

    //la nota in coda dice perche' un round e' incompleto: serve a leggere gli errori
    for (const RoundResult& r : rounds_) {
        out << "Round " << r.round << ": "
            << "North " << cardName(r.north) << " | "
            << "South " << cardName(r.south) << " | "
            << "Briscola " << cardName(r.briscola)
            << (r.briscolaFromCarry ? " (ereditata)" : "") << " | "
            << "Leader " << playerName(r.leader) << " | "
            << "Winner " << playerName(r.winner) << " | "
            << "Punti " << r.points;
        if (!r.note.empty())
            out << "   [" << r.note << "]";
        out << "\n";
    }

    out << "\nPunteggio finale:\n"
        << "  North: " << northScore_ << "\n"
        << "  South: " << southScore_ << "\n"
        << "Vincitore: " << overallWinner() << "\n";

    return true;
}

void GameReport::printSummary() const
{
    std::cout << "\n=== " << gameName_ << " ===" << std::endl;
    std::cout << "North: " << northScore_ << "   South: " << southScore_
              << "   ->  " << overallWinner() << std::endl;

    //quanti round sono stati letti per intero: indicatore veloce di quanto ha funzionato
    int complete = 0;
    for (const RoundResult& r : rounds_)
        if (r.complete) ++complete;
    std::cout << "Round con entrambe le carte riconosciute: "
              << complete << "/" << rounds_.size() << std::endl;
}
