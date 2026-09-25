//Author: Francesco Pizzato
#include "../include/CardRectifier.h"

/*
this module transforms the detected card in a normalized retangle for comparison with templates
*/

//order the corners of the detected card in a consistent manner [TL, TR, BR, BL]
//robust to ANY rotation: sort by angle around the centroid
std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point>& corners)
{
    //centroid of the card (mean of the 4 corners)
    cv::Point2f center(0.f, 0.f);
    for (const cv::Point& p : corners)
        center += cv::Point2f((float) p.x, (float) p.y);
    center *= (1.0f / (float) corners.size());

    //copy to Point2f
    std::vector<cv::Point2f> pts;
    for (const cv::Point& p : corners)
        pts.push_back(cv::Point2f((float) p.x, (float) p.y));

    //sort by angle around the centroid 
    std::sort(pts.begin(), pts.end(),
        [&center](const cv::Point2f& a, const cv::Point2f& b) {
            return std::atan2(a.y - center.y, a.x - center.x)
                 < std::atan2(b.y - center.y, b.x - center.x);
        });

    //start the sequence from the top-left corner 
    int position = 0;
    float bestSum = pts[0].x + pts[0].y;
    for (int k = 1; k < (int) pts.size(); ++k) {
        float s = pts[k].x + pts[k].y;
        if (s < bestSum) {
            bestSum = s;
            position = k;
        }
    }
    std::rotate(pts.begin(), pts.begin() + position, pts.end());

    return pts;
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

    //normalize to portrait: cards are taller than wide.
    //if it came out landscape (e.g. the horizontal briscola), rotate 90° -> portrait
    if (straightImage.cols > straightImage.rows)
        cv::rotate(straightImage, straightImage, cv::ROTATE_90_CLOCKWISE);

    return straightImage;
}