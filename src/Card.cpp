//Author: Facco Filippo
#include "../include/Card.h"

#include <algorithm>
#include <cctype>

/*
this module converts between the recognizer labels (1-40) and the (number, suit)
form required by the CSV, and holds the two tables that define the game: how many
points each card is worth and how strong it is
*/

//label 1-40 -> carta; le decine danno il seme, il resto il numero
Card cardFromLabel(int label)
{
    //fuori range: carta non valida, number resta 0
    if (label < 1 || label > 40)
        return Card{0, Suit::Spade};

    Card card;
    card.number = (label - 1) % 10 + 1;              //1..10 dentro il seme
    card.suit   = static_cast<Suit>((label - 1) / 10); //0=Spade 1=Bastoni 2=Coppe 3=Denari
    return card;
}

//inversa di cardFromLabel; carta non valida -> 0, che il recognizer usa come "sconosciuta"
int labelFromCard(const Card& card)
{
    if (!card.valid())
        return 0;
    return static_cast<int>(card.suit) * 10 + card.number;
}

//nome italiano del seme, per la console e il file .txt
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

//nome del seme come lo scrive il ground truth: il nostro CSV deve usare questo,
//non suitName(), altrimenti il confronto con le annotazioni non e' diretto
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

//carta per esteso, es. "7 di Coppe"; "??" segnala a colpo d'occhio una lettura mancata
std::string cardName(const Card& card)
{
    if (!card.valid())
        return "??";
    return std::to_string(card.number) + " di " + suitName(card.suit);
}

//North e South sono le due direzioni fisse del tavolo, non due persone
std::string playerName(Player player)
{
    return (player == Player::North) ? "North" : "South";
}

//l'avversario: comodo perche' in una mano a due chi non vince perde
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

//seme dal nome letto nel CSV; false se la stringa non e' riconosciuta, cosi' il
//chiamante puo' trattare la carta come non annotata invece di inventarsi un seme
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

//giocatore dal nome letto nel CSV
bool playerFromName(const std::string& name, Player& out)
{
    const std::string n = normalize(name);
    //"sud" e' un refuso presente nel ground truth (game4results.csv, round 1)
    if (n == "north" || n == "nord") { out = Player::North; return true; }
    if (n == "south" || n == "sud")  { out = Player::South; return true; }
    return false;
}

//punti della carta (Tabella 1 della consegna). Una carta non riconosciuta vale 0:
//i punti della mano risultano piu' bassi del vero, ma non inventano punteggio
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

/*
forza della carta per i confronti: 1 > 3 > 10 > 9 > 8 > 7 > 6 > 5 > 4 > 2.
Non coincide con i punti (il 3 vale 10 punti ma perde contro l'Asso) ne' col numero
(il 3 batte il 10), quindi serve una tabella a parte.
*/
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
