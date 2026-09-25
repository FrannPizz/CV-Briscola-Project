#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

#ifndef CARDRECOGNIZER_H_INCLUDED
#define CARDRECOGNIZER_H_INCLUDED


struct CardTemplate {
    int                       label;        //1-10 Spades, 11-20 Clubs, 21-30 Cups, 31-40 Coins, 0 = card back
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat                   descriptors;
};

void computeSIFT(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors);

std::vector<CardTemplate> loadTemplates();

int countGoodMatches(const cv::Mat& descA, const cv::Mat& descB);

int recognize(const cv::Mat& card, const std::vector<CardTemplate>& templates);

#endif 
