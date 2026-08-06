//Author: Francesco Pizzato

#include "../include/CardRecognizer.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <stdexcept>

/*
this module recognize the detected card and associate it with the template with the highest number of good matches
*/

//compute ORB keypoints and descriptors for a given grayscale image
void computeORB(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors)
{
    //create an ORB detector and compute keypoints and descriptors
    cv::Ptr<cv::ORB> orbPtr = cv::ORB::create();
    cv::ORB& orb = *orbPtr;
    orb.detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
}

CardTemplate loadTemplate(const std::string& imagePath, const int& label)
{
    //load the template image in grayscale
    cv::Mat templateImage = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
    if (templateImage.empty()) {
        throw std::runtime_error("Could not load template image: " + imagePath);
    }

    //compute ORB keypoints and descriptors for the template image
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    computeORB(templateImage, keypoints, descriptors);

    //return a CardTemplate struct containing the label, keypoints, and descriptors
    return CardTemplate{label, keypoints, descriptors};
}

std::vector<CardTemplate> loadTemplates()
{
    std::vector<CardTemplate> templates;

    //iterate through the expected template images (1 to 40)
    for (int i = 1; i <= 40; ++i) {
        std::string imagePath = "../data/template/" + std::to_string(i) + ".jpg";
        try {
            CardTemplate cardTemplate = loadTemplate(imagePath, i);
            templates.push_back(cardTemplate);
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    return templates;
}

int countGoodMatches(const cv::Mat& descA, const cv::Mat& descB)
{
    
    //use a bruteforce matcher with Hamming distance to find matches between descriptors
    cv::BFMatcher matcher(cv::NORM_HAMMING);

    //perform k nearest neighbors matching (k=2) to find the two best matches for each descriptor
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(descA, descB, knn, 2);

    //apply lowe's ratio test to filter out bad matches and count the number of good matches
    int goodMatchesCount = 0;
    for (int i = 0; i < knn.size(); ++i) {

        const std::vector<cv::DMatch>& m = knn[i];
        //check if the descriptor is similar enough to the best match compared to the second best match
        if (m.size() == 2 && m[0].distance < 0.75f * m[1].distance) {
            goodMatchesCount++;
        }
    }
    return goodMatchesCount;
}

//same matching as countGoodMatches, then a geometric check: only the matches that fit
//a single homography (RANSAC inliers) are counted. Tells apart cards whose pips are
//the same drawing repeated a different number of times, which the ratio test cannot do
int countInliers(const std::vector<cv::KeyPoint>& keypointsA, const cv::Mat& descA,
                 const std::vector<cv::KeyPoint>& keypointsB, const cv::Mat& descB)
{
    if (descA.empty() || descB.empty())
        return 0;

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(descA, descB, knn, 2);

    //same Lowe ratio test as before, but here the matches are kept, not just counted
    std::vector<cv::Point2f> ptsA;
    std::vector<cv::Point2f> ptsB;
    for (int i = 0; i < knn.size(); ++i) {
        const std::vector<cv::DMatch>& m = knn[i];
        if (m.size() == 2 && m[0].distance < 0.75f * m[1].distance) {
            ptsA.push_back(keypointsA[m[0].queryIdx].pt);
            ptsB.push_back(keypointsB[m[0].trainIdx].pt);
        }
    }

    //an homography needs at least 4 point pairs
    if (ptsA.size() < 4)
        return 0;

    //3.0 px of reprojection error: the card is rectified, so the residual distortion is small
    cv::Mat inlierMask;
    cv::Mat homography = cv::findHomography(ptsA, ptsB, cv::RANSAC, 3.0, inlierMask);
    if (homography.empty() || inlierMask.empty())
        return 0;

    return cv::countNonZero(inlierMask);
}

int recognize(const cv::Mat& card, const std::vector<CardTemplate>& templates)
{
    //compute ORB keypoints and descriptors for the input card image
    std::vector<cv::KeyPoint> cardKeypoints;
    cv::Mat cardDescriptors;
    computeORB(card, cardKeypoints, cardDescriptors);

    //inliers are far fewer than raw matches, so this threshold is lower than the old one
    const int MIN_INLIERS = 8;

    //initialize variables to keep track of the best matching template
    int bestLabel = 0;
    int maxInliers = 0;

    //iterate through the loaded templates and find the one with the most inliers
    for (int i = 0; i < templates.size(); ++i) {
        //get the current template
        const CardTemplate& t = templates[i];

        int inlierCount = countInliers(cardKeypoints, cardDescriptors, t.keypoints, t.descriptors);

        if (inlierCount > maxInliers) {
            maxInliers = inlierCount;
            bestLabel = t.label;
        }
    }

    //if the best match is bad, no match
    if (maxInliers < MIN_INLIERS)
        return 0;

    return bestLabel;
}