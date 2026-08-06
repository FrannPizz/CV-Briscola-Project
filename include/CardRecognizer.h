#include <string>
#include <vector>
#include <opencv2/core.hpp>

#ifndef CARDRECOGNIZER_H_INCLUDED
#define CARDRECOGNIZER_H_INCLUDED


struct CardTemplate {
    int                       label;        //1-10 Swords, 11-20 Batons, 21-30 Cups, 31-40 Coins
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat                   descriptors;
};

void computeORB(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors);

std::vector<CardTemplate> loadTemplates();

int countGoodMatches(const cv::Mat& descA, const cv::Mat& descB);

/*
Come countGoodMatches, ma i match superstiti passano anche una verifica GEOMETRICA:
si stima un'omografia con RANSAC fra i due insiemi di keypoint e si contano solo gli
inlier, cioe' i match che stanno in una trasformazione coerente.

Serve perche' le carte numeriche dello stesso seme hanno le figure ripetute identiche
(le rosette dei denari sono lo stesso disegno su 3, 5, 6, 10): i descrittori ORB le
trovano tutte simili e il ratio test da solo non le distingue. La disposizione dei
pips invece cambia, e RANSAC la vede: i match verso il template sbagliato non stanno
in una singola omografia e vengono scartati come outlier.

Ritorna 0 se i match sono meno di 4, il minimo per stimare un'omografia.
*/
int countInliers(const std::vector<cv::KeyPoint>& keypointsA, const cv::Mat& descA,
                 const std::vector<cv::KeyPoint>& keypointsB, const cv::Mat& descB);

int recognize(const cv::Mat& card, const std::vector<CardTemplate>& templates);

#endif 
