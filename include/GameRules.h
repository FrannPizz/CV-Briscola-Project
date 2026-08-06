//Author: Facco Filippo
#ifndef GAMERULES_H_INCLUDED
#define GAMERULES_H_INCLUDED

#include "Card.h"

/*
Regole della Briscola necessarie per assegnare i punti (sezione 3 della consegna).
Modulo puro: nessuna dipendenza da OpenCV, testabile senza video.
*/

//una carta insieme a chi l'ha giocata
struct PlayedCard {
    Card   card;
    Player player = Player::North;
};

/*
Vincitore della mano, date le due carte in ORDINE DI GIOCO.
  - se almeno una e' di briscola, vince la briscola piu' alta;
  - se i semi sono diversi (e nessuna e' briscola), vince chi ha aperto;
  - se i semi sono uguali, vince la carta piu' alta.

briscola: se non e' valida (non riconosciuta) la regola della briscola viene
saltata del tutto. Prende la Card e non il Suit apposta: un Suit non ha modo di
dire "sconosciuto" e varrebbe Spade per default, facendo vincere ogni carta di
spade per sbaglio.

Se una delle due carte non e' valida vince l'altra; se nessuna delle due lo e',
ritorna il giocatore che ha aperto.
*/
Player roundWinner(const PlayedCard& first, const PlayedCard& second, const Card& briscola);

//punti della mano: somma dei punti delle due carte giocate
int roundPoints(const Card& a, const Card& b);

#endif
