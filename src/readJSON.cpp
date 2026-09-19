//Author: Francesco Pizzato
#include "../include/readJSON.h"

/*
this module reads the detected cards from the json: one JSONCardInfo per detected card
*/

std::vector<JSONCardInfo> readJSON(const std::string& jsonFilePath) {

    std::vector<JSONCardInfo> cardInfos;

    //read the json
    std::ifstream file(jsonFilePath);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + jsonFilePath);
    }

    //parse the whole json once
    nlohmann::json jsonData;
    file >> jsonData;

    //for every detected card in the json
    for (const auto& card : jsonData) {
        if (!card.is_object()) {
            continue;
        }

        //one JSONCardInfo per detected card 
        JSONCardInfo info;
        info.round = card.value("round", 0);
        info.frame = card.value("frame", 0);

        //read the 4 real corners of the card 
        if (card.contains("corners") && card["corners"].is_array()) {
            for (const auto& pt : card["corners"]) {
                int x = (int) pt[0].get<float>();
                int y = (int) pt[1].get<float>();
                info.corners.push_back(cv::Point(x, y));
            }
        }
        //put card information into the vector of detected cards  
        cardInfos.push_back(info);
    }
    
    //return all cards informations
    return cardInfos;
}
