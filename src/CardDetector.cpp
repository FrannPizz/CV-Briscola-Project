//Author: Francesco Pizzato
#include "../include/CardDetector.h"
#include <opencv2/imgproc.hpp>

/*
this module detetcts cards in frames and return the detetcted card (conners, centroid, half) if any
*/

//preprocess the frame: convert to grayscale and apply gaussian blur    
cv::Mat preprocess(const cv::Mat& frame)
{
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

    return blurred;
}

//apply canny edge detection and morphological closing to the preprocessed frame
std::vector<DetectedCard> detect(const cv::Mat& frame, const Params& p)
{
    //preprocess the frame
    cv::Mat gray = preprocess(frame);

    //apply canny edge
    cv::Mat edgesImage = edgeMask(gray, p);

    //find contours in the edge mask and filter them based on area and shape
    std::vector<std::vector<cv::Point>> contours = findCandidates(edgesImage);

    std::vector<DetectedCard> detectedCards;

    for (const auto& contour : contours) {
        std::vector<cv::Point> corners;
        if (isCardQuad(contour, frame.size(), p, corners)) {
            DetectedCard card = buildCard(corners, frame.size());
            detectedCards.push_back(card);
        }
    }

    return detectedCards;
}

//edge mask: suppress texture, then Canny + morphological closing
cv::Mat edgeMask(const cv::Mat& gray, const Params& p)
{
    //auto Canny thresholds from the mean (no magic numbers)
    double meanVal = cv::mean(gray)[0];
    double lower = std::max(0.0, (1.0 - p.cannySigma) * meanVal);
    double upper = std::min(255.0, (1.0 + p.cannySigma) * meanVal);

    cv::Mat edgesImage;
    cv::Canny(gray, edgesImage, lower, upper);

    //morphological closing to join broken edges
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(p.closeKernel, p.closeKernel));
    cv::morphologyEx(edgesImage, edgesImage, cv::MORPH_CLOSE, kernel);

    return edgesImage;
}

//find contours in the edge mask and filter them based on area and shape
std::vector<std::vector<cv::Point>> findCandidates(const cv::Mat& mask)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

//check if a contour is a valid card candidate based on area and aspect ratio
bool isCardQuad(const std::vector<cv::Point>& contour, cv::Size frameSize, const Params& p, std::vector<cv::Point>& corners)
{
    //calculate the contour area and compare it to the frame area
    double area = cv::contourArea(contour);
    double frameArea = frameSize.width * frameSize.height;
    double areaRatio = area / frameArea;

    //if too small or too large, discard the contour
    if (areaRatio < p.minAreaRatio || areaRatio > p.maxAreaRatio)
        return false;

    //minimum-area rotated rectangle enclosing the contour: always gives 4 corners
    cv::RotatedRect rr = cv::minAreaRect(contour);
    double w = rr.size.width;
    double h = rr.size.height;
    double rectArea = w * h;
    if (rectArea <= 0)
        return false;

    //extent: how much the contour fills its rectangle (a full rectangle ~1, an L/blob less)
    double extent = area / rectArea;
    if (extent < p.minExtent)
        return false;

    //aspect ratio of the sides (rotation invariant)
    double aspectRatio = std::min(w, h) / std::max(w, h);
    if (aspectRatio < p.minAspect || aspectRatio > p.maxAspect)
        return false;

    //the 4 corners come from the rotated rectangle
    cv::Point2f pts[4];
    rr.points(pts);
    corners.clear();
    for (int i = 0; i < 4; ++i)
        corners.push_back(cv::Point(cvRound(pts[i].x), cvRound(pts[i].y)));

    return true;
}

//build a DetectedCard object from a valid card contour
DetectedCard buildCard(const std::vector<cv::Point>& corners, cv::Size frameSize)
{
    //calculate the centroid of the card using image moments
    cv::Moments mom = cv::moments(corners);

    float cx = static_cast<float>(mom.m10 / mom.m00);
    float cy = static_cast<float>(mom.m01 / mom.m00);
    cv::Point2f centroid(cx, cy);

    //determine if the card is in the north or south half of the frame
    float midY = frameSize.height / 2.0f;
    Half half;
    if (centroid.y < midY){
        half = Half::North;
    }else{
        half = Half::South;
    }

    return DetectedCard{corners, centroid, half};
}
