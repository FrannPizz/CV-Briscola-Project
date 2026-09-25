//Author: Filippo Facco
#include "../include/FixedMatch.h"

/*
this module chooses the two cards of every round: every frame votes the cards,
then the hungarian algorithm gives a different card to every slot 
This is a way to fix the problem of the card that is never recognized and for improve the final result of the game.
*/

// For every round we collect votes from every frame:
// - frame with 2 or more cards: the highest card (smallest y) gets 2 votes as North card,
//   the lowest card gets 2 votes as South card
// - frame with only 1 card: 1 vote for both North and South
// The briscola is removed from the cards of the frame, except in the last 3 rounds.
// This is utilized in the hungarian algorithm.
void voteFrame(const std::vector<int>& labels, const std::vector<float>& ys, std::vector<std::vector<int>>& scores, int round)
{
    if (labels.empty())
        return;

    int northSlot = 2 * (round - 1);
    int southSlot = northSlot + 1;

    if (labels.size() == 1) {
        scores[northSlot][labels[0] - 1] += 1;
        scores[southSlot][labels[0] - 1] += 1;
        return;
    }

    //find the highest and the lowest card of the frame
    int up = 0;
    int down = 0;
    for (int k = 1; k < labels.size(); ++k) {
        if (ys[k] < ys[up])
            up = k;
        if (ys[k] >= ys[down])
            down = k;
    }
    scores[northSlot][labels[up] - 1] += 2;
    scores[southSlot][labels[down] - 1] += 2;
}

/*
scores[slot][card]: how much the card fits the slot.
the slots are 2 per round: slot 2*(round-1) = North card, slot 2*(round-1)+1 = South card
*/
std::vector<std::vector<int>> buildScores(const std::vector<CardDetection>& detections, int nRounds, cv::Point2f briscolaPosition, const MatchParams& p)
{
    std::vector<std::vector<int>> scores(2 * nRounds, std::vector<int>(40, 0));

    for (int round = 1; round <= nRounds; ++round) {

        //cards of the current frame (the json is ordered by round and frame)
        std::vector<int> frameLabels;
        std::vector<float> frameYs;
        int currentFrame = -1;

        for (int i = 0; i < detections.size(); ++i) {
            const CardDetection& d = detections[i];

            //only recognized played cards of this round
            if (d.round != round || d.label == 0)
                continue;

            //the briscola under the deck is not a played card: remove it from the cards of the frame
            if (isBriscolaUnderDeck(d, briscolaPosition, nRounds, p))
                continue;

            //new frame: vote the previous one and start again
            if (d.frame != currentFrame) {
                voteFrame(frameLabels, frameYs, scores, round);
                frameLabels.clear();
                frameYs.clear();
                currentFrame = d.frame;
            }

            //same label twice in a frame: keep the highest box
            int position = -1;
            for (int k = 0; k < frameLabels.size(); ++k) {
                if (frameLabels[k] == d.label)
                    position = k;
            }
            if (position == -1) {
                frameLabels.push_back(d.label);
                frameYs.push_back(d.center.y);
            } else if (d.center.y < frameYs[position]) {
                frameYs[position] = d.center.y;
            }
        }
        //vote the last frame of the round
        voteFrame(frameLabels, frameYs, scores, round);
    }
    return scores;
}

/*
hungarian algorithm: gives to every row (a slot) a different column (a card)
so that the sum of the scores is maximum.
It works on costs to minimize, so every score becomes (maxScore - score).
Returns, for every row, the chosen column (-1 if the column is only a padding one).
*/
std::vector<int> hungarianMax(const std::vector<std::vector<int>>& scores)
{
    int rows = scores.size();
    int cols = scores[0].size();

    //the algorithm needs a square matrix: the missing rows or columns get score 0
    int n = std::max(rows, cols);
    int maxScore = 0;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            maxScore = std::max(maxScore, scores[i][j]);
        }
    }

    std::vector<std::vector<int>> cost(n, std::vector<int>(n, maxScore));
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            cost[i][j] = maxScore - scores[i][j];
        }
    }

    //indices start from 1, index 0 is a helper
    const int INF = 1000000000;
    std::vector<int> u(n + 1, 0);        //potential of the rows
    std::vector<int> v(n + 1, 0);        //potential of the columns
    std::vector<int> p(n + 1, 0);        //p[j] = row assigned to column j
    std::vector<int> way(n + 1, 0);      //previous column on the path

    //add the rows one at a time
    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<int> minv(n + 1, INF);
        std::vector<bool> used(n + 1, false);

        //search the cheapest path from row i to a free column
        while (true) {
            used[j0] = true;
            int i0 = p[j0];
            int delta = INF;
            int j1 = 0;

            for (int j = 1; j <= n; ++j) {
                if (used[j])
                    continue;
                int current = cost[i0 - 1][j - 1] - u[i0] - v[j];
                if (current < minv[j]) {
                    minv[j] = current;
                    way[j] = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = j;
                }
            }

            //update the potentials
            for (int j = 0; j <= n; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }

            j0 = j1;
            //free column found
            if (p[j0] == 0)
                break;
        }

        //go back along the path and move the assignments
        while (j0 != 0) {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        }
    }

    //from "row of every column" to "column of every row"
    std::vector<int> rowToCol(rows, -1);
    for (int j = 1; j <= n; ++j) {
        int row = p[j] - 1;
        if (row >= 0 && row < rows && j - 1 < cols)
            rowToCol[row] = j - 1;
    }
    return rowToCol;
}
