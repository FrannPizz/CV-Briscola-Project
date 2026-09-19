#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef MATCHTHEORY_H_INCLUDED
#define MATCHTHEORY_H_INCLUDED

//one recognized card in one frame
struct CardDetection {
    int         round = 0;
    int         frame = 0;
    int         label = 0;          //1-10 Spades, 11-20 Clubs, 21-30 Cups, 31-40 Coins, 0 = no match
    cv::Point2f center;             //center of the yolo box
};

//result of one round, as required by the csv
struct RoundResult {
    int         round = 0;
    int         northLabel = 0;
    int         southLabel = 0;
    int         briscolaLabel = 0;
    std::string leader;             //"North" or "South"
    std::string winner;             //"North" or "South"
    int         points = 0;
};

//a position on the table where a card stays for some frames
struct PositionCluster {
    cv::Point2f      firstCenter;
    std::vector<int> frames;        //frame of every box of the cluster
    std::vector<int> labels;        //label of the same box
};

//distances in pixels (videos 1080x1920)
struct MatchParams {
    float clusterDistance = 70.0f;  //two boxes closer than this are the same card on the table
    float sameColumn      = 100.0f; //the two played cards are stacked: their x is similar
    float minOffset       = 60.0f;  //the second card is moved up or down of at least this
    float frameMiddleY    = 960.0f; //half of the frame height, used only if no round has two cards
    float briscolaDistance = 150.0f; //a box closer than this to the briscola (rounds 1..nRounds-3) is the briscola under the deck
    int   maxDoubleFrames = 1;      //a briscola candidate seen twice in more frames than this was played (1 = tolerate one wrong box)
};

//game rules
int cardNumber(int label);

int cardSuit(int label);

int cardPoints(int label);

int cardStrength(int label);

std::string roundWinner(int northLabel, int southLabel, const std::string& leader, int briscolaLabel);

//briscola
bool briscolaOnTable(int round, int nRounds);

int countDoubleFrames(const std::vector<CardDetection>& detections, int label, int nRounds, const MatchParams& p);

int findBriscola(const std::vector<CardDetection>& detections, const MatchParams& p);

cv::Point2f findBriscolaPosition(const std::vector<CardDetection>& detections, int briscolaLabel, int nRounds);

bool isBriscolaUnderDeck(const CardDetection& d, cv::Point2f briscolaPosition, int nRounds, const MatchParams& p);

//positions of the cards on the table
std::vector<PositionCluster> buildClusters(const std::vector<CardDetection>& detections, int round, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p);

int countFrames(const PositionCluster& cluster);

int findFirstCluster(const std::vector<PositionCluster>& clusters);

bool clusterHasLabel(const PositionCluster& cluster, int label);

int findSecondCluster(const std::vector<PositionCluster>& clusters, int first, int cardOne, int cardTwo, const MatchParams& p);

float findTableMiddle(const std::vector<CardDetection>& detections, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p);


//whole game: from the detections of all the rounds to one result per round
std::vector<RoundResult> analyzeMatch(const std::vector<CardDetection>& detections, const MatchParams& p = MatchParams());



#endif
