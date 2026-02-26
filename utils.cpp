#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>

std::vector<std::pair<std::string, int>> parseConfig(const std::string& filename) {
    std::vector<std::pair<std::string, int>> seedNodes;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string ip;
        int port;

        // Extract the IP and Port from the line
        if (iss >> ip >> port) {
            seedNodes.push_back(std::make_pair(ip, port));
        } else {
            std::cerr << "Warning: Malformed line in config: " << line << std::endl;
        }
    }

    file.close();
    return seedNodes;
}

void logToFile(const std::string& filename, const std::string& message) {
    // std::ios::app opens the file in "append" mode so we don't overwrite previous logs
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        file << message << std::endl;
        file.close();
    }
}