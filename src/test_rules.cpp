//Author: <il tuo nome>
//
// Test delle regole di gioco. Non usa OpenCV ne' i video: gira anche senza dataset.
// Serve a essere sicuri che vincitore e punti siano giusti PRIMA di dare la colpa
// alla visione quando le metriche non tornano.
//
// Uso: ./main test

#include <iostream>
#include <string>

#include "../include/Card.h"
#include "../include/GameRules.h"
#include "../include/RulesTests.h"

static int failures = 0;

static void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cout << "  FALLITO: " << what << std::endl;
        ++failures;
    }
}

//scorciatoia per costruire una carta
static Card C(int number, Suit suit)
{
    Card c;
    c.number = number;
    c.suit   = suit;
    return c;
}

static void testLabelConversion()
{
    std::cout << "conversione label <-> carta" << std::endl;

    check(cardFromLabel(1)  == C(1, Suit::Spade),    "1 = Asso di Spade");
    check(cardFromLabel(11) == C(1, Suit::Bastoni),  "11 = Asso di Bastoni");
    check(cardFromLabel(23) == C(3, Suit::Coppe),    "23 = 3 di Coppe");
    check(cardFromLabel(40) == C(10, Suit::Denari),  "40 = Re di Denari");

    check(!cardFromLabel(0).valid(),  "label 0 non valida");
    check(!cardFromLabel(41).valid(), "label 41 non valida");

    //andata e ritorno su tutte e 40
    for (int label = 1; label <= 40; ++label)
        check(labelFromCard(cardFromLabel(label)) == label,
              "round-trip label " + std::to_string(label));
}

static void testPoints()
{
    std::cout << "punti delle carte" << std::endl;

    check(cardPoints(C(1,  Suit::Coppe)) == 11, "Asso vale 11");
    check(cardPoints(C(3,  Suit::Coppe)) == 10, "3 vale 10");
    check(cardPoints(C(10, Suit::Coppe)) == 4,  "Re vale 4");
    check(cardPoints(C(9,  Suit::Coppe)) == 3,  "Cavallo vale 3");
    check(cardPoints(C(8,  Suit::Coppe)) == 2,  "Fante vale 2");

    for (int n : {2, 4, 5, 6, 7})
        check(cardPoints(C(n, Suit::Spade)) == 0,
              "il " + std::to_string(n) + " non vale punti");

    //il mazzo intero deve fare 120 punti: e' il controllo che smaschera gli errori
    int total = 0;
    for (int label = 1; label <= 40; ++label)
        total += cardPoints(cardFromLabel(label));
    check(total == 120, "totale del mazzo = 120 (trovato " + std::to_string(total) + ")");
}

static void testStrength()
{
    std::cout << "forza delle carte" << std::endl;

    //1 > 3 > 10 > 9 > 8 > 7 > 6 > 5 > 4 > 2
    const int order[] = {1, 3, 10, 9, 8, 7, 6, 5, 4, 2};
    for (int i = 0; i + 1 < 10; ++i) {
        check(cardStrength(C(order[i], Suit::Denari)) > cardStrength(C(order[i+1], Suit::Denari)),
              std::to_string(order[i]) + " batte " + std::to_string(order[i+1]));
    }

    //controlli puntuali sui casi controintuitivi
    check(cardStrength(C(3, Suit::Spade)) > cardStrength(C(10, Suit::Spade)), "il 3 batte il Re");
    check(cardStrength(C(2, Suit::Spade)) < cardStrength(C(4, Suit::Spade)),  "il 2 e' la piu' debole");
}

static void testWinner()
{
    std::cout << "vincitore della mano" << std::endl;
    const Card BRISCOLA = C(1, Suit::Denari);   //conta solo il seme

    //stesso seme, nessuna briscola: vince la piu' alta
    check(roundWinner(PlayedCard{C(7, Suit::Coppe), Player::North},
                      PlayedCard{C(1, Suit::Coppe), Player::South},
                      BRISCOLA) == Player::South,
          "stesso seme: l'Asso batte il 7");

    check(roundWinner(PlayedCard{C(3, Suit::Coppe), Player::North},
                      PlayedCard{C(10, Suit::Coppe), Player::South},
                      BRISCOLA) == Player::North,
          "stesso seme: il 3 batte il Re");

    //semi diversi senza briscola: vince chi ha aperto, anche con carta debolissima
    check(roundWinner(PlayedCard{C(2, Suit::Coppe), Player::North},
                      PlayedCard{C(1, Suit::Spade), Player::South},
                      BRISCOLA) == Player::North,
          "seme diverso: chi apre vince anche col 2");

    //briscola giocata in risposta: taglia e vince
    check(roundWinner(PlayedCard{C(1, Suit::Coppe), Player::North},
                      PlayedCard{C(2, Suit::Denari), Player::South},
                      BRISCOLA) == Player::South,
          "il 2 di briscola taglia l'Asso");

    //briscola giocata per prima, risposta di altro seme
    check(roundWinner(PlayedCard{C(4, Suit::Denari), Player::South},
                      PlayedCard{C(1, Suit::Coppe), Player::North},
                      BRISCOLA) == Player::South,
          "briscola in apertura batte l'Asso di altro seme");

    //due briscole: vince la piu' alta
    check(roundWinner(PlayedCard{C(10, Suit::Denari), Player::North},
                      PlayedCard{C(3,  Suit::Denari), Player::South},
                      BRISCOLA) == Player::South,
          "due briscole: il 3 batte il Re");

    //il leader puo' essere South: il ruolo non dipende dalla posizione negli argomenti
    check(roundWinner(PlayedCard{C(5, Suit::Bastoni), Player::South},
                      PlayedCard{C(6, Suit::Coppe),   Player::North},
                      BRISCOLA) == Player::South,
          "leader South con seme diverso vince");
}

static void testPartialRecognition()
{
    std::cout << "carte non riconosciute" << std::endl;
    const Card BRISCOLA = C(1, Suit::Spade);

    Card unknown;   //number = 0

    check(roundWinner(PlayedCard{unknown, Player::North},
                      PlayedCard{C(5, Suit::Coppe), Player::South},
                      BRISCOLA) == Player::South,
          "se la prima non e' riconosciuta vince l'altra");

    check(roundWinner(PlayedCard{C(5, Suit::Coppe), Player::North},
                      PlayedCard{unknown, Player::South},
                      BRISCOLA) == Player::North,
          "se la seconda non e' riconosciuta vince l'altra");

    check(cardPoints(unknown) == 0, "carta sconosciuta non da' punti");
}

//regressione: con la briscola non riconosciuta si usava il seme di default (Spade),
//e ogni carta di spade vinceva la mano per sbaglio
static void testUnknownBriscola()
{
    std::cout << "briscola non riconosciuta" << std::endl;
    const Card NESSUNA;   //briscola sconosciuta

    //semi diversi: deve vincere chi ha aperto, anche se l'altro gioca spade
    check(roundWinner(PlayedCard{C(3, Suit::Denari), Player::North},
                      PlayedCard{C(2, Suit::Spade),  Player::South},
                      NESSUNA) == Player::North,
          "senza briscola nota le spade non tagliano");

    //stesso seme: continua a vincere la carta piu' alta
    check(roundWinner(PlayedCard{C(5, Suit::Coppe), Player::South},
                      PlayedCard{C(1, Suit::Coppe), Player::North},
                      NESSUNA) == Player::North,
          "senza briscola nota lo stesso seme si confronta per forza");
}

int runRulesTests()
{
    failures = 0;

    testLabelConversion();
    testPoints();
    testStrength();
    testWinner();
    testPartialRecognition();
    testUnknownBriscola();

    if (failures == 0) {
        std::cout << "\nTutti i test passati." << std::endl;
        return 0;
    }
    std::cout << "\n" << failures << " test falliti." << std::endl;
    return 1;
}
