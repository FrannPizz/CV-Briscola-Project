#include <opencv2/imgproc.hpp>
#include <vector>
#include <opencv2/core.hpp>

#ifndef CARDDETECTOR_H_INCLUDED
#define CARDDETECTOR_H_INCLUDED

enum class Half { North, South };

struct DetectedCard {
    std::vector<cv::Point> corners;
    cv::Point2f            centroid;
    Half                   half;
};

struct Params {
    double minAreaRatio   = 0.005;
    double maxAreaRatio   = 0.20;
    double approxEpsRatio = 0.02;
    double minAspect      = 0.45;
    double maxAspect      = 0.68;
    double minExtent      = 0.50;
    int    medianKernel   = 5;
    int    closeKernel    = 5;
    double cannySigma     = 0.33;
    //due rilevamenti piu' vicini di questa frazione della larghezza del frame sono
    //la stessa carta (contorni annidati restituiti da RETR_LIST)
    double dupRadiusRatio = 0.05;
};

std::vector<DetectedCard> detect(const cv::Mat& frame, const Params& p = Params());

cv::Mat preprocess(const cv::Mat& frame);

cv::Mat edgeMask(const cv::Mat& gray, const Params& p);

std::vector<std::vector<cv::Point>> findCandidates(const cv::Mat& mask);

bool isCardQuad(const std::vector<cv::Point>& contour,   cv::Size frameSize, const Params& p, std::vector<cv::Point>& quad);

DetectedCard buildCard(const std::vector<cv::Point>& quad, cv::Size frameSize);

std::vector<DetectedCard> dedupCards(const std::vector<DetectedCard>& cards, cv::Size frameSize, const Params& p);

#endif 
