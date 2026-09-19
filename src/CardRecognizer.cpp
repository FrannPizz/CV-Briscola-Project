//Author: Filippo Facco
#include "../include/CardRecognizer.h"

/*
this module recognize the detected card and associate it with the template with the highest number of good matches
*/

//compute SIFT keypoints and descriptors for a given grayscale image
void computeSIFT(const cv::Mat& gray, std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors)
{
    //create a SIFT detector and compute keypoints and descriptors
    //(SIFT is scale and rotation invariant: portrait cards match the landscape templates)

    cv::Ptr<cv::SIFT> siftPtr = cv::SIFT::create(); 
    cv::SIFT& sift = *siftPtr;
    sift.detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
}

CardTemplate loadTemplate(const std::string& imagePath, const int& label)
{
    //load the template image in grayscale
    cv::Mat templateImage = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
    if (templateImage.empty()) {
        throw std::runtime_error("Could not load template image: " + imagePath);
    }

    //compute SIFT keypoints and descriptors for the template image
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    computeSIFT(templateImage, keypoints, descriptors);

    //return a CardTemplate struct containing the label, keypoints, and descriptors
    return CardTemplate{label, keypoints, descriptors};
}

std::vector<CardTemplate> loadTemplates()
{
    std::vector<CardTemplate> templates;

    //iterate through the template images (0 to 40)
    //0.jpg is the card back with label 0 (= no match): yolo also finds the deck and the piles of won cards,
    //and without it SIFT gives them the label of a card. It is the first one, so a card must have MORE matches to win
    for (int i = 0; i <= 40; ++i) {
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
    
    //SIFT descriptors are float vectors: need at least 2 per side for knn with k=2
    if (descA.rows < 2 || descB.rows < 2)
        return 0;

    //use a bruteforce matcher with L2 distance to find matches between descriptors
    cv::BFMatcher matcher(cv::NORM_L2);

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
    //compute SIFT keypoints and descriptors for the input card image
    std::vector<cv::KeyPoint> cardKeypoints;
    cv::Mat cardDescriptors;
    computeSIFT(card, cardKeypoints, cardDescriptors);

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
    //(if the best template is the card back, bestLabel is already 0 = no match)
    if (maxGoodMatches < MIN_GOOD_MATCHES)
        return 0;

    return bestLabel;
}