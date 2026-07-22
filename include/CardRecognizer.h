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

int recognize(const cv::Mat& card, const std::vector<CardTemplate>& templates);

#endif 
