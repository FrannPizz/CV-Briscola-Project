#include "MatchTheory.h"
#include <vector>

#ifndef FIXEDMATCH_H_INCLUDED
#define FIXEDMATCH_H_INCLUDED

//votes of one frame to the North and South slot of its round
void voteFrame(const std::vector<int>& labels, const std::vector<float>& ys, std::vector<std::vector<int>>& scores, int round);

//scores[slot][card] for the whole game, slot 2*(round-1) = North, 2*(round-1)+1 = South
std::vector<std::vector<int>> buildScores(const std::vector<CardDetection>& detections, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p);

//hungarian algorithm: a different card to every slot with the maximum sum of scores
std::vector<int> hungarianMax(const std::vector<std::vector<int>>& scores);

#endif
