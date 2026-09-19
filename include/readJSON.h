#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#ifndef READJSON_H
#define READJSON_H

struct JSONCardInfo {
    int round = 0;
    int frame = 0;
    std::vector<cv::Point> corners;
};

std::vector<JSONCardInfo> readJSON(const std::string& jsonFilePath);

#endif 