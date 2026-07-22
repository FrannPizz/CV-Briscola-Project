#ifndef CARDRECTIFIER_H_INCLUDED
#define CARDRECTIFIER_H_INCLUDED

#include <vector>
#include <opencv2/core.hpp>

std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point>& corners);

cv::Mat rectify(const cv::Mat& frame, const std::vector<cv::Point>& corners);

#endif 
