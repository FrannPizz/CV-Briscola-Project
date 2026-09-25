#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <vector>
#include <cmath>

#ifndef CARDRECTIFIER_H_INCLUDED
#define CARDRECTIFIER_H_INCLUDED

std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point>& corners);

cv::Mat rectify(const cv::Mat& frame, const std::vector<cv::Point>& corners);

#endif 
