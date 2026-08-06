//Author: Facco Filippo
#ifndef ROUNDANALYZER_H_INCLUDED
#define ROUNDANALYZER_H_INCLUDED

#include <map>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "Card.h"
#include "CardDetector.h"
#include "CardRecognizer.h"

/*
Analisi TEMPORALE di un round: da un video a un risultato strutturato.

Il detector lavora su un singolo frame, ma la consegna chiede informazioni che
esistono solo nel tempo (chi ha giocato per primo). L'idea qui e':

  1. si scorrono i frame del video (uno ogni frameStep) e si fa
     detect -> rectify -> recognize su ognuno;
  2. le detection vicine fra loro nello spazio vengono raggruppate in "track":
     una carta appoggiata sul tavolo resta ferma, quindi produce una track lunga;
  3. ogni track accumula un ISTOGRAMMA di label: l'etichetta finale e' quella a
     maggioranza. Questo assorbe gli errori sporadici del recognizer, che sul
     singolo frame e' rumoroso;
  4. le track vengono classificate usando quando compaiono e quanto durano:
       - le due carte GIOCATE compaiono durante il round, una per meta';
       - mazzo e mazzetti raccolti sono a faccia in giu': il recognizer ha solo i
         40 fronti, quindi su di loro restituisce 0 e si escludono da soli;
  5. chi ha aperto (leader) e' la carta giocata che compare PRIMA nel tempo.

La BRISCOLA va trovata a parte: sul tavolo sta di traverso e mezza sotto il mazzo,
quindi il suo contorno non si chiude mai e il detector geometrico non la vede.
Si sfrutta invece una proprieta' del gioco: nei PRIMI frame di un round l'unica
carta scoperta sul tavolo e' proprio la briscola (tutto il resto e' a faccia in
giu'). Basta quindi fare ORB sull'intero frame iniziale e cercare il template che
matcha meglio -- vedi recognizeBriscola().
*/

//un gruppo di detection alla stessa posizione, viste su piu' frame
struct CardTrack {
    cv::Point2f centroid;                //media delle posizioni viste
    Half        half = Half::North;      //meta' del frame in cui sta (North/South)
    int         firstFrame = 0;          //indice del primo frame in cui compare
    int         lastFrame  = 0;          //indice dell'ultimo frame in cui compare
    int         sightings  = 0;          //in quanti frame campionati e' stata vista
    int         namedSightings = 0;      //quante volte il recognizer ha dato label != 0

    std::map<int, int> labelVotes;       //label -> quante volte e' stata votata

    int bestLabel() const;               //label a maggioranza (0 se nessuna)
    int bestVotes() const;               //voti della label vincente
    Card card() const { return cardFromLabel(bestLabel()); }
};

struct AnalyzerParams {
    int    frameStep        = 2;      //campiona 1 frame ogni N (2 = meta' dei frame)
    double mergeRadius      = 60.0;   //px: entro questo raggio due detection sono la stessa carta
    int    minSightings     = 3;      //track viste meno volte di cosi' = rumore, scartate
    int    minNamedSightings= 2;      //minimo di frame in cui il recognizer l'ha nominata
    double earlyFraction    = 0.15;   //frazione iniziale del round: li' c'e' gia' la briscola
    bool   verbose          = false;  //stampa le track trovate (utile per tarare i parametri)

    //ricerca della briscola sul frame intero
    int    briscolaFrames     = 3;    //quanti frame iniziali provare (voto di maggioranza)
    int    briscolaFeatures   = 4000; //keypoint ORB: il frame e' grande e pieno di tessuto
    /*
    Inlier minimi perche' la lettura sia credibile. Misurato: quando la briscola e'
    leggibile gli inlier stanno sulle centinaia, quando non lo e' il rumore di fondo
    arriva a 6-7. La soglia sta larga in mezzo, sul lato prudente: una briscola
    sbagliata falsa il vincitore di TUTTI i round, mentre non leggerla lascia
    semplicemente cadere la regola della briscola.
    */
    int    briscolaMinMatches = 15;
    double briscolaMinRatio   = 1.5;  //il migliore deve staccare il secondo di questo fattore
    //semi-lato del ritaglio attorno al mazzo, in frazioni della larghezza del frame:
    //restringere il campo toglie keypoint di sfondo e allarga il margine sul secondo
    double briscolaRoiRatio   = 0.30;

    Params detector;                  //parametri del detector geometrico (CardDetector.h)
};

struct RoundResult {
    int    round    = 0;
    Card   north;                        //carta giocata da North
    Card   south;                        //carta giocata da South
    Card   briscola;
    Player leader   = Player::North;     //chi ha aperto
    Player winner   = Player::North;
    int    points   = 0;

    bool        complete = false;        //entrambe le carte giocate riconosciute
    bool        briscolaFromCarry = false; //briscola ereditata dai round precedenti
    std::string note;                    //diagnostica leggibile (perche' e' incompleto)
};

/*
Scorre il video e costruisce le track. Esposta separatamente perche' e' il punto
in cui conviene guardare quando qualcosa non torna.
*/
std::vector<CardTrack> collectTracks(const std::string& videoPath,
                                     const std::vector<CardTemplate>& templates,
                                     const AnalyzerParams& params = AnalyzerParams());

/*
Track del MAZZO: sta ferma per tutto il round, e' presente fin dall'inizio e non
viene mai riconosciuta (e' a faccia in giu', e i template sono solo i 40 fronti).
Serve per sapere dove guardare per la briscola, che gli sta sempre appiccicata.
Ritorna -1 se non la trova.
*/
int findDeckTrack(const std::vector<CardTrack>& tracks,
                  int sampledFrames,
                  const AnalyzerParams& params);

/*
Briscola letta dai primi frame del round con ORB.
roi: zona in cui cercare. Se vuota si usa il fotogramma intero, ma conviene
passare un ritaglio attorno al mazzo: su sfondi che generano molti keypoint (il
legno di game4) l'intero frame annega la carta nel rumore e il margine sul
secondo classificato non basta piu'.
Ritorna una Card non valida se nessun template stacca abbastanza dagli altri
(succede negli ultimi round, quando la briscola e' stata pescata dal tavolo).
*/
Card recognizeBriscola(const std::string& videoPath,
                       const std::vector<CardTemplate>& templates,
                       const AnalyzerParams& params = AnalyzerParams(),
                       const cv::Rect& roi = cv::Rect());

/*
Indici delle due track giocate (North e South), -1 se non trovate.
briscolaLabel: se una track ha questa label ED e' presente fin dall'inizio del
round, e' la briscola sul tavolo e va esclusa. Se invece compare a meta' round
e' la briscola giocata da un giocatore, e allora conta come carta giocata.

L'assegnazione ai giocatori usa la posizione RELATIVA fra le due carte, non il
campo Half (che divide il fotogramma a meta' esatta). I giocatori giocano verso
il centro del tavolo e l'inquadratura non e' centrata sulla zona di gioco:
misurato su game4, le due carte cadono a y=1017 e y=1412 su un fotogramma alto
1920, quindi con la meta' assoluta finiscono entrambe South e una si perde.
Il campo Half resta come ripiego quando di carta ne viene trovata una sola.
*/
void selectPlayedTracks(const std::vector<CardTrack>& tracks,
                        int briscolaLabel,
                        int sampledFrames,
                        const AnalyzerParams& params,
                        int& northIndex,
                        int& southIndex);

/*
Analisi completa di un round.
carriedBriscola: briscola dei round precedenti, usata quando nel video non e' piu'
visibile (negli ultimi round viene pescata). Passare una Card non valida al primo round.
*/
RoundResult analyzeRound(const std::string& videoPath,
                         int roundNumber,
                         const std::vector<CardTemplate>& templates,
                         const Card& carriedBriscola,
                         const AnalyzerParams& params = AnalyzerParams());

#endif
