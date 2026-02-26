#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <utility>

// Function to read the seed IP and Port pairs from the config file
std::vector<std::pair<std::string, int>> parseConfig(const std::string& filename);

void logToFile(const std::string& filename, const std::string& message);

#endif