//Author: <il tuo nome>
#include "../include/Card.h"

#include <algorithm>
#include <cctype>

Card cardFromLabel(int label)
{
    if (label < 1 || label > 40)
        return Card{0, Suit::Spade};

    Card card;
    card.number = (label - 1) % 10 + 1;
    card.suit   = static_cast<Suit>((label - 1) / 10);
    return card;
}

int labelFromCard(const Card& card)
{
    if (!card.valid())
        return 0;
    return static_cast<int>(card.suit) * 10 + card.number;
}

std::string suitName(Suit suit)
{
    switch (suit) {
        case Suit::Spade:   return "Spade";
        case Suit::Bastoni: return "Bastoni";
        case Suit::Coppe:   return "Coppe";
        case Suit::Denari:  return "Denari";
    }
    return "??";
}

std::string suitCsvName(Suit suit)
{
    //nomi da mazzo francese usati dal ground truth: clubs = Bastoni, coins = Denari
    switch (suit) {
        case Suit::Spade:   return "spades";
        case Suit::Bastoni: return "clubs";
        case Suit::Coppe:   return "cups";
        case Suit::Denari:  return "coins";
    }
    return "";
}

std::string cardName(const Card& card)
{
    if (!card.valid())
        return "??";
    return std::to_string(card.number) + " di " + suitName(card.suit);
}

std::string playerName(Player player)
{
    return (player == Player::North) ? "North" : "South";
}

Player otherPlayer(Player player)
{
    return (player == Player::North) ? Player::South : Player::North;
}

//minuscolo + niente spazi ai bordi, per confrontare stringhe che arrivano dal CSV
static std::string normalize(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c)))
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool suitFromName(const std::string& name, Suit& out)
{
    //grafia usata dai file gameXresults.csv del dataset
    const std::string n = normalize(name);
    if (n == "spades") { out = Suit::Spade;   return true; }
    if (n == "clubs")  { out = Suit::Bastoni; return true; }
    if (n == "cups")   { out = Suit::Coppe;   return true; }
    if (n == "coins")  { out = Suit::Denari;  return true; }
    return false;
}

bool playerFromName(const std::string& name, Player& out)
{
    const std::string n = normalize(name);
    //"sud" e' un refuso presente nel ground truth (game4results.csv, round 1)
    if (n == "north" || n == "nord") { out = Player::North; return true; }
    if (n == "south" || n == "sud")  { out = Player::South; return true; }
    return false;
}

int cardPoints(const Card& card)
{
    if (!card.valid())
        return 0;

    switch (card.number) {
        case 1:  return 11;   //Asso
        case 3:  return 10;
        case 10: return 4;    //Re
        case 9:  return 3;    //Cavallo
        case 8:  return 2;    //Fante
        default: return 0;    //7,6,5,4,2 non danno punti
    }
}

int cardStrength(const Card& card)
{
    if (!card.valid())
        return -1;

    //dalla piu' debole alla piu' forte: l'indice nel vettore e' la forza
    static const int ORDER[] = { 2, 4, 5, 6, 7, 8, 9, 10, 3, 1 };

    const int n = static_cast<int>(sizeof(ORDER) / sizeof(ORDER[0]));
    for (int i = 0; i < n; ++i) {
        if (ORDER[i] == card.number)
            return i;
    }
    return -1;
}
