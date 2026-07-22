//Author: Francesco Pizzato
#include "../include/CardRectifier.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

/*
this module trasnforms the detected card in a normlized retangle for comparisino with templates
*/

//order the corners of the detected card in a consistent manner [TL, TR, BR, BL]
std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point>& corners)
{
    // TL = min(x+y), BR = max(x+y), TR = max(x-y), BL = min(x-y)
    cv::Point2f tl = corners[0], tr = corners[0], br = corners[0], bl = corners[0];

    double minSum  = corners[0].x + corners[0].y;
    double maxSum  = minSum;
    double minDiff = corners[0].x - corners[0].y;
    double maxDiff = minDiff;

    for (const cv::Point& pt : corners) {
        double sum  = pt.x + pt.y;
        double diff = pt.x - pt.y;

        if (sum < minSum) {
            minSum = sum;
            tl = pt;
        }
        if (sum > maxSum) {
            maxSum = sum;
            br = pt;
        }
        if (diff > maxDiff) {
            maxDiff = diff;
            tr = pt;
        }
        if (diff < minDiff) {
            minDiff = diff;
            bl = pt;
        }
    }

    return { tl, tr, br, bl };
}

//apply perspective transformation to rectify the detected card
cv::Mat rectify(const cv::Mat& frame, const std::vector<cv::Point>& corners)
{
    std::vector<cv::Point2f> orderedCorners = orderCorners(corners);

    //calculate the width and height of the new image 
    double widthA = cv::norm(orderedCorners[2] - orderedCorners[3]);
    double widthB = cv::norm(orderedCorners[1] - orderedCorners[0]);
    double maxWidth = std::max(widthA, widthB);

    double heightA = cv::norm(orderedCorners[1] - orderedCorners[2]);
    double heightB = cv::norm(orderedCorners[0] - orderedCorners[3]);
    double maxHeight = std::max(heightA, heightB);

    //define the destination points for the perspective transform
    std::vector<cv::Point2f> dst = {
        cv::Point2f(0, 0),
        cv::Point2f(maxWidth - 1, 0),
        cv::Point2f(maxWidth - 1, maxHeight - 1),
        cv::Point2f(0, maxHeight - 1)
    };

    //compute the perspective transform matrix and apply it to the image
    cv::Mat M = cv::getPerspectiveTransform(orderedCorners, dst);
    cv::Mat straightImage;
    //apply the perspective transformation to the original frame to obtain the rectified card image
    cv::warpPerspective(frame, straightImage, M, cv::Size(static_cast<int>(maxWidth), static_cast<int>(maxHeight)));

    return straightImage;
}