//Author: Francesco Pizzato

#include "../include/CardRecognizer.h"
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

int recognize(const cv::Mat& card, const std::vector<CardTemplate>& templates)
{
    //compute ORB keypoints and descriptors for the input card image
    std::vector<cv::KeyPoint> cardKeypoints;
    cv::Mat cardDescriptors;
    computeORB(card, cardKeypoints, cardDescriptors);

    //low threshold
    const int MIN_GOOD_MATCHES = 15;

    //initialize variables to keep track of the best matching template
    int bestLabel = 0;
    int maxGoodMatches = 0;

    //iterate through the loaded templates and find the one with the highest number of good matches
    for (int i = 0; i < templates.size(); ++i) {
        //get the current template
        const CardTemplate& t = templates[i];

        int goodMatchesCount = countGoodMatches(cardDescriptors, t.descriptors);

        if (goodMatchesCount > maxGoodMatches) {
            maxGoodMatches = goodMatchesCount;
            bestLabel = t.label;
        }
    }

    //if the best match is bad, no match
    if (maxGoodMatches < MIN_GOOD_MATCHES)
        return 0;

    return bestLabel;
}