#include "MatchTheory.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef PRINT_H_INCLUDED
#define PRINT_H_INCLUDED

//explanation of the pipeline, printed at the start of the program
void printIntro(const std::string& game);

//csv in the same format of the ground truth
bool writeCSV(const std::vector<RoundResult>& results, const std::string& csvPath);

#endif
