//Author: Facco Filippo
#include "../include/GameRules.h"

Player roundWinner(const PlayedCard& first, const PlayedCard& second, const Card& briscola)
{
    //fallback: se una carta non e' stata riconosciuta non si puo' confrontare
    if (!first.card.valid() && !second.card.valid())
        return first.player;
    if (!second.card.valid())
        return first.player;
    if (!first.card.valid())
        return second.player;

    //briscola sconosciuta: si applicano solo le regole di seme e di forza
    const bool firstIsBriscola  = briscola.valid() && (first.card.suit  == briscola.suit);
    const bool secondIsBriscola = briscola.valid() && (second.card.suit == briscola.suit);

    //1. almeno una briscola: vince la briscola piu' alta
    if (firstIsBriscola || secondIsBriscola) {
        if (firstIsBriscola && !secondIsBriscola)
            return first.player;
        if (secondIsBriscola && !firstIsBriscola)
            return second.player;
        return (cardStrength(first.card) > cardStrength(second.card)) ? first.player
                                                                     : second.player;
    }

    //2. semi diversi e nessuna briscola: la risposta non prende, vince chi ha aperto
    if (first.card.suit != second.card.suit)
        return first.player;

    //3. stesso seme: vince la carta piu' alta
    return (cardStrength(first.card) > cardStrength(second.card)) ? first.player
                                                                 : second.player;
}

int roundPoints(const Card& a, const Card& b)
{
    return cardPoints(a) + cardPoints(b);
}
