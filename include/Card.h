//Author: Facco Filippo
#ifndef CARD_H_INCLUDED
#define CARD_H_INCLUDED

#include <string>

/*
Rappresentazione di una carta come (numero, seme), che e' il formato richiesto dal
CSV di consegna. Il resto della pipeline lavora con la "label" 1..40 del recognizer:
qui stanno le conversioni fra i due mondi, piu' punti e forza di ogni carta.

Convenzione label (verificata sui template in data/template/):
  1-10  Spade    (1.jpg  = Asso di Spade)
  11-20 Bastoni  (11.jpg = Asso di Bastoni)
  21-30 Coppe    (23.jpg = 3 di Coppe)
  31-40 Denari   (40.jpg = Re di Denari)
Il numero va da 1 a 10, dove 8 = Fante, 9 = Cavallo, 10 = Re.
*/

enum class Suit { Spade = 0, Bastoni = 1, Coppe = 2, Denari = 3 };

enum class Player { North, South };

struct Card {
    int  number = 0;              //1..10; 0 = carta sconosciuta / non riconosciuta
    Suit suit   = Suit::Spade;

    bool valid() const { return number >= 1 && number <= 10; }
    bool operator==(const Card& o) const { return number == o.number && suit == o.suit; }
    bool operator!=(const Card& o) const { return !(*this == o); }
};

//conversioni label <-> carta
Card cardFromLabel(int label);          //label 1..40; qualsiasi altro valore -> carta non valida
int  labelFromCard(const Card& card);   //carta non valida -> 0

//nome leggibile, per console e file .txt
std::string suitName(Suit suit);                  //"Spade", "Bastoni", "Coppe", "Denari"
std::string cardName(const Card& card);           //es. "7 di Coppe"
std::string playerName(Player player);
Player      otherPlayer(Player player);

/*
Nome del seme come lo scrive il ground truth (gameXresults.csv): "spades",
"clubs", "cups", "coins". Il nostro CSV deve usare la stessa grafia, altrimenti
il confronto con le annotazioni non e' diretto.
Attenzione: sono nomi da mazzo francese applicati a un mazzo trentino, quindi
clubs = Bastoni e coins = Denari.
*/
std::string suitCsvName(Suit suit);

//parsing dal ground truth, case-insensitive
bool suitFromName(const std::string& name, Suit& out);
bool playerFromName(const std::string& name, Player& out);

//punti della carta: 1->11, 3->10, 10->4, 9->3, 8->2, resto 0
int cardPoints(const Card& card);

//forza per il confronto: ordine 1 > 3 > 10 > 9 > 8 > 7 > 6 > 5 > 4 > 2.
//Valore piu' alto = carta piu' forte. Carta non valida -> -1.
int cardStrength(const Card& card);

#endif
