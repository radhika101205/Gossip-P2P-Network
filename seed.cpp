#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utils.h"
#include <algorithm> 
#include <cstring>

std::vector<std::string> peerList;
std::mutex plMutex; 

void handlePeerConnection(int clientSocket) {
    char buffer[1024] = {0};
    int valread = read(clientSocket, buffer, 1024);
    
    if (valread > 0) {
        std::string msg(buffer);
        std::cout << "Received message: " << msg << std::endl;

        if (msg.find("REGISTER:") == 0) {
            std::string peerInfo = msg.substr(9); 
            
            // Thread-safe addition to PL
            {
                std::lock_guard<std::mutex> lock(plMutex);
                if (std::find(peerList.begin(), peerList.end(), peerInfo) == peerList.end()) {
                    peerList.push_back(peerInfo);
                    std::cout << "[LOG] Consensus outcome: Added " << peerInfo << " to PL." << std::endl;
                    logToFile("outputfile.txt", "[LOG] Consensus outcome: Added " + peerInfo + " to PL.");
                }
            }

            // Send PL back
            std::string plResponse = "PL:";
            {
                std::lock_guard<std::mutex> lock(plMutex);
                for (size_t i = 0; i < peerList.size(); ++i) {
                    plResponse += peerList[i];
                    if (i < peerList.size() - 1) plResponse += ",";
                }
            }
            send(clientSocket, plResponse.c_str(), plResponse.length(), 0);
        }
        else if (msg.find("Dead Node:") == 0) {
            size_t firstColon = msg.find(':');
            size_t secondColon = msg.find(':', firstColon + 1);
            size_t thirdColon = msg.find(':', secondColon + 1);
            
            if (firstColon != std::string::npos && thirdColon != std::string::npos) {
                std::string deadIPPort = msg.substr(firstColon + 1, thirdColon - firstColon - 1);
                
                // Thread-safe removal from PL
                {
                    std::lock_guard<std::mutex> lock(plMutex);
                    auto it = std::find(peerList.begin(), peerList.end(), deadIPPort);
                    if (it != peerList.end()) {
                        peerList.erase(it);
                        std::cout << "[LOG] Consensus outcome: Removed dead node " << deadIPPort << " from PL.\n";
                        logToFile("outputfile.txt", "[LOG] Consensus outcome: Removed dead node " + deadIPPort + " from PL.");
                    }
                }
            }
        }
    }
    close(clientSocket);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <Port_Number>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) return 1;

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) return 1;
    if (listen(serverSocket, 10) < 0) return 1;

    std::cout << "Seed Node started. Listening on port " << port << "...\n";

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) continue;

        std::thread worker(handlePeerConnection, clientSocket);
        worker.detach();
    }

    close(serverSocket);
    return 0;
}