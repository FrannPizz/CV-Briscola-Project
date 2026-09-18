//Author: Francesco Pizzato
#include "../include/MatchTheory.h"
#include "../include/FixedMatch.h"

/*
this module rebuilds the whole game from the recognized cards: briscola, the two cards of every round,
who played them, leader, winner and points
*/

//given a label 1..40, return the number of the card 1..10
int cardNumber(int label)
{
    return (label - 1) % 10 + 1;
}

//given a label 1..40, return the suit of the card 0..3 (0 Spades, 1 Clubs, 2 Cups, 3 Coins)
int cardSuit(int label)
{
    return (label - 1) / 10;
}

//points of the card: 1->11, 3->10, 10->4, 9->3, 8->2, others 0
int cardPoints(int label)
{
    if (label < 1 || label > 40)
        return 0;

    int number = cardNumber(label);
    if (number == 1)  return 11;
    if (number == 3)  return 10;
    if (number == 10) return 4;
    if (number == 9)  return 3;
    if (number == 8)  return 2;
    return 0;
}

//set the strength of the card, the order in the array is the strenght 
int cardStrength(int label)
{
    //from the weakest to the strongest: the position in the array is the strength
    int order[10] = { 2, 4, 5, 6, 7, 8, 9, 10, 3, 1 };

    int number = cardNumber(label);

    for (int i = 0; i < 10; ++i) {
        if (order[i] == number)
            return i;
    }
    return -1;
}

//find the winner of the round: returns "North" or "South"
std::string roundWinner(int northLabel, int southLabel, const std::string& leader, int briscolaLabel)
{
    bool northIsBriscola = cardSuit(northLabel) == cardSuit(briscolaLabel);
    bool southIsBriscola = cardSuit(southLabel) == cardSuit(briscolaLabel);

    //only one briscola: the briscola wins
    if (northIsBriscola && !southIsBriscola)
        return "North";
    if (southIsBriscola && !northIsBriscola)
        return "South";

    //different suits (and no briscola): the answer does not take, the leader wins
    if (cardSuit(northLabel) != cardSuit(southLabel))
        return leader;

    //same suit: the strongest card wins
    if (cardStrength(northLabel) > cardStrength(southLabel))
        return "North";
    return "South";
}

//true if the briscola is still under the deck in this round, so a card with its label is not a played card.
bool briscolaOnTable(int round, int nRounds)
{
    return round <= nRounds - 3;
}

/*
number of frames (in the rounds where the briscola is under the deck) where the label is seen twice in two
different places. The briscola is under the deck until round nRounds-3, so it cannot be also on the table:
if a label is seen twice, that card was played and it is not the briscola
*/
int countDoubleFrames(const std::vector<CardDetection>& detections, int label, int nRounds, const MatchParams& p)
{
    int count = 0;
    int lastRound = -1;
    int lastFrame = -1;

    for (int i = 0; i < detections.size(); ++i) {
        const CardDetection& a = detections[i];
        if (a.label != label || !briscolaOnTable(a.round, nRounds))
            continue;

        //the same frame is counted only once
        if (a.round == lastRound && a.frame == lastFrame)
            continue;

        //another box of the same frame with the same label, far from this one
        //(the boxes of the same frame are consecutive in the json)
        for (int j = i + 1; j < detections.size(); ++j) {
            const CardDetection& b = detections[j];
            if (b.round != a.round || b.frame != a.frame)
                break;
            if (b.label == label && cv::norm(a.center - b.center) > p.clusterDistance) {
                count++;
                lastRound = a.round;
                lastFrame = a.frame;
                break;
            }
        }
    }
    return count;
}

/*
the briscola is the same for the whole game and stays on the table in almost every round,
a played card appears only in its round: the briscola is the card seen in more rounds.
If that card is seen twice in the same frame (under the deck and played) it is not the briscola:
the covered briscola is sometimes read as another card of the same suit (e.g. 6 of Coins read as 10 of Coins),
so the next card with the most rounds is tried
*/
int findBriscola(const std::vector<CardDetection>& detections, const MatchParams& p)
{
    //number of rounds of the game
    int nRounds = 0;
    for (int i = 0; i < detections.size(); ++i)
        nRounds = std::max(nRounds, detections[i].round);

    //seen[label][round] = true if the label is recognized at least once in that round
    std::vector<std::vector<bool>> seen(41, std::vector<bool>(nRounds + 1, false)); //41 because the labels are 1..40, index 0 is unused
    for (int i = 0; i < detections.size(); ++i) {
        int label = detections[i].label;
        if (label >= 1 && label <= 40)
            seen[label][detections[i].round] = true;
    }

    //count the rounds of every label
    std::vector<int> rounds(41, 0);
    for (int label = 1; label <= 40; ++label) {
        for (int round = 1; round <= nRounds; ++round) {
            if (seen[label][round])
                rounds[label]++;
        }
    }

    //try the labels from the one seen in more rounds: the first one that is not played is the briscola
    std::vector<bool> rejected(41, false);
    int firstChoice = 0;
    for (int attempt = 0; attempt < 40; ++attempt) {

        //label with the most rounds among the ones not rejected yet
        int best = 0;
        for (int label = 1; label <= 40; ++label) {
            if (!rejected[label] && (best == 0 || rounds[label] > rounds[best]))
                best = label;
        }
        if (best == 0 || rounds[best] == 0)
            break;
        if (firstChoice == 0)
            firstChoice = best;

        //seen twice in the same frame in more than maxDoubleFrames frames: it was played, it is not the briscola
        if (countDoubleFrames(detections, best, nRounds, p) <= p.maxDoubleFrames)
            return best;

        rejected[best] = true;
    }

    //every candidate was rejected: keep the card seen in more rounds
    return firstChoice;
}

/*
position of the briscola under the deck: median center of the boxes with the briscola label in the rounds
where it is under the deck (in the last 3 rounds it can be in a hand or played, so they are not used).
Returns a point far outside the frame if the briscola is never seen
*/
cv::Point2f findBriscolaPosition(const std::vector<CardDetection>& detections, int briscolaLabel, int nRounds)
{
    std::vector<float> xs;
    std::vector<float> ys;
    for (int i = 0; i < detections.size(); ++i) {
        const CardDetection& d = detections[i];
        if (d.label == briscolaLabel && briscolaOnTable(d.round, nRounds)) {
            xs.push_back(d.center.x);
            ys.push_back(d.center.y);
        }
    }
    if (xs.empty())
        return cv::Point2f(-10000.0f, -10000.0f);

    //median: the middle value after sorting
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    int middle = xs.size() / 2;
    return cv::Point2f(xs[middle], ys[middle]);
}

//true if the box is the briscola under the deck (not a played card): rounds 1..17 and close to the briscola position
//(whatever its label: the covered briscola is often read as another card, e.g. 6 of Coins read as 10 of Coins)
bool isBriscolaUnderDeck(const CardDetection& d, cv::Point2f briscolaPosition, int nRounds, const MatchParams& p)
{
    return briscolaOnTable(d.round, nRounds) && cv::norm(d.center - briscolaPosition) < p.briscolaDistance;
}

//group the boxes of a round by position, without looking at the labels
//(a covered card often gets the label of the card on top of it, its position does not change)
std::vector<PositionCluster> buildClusters(const std::vector<CardDetection>& detections, int round, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p)
{
    std::vector<PositionCluster> clusters;

    for (int i = 0; i < detections.size(); ++i) {
        const CardDetection& d = detections[i];
        if (d.round != round)
            continue;

        //cards not recognized (???) are not used for the game
        if (d.label == 0)
            continue;

        //the briscola under the deck is not a played card (in the last 3 rounds it can be played)
        if (isBriscolaUnderDeck(d, briscolaPosition, nRounds, p))
            continue;

        //find the nearest cluster
        int best = -1;
        float bestDistance = p.clusterDistance;
        for (int k = 0; k < clusters.size(); ++k) {
            float distance = cv::norm(clusters[k].firstCenter - d.center);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = k;
            }
        }

        //no cluster near: new card position
        if (best == -1) {
            PositionCluster cluster;
            cluster.firstCenter = d.center;
            clusters.push_back(cluster);
            best = clusters.size() - 1;
        }
        clusters[best].frames.push_back(d.frame);
        clusters[best].labels.push_back(d.label);
    }
    return clusters;
}


//number of different frames where the card of the cluster is seen (not the frames of the round).
int countFrames(const PositionCluster& cluster)
{
    int count = 0;
    for (int i = 0; i < cluster.frames.size(); ++i) {
        //the boxes are in increasing frame order (as in the json): a new frame starts when the value changes
        if (i == 0 || cluster.frames[i] != cluster.frames[i - 1])
            count++;
    }
    return count;
}

//first card played in the round: the earliest cluster seen in at least 3 frames (-1 if none)
int findFirstCluster(const std::vector<PositionCluster>& clusters)
{
    int first = -1;
    for (int k = 0; k < clusters.size(); ++k) {
        if (countFrames(clusters[k]) < 3)
            continue;
        if (first == -1 || clusters[k].frames[0] < clusters[first].frames[0])
            first = k;
    }
    return first;
}

//true if the cluster contains at least once the label
bool clusterHasLabel(const PositionCluster& cluster, int label)
{
    for (int i = 0; i < cluster.labels.size(); ++i) {
        if (cluster.labels[i] == label)
            return true;
    }
    return false;
}

//second card played: a later cluster in the same column, moved up or down (-1 if none).
//cardOne and cardTwo are the two cards of the round (0 if not known): a cluster that contains one of them
//is preferred, so a wrong box is not taken
int findSecondCluster(const std::vector<PositionCluster>& clusters, int first, int cardOne, int cardTwo, const MatchParams& p)
{
    int second = -1;
    bool secondHasCard = false;
    for (int k = 0; k < clusters.size(); ++k) {
        if (k == first || countFrames(clusters[k]) < 2)
            continue;

        //same column and moved up or down
        float dx = std::abs(clusters[k].firstCenter.x - clusters[first].firstCenter.x);
        float dy = std::abs(clusters[k].firstCenter.y - clusters[first].firstCenter.y);
        if (dx >= p.sameColumn || dy <= p.minOffset)
            continue;

        //appears after the first card
        if (clusters[k].frames[0] <= clusters[first].frames[0])
            continue;

        //does this candidate contain one of the two cards of the round?
        bool hasCard = clusterHasLabel(clusters[k], cardOne) || clusterHasLabel(clusters[k], cardTwo);

        //more than one candidate: first the one with a card of the round, then the one seen in more frames
        bool better = false;
        if (second == -1)
            better = true;
        else if (hasCard && !secondHasCard)
            better = true;
        else if (hasCard == secondHasCard && countFrames(clusters[k]) > countFrames(clusters[second]))
            better = true;

        if (better) {
            second = k;
            secondHasCard = hasCard;
        }
    }
    return second;
}

/*
middle of the table between North and South, used when a round has only one card detetcted.
In every round with two cards the middle point of their y is the border between the players 
useful for when we have only 1 card detected, we can use the middle of the table to determine if it is North or South
*/
float findTableMiddle(const std::vector<CardDetection>& detections, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p)
{
    float sum = 0.0f;
    int count = 0;

    for (int round = 1; round <= nRounds; ++round) {
        std::vector<PositionCluster> clusters = buildClusters(detections, round, nRounds, briscolaPosition, p);
        int first = findFirstCluster(clusters);
        if (first == -1)
            continue;
        //the cards of the round are not known yet here: no label preference
        int second = findSecondCluster(clusters, first, 0, 0, p);
        if (second == -1)
            continue;

        //middle point between the two cards of the round
        sum += (clusters[first].firstCenter.y + clusters[second].firstCenter.y) / 2.0f;
        count++;
    }

    if (count == 0)
        return p.frameMiddleY;
    return sum / count;
}

//logic behind the whole game: briscola, the two cards of every round (votes + hungarian) and who played them.
//the card seen first is the leader card; with two cards on the table the higher one is North,
//with one card it goes to the player on its half of the table. Then winner and points of every round
std::vector<RoundResult> analyzeMatch(const std::vector<CardDetection>& detections, const MatchParams& p)
{
    std::vector<RoundResult> results;
    if (detections.empty())
        return results;

    //number of rounds of the game
    int nRounds = 0;
    for (int i = 0; i < detections.size(); ++i)
        nRounds = std::max(nRounds, detections[i].round);

    //1. briscola: the card seen in more rounds
    int briscola = findBriscola(detections, p);

    //position of the briscola under the deck: the boxes there are not played cards
    cv::Point2f briscolaPosition = findBriscolaPosition(detections, briscola, nRounds);

    //2. votes of every card for every slot
    std::vector<std::vector<int>> scores = buildScores(detections, nRounds, briscolaPosition, p);

    //3. every card is played once: the hungarian algorithm gives a different card to every slot
    //   (a card never recognized stays free and goes to the slot without votes)
    std::vector<int> slotCard = hungarianMax(scores);

    //4. middle of the table, for the rounds with only one card
    float tableMiddle = findTableMiddle(detections, nRounds, briscolaPosition, p);

    //5. for every round: which card was played first and by who
    for (int round = 1; round <= nRounds; ++round) {
        int cardOne = slotCard[2 * (round - 1)] + 1;
        int cardTwo = slotCard[2 * (round - 1) + 1] + 1;

        //first frame where the two cards are recognized
        int frameOne = -1;
        int frameTwo = -1;
        for (int i = 0; i < detections.size(); ++i) {
            const CardDetection& d = detections[i];
            if (d.round != round)
                continue;
            if (isBriscolaUnderDeck(d, briscolaPosition, nRounds, p))
                continue;
            if (d.label == cardOne && frameOne == -1)
                frameOne = d.frame;
            if (d.label == cardTwo && frameTwo == -1)
                frameTwo = d.frame;
        }
        bool timingKnown = frameOne != -1 && frameTwo != -1 && frameOne != frameTwo;

        //positions of the first and of the second card on the table
        std::vector<PositionCluster> clusters = buildClusters(detections, round, nRounds, briscolaPosition, p);
        int first = findFirstCluster(clusters);
        int second = -1;
        if (first != -1)
            second = findSecondCluster(clusters, first, cardOne, cardTwo, p);

        //cardA = played first, cardB = played second
        int cardA = cardOne;
        int cardB = cardTwo;
        if (timingKnown) {
            if (frameTwo < frameOne) {
                cardA = cardTwo;
                cardB = cardOne;
            }
        } else if (second != -1) {
            //one card is never recognized: the card read in the second position is cardB
            int votesOne = 0;
            int votesTwo = 0;
            for (int i = 0; i < clusters[second].labels.size(); ++i) {
                if (clusters[second].labels[i] == cardOne)
                    votesOne++;
                if (clusters[second].labels[i] == cardTwo)
                    votesTwo++;
            }
            if (votesOne >= votesTwo) {
                cardA = cardTwo;
                cardB = cardOne;
            }
        }

        //who played cardA: North is up (smaller y), South is down
        bool firstIsNorth = true;
        if (first != -1 && second != -1) {
            //two cards on the table: the higher one is the North card
            firstIsNorth = clusters[first].firstCenter.y < clusters[second].firstCenter.y;
        } else if (first != -1) {
            //only one card on the table: it goes to the player on its half of the table
            firstIsNorth = clusters[first].firstCenter.y < tableMiddle;
        }
        //no card position: cardA stays North

        //6. result of the round
        RoundResult result;
        result.round = round;
        result.briscolaLabel = briscola;
        result.points = cardPoints(cardA) + cardPoints(cardB);

        if (firstIsNorth) {
            result.leader = "North";
            result.northLabel = cardA;
            result.southLabel = cardB;
        } else {
            result.leader = "South";
            result.northLabel = cardB;
            result.southLabel = cardA;
        }

        result.winner = roundWinner(result.northLabel, result.southLabel, result.leader, briscola);

        results.push_back(result);
    }

    return results;
}
