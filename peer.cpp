#include <iostream>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <random>
#include "utils.h"
#include <thread>
#include <set>
#include <sstream>
#include <chrono> 
#include <mutex>

// Global state
std::set<std::string> myNeighbors;      
std::set<std::string> messageList;      
std::mutex mlMutex;                     
std::string myGlobalIP;                 
int myGlobalPort;                       

// Peer Server Thread
void startPeerServer() { 
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(myGlobalPort);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) return;
    listen(serverSocket, 10);
    
    std::cout << "[LOG] Peer is now listening for gossip on port " << myGlobalPort << "\n";
    logToFile("outputfile.txt", "[LOG] Peer is now listening for gossip on port " + std::to_string(myGlobalPort));

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket >= 0) {
            char buffer[1024] = {0};
            int valread = read(clientSocket, buffer, 1024);
            if (valread > 0) {
                std::string msg(buffer, valread); 
                
                bool isNewMessage = false;
                {
                    std::lock_guard<std::mutex> lock(mlMutex);
                    if (messageList.find(msg) == messageList.end()) {
                        messageList.insert(msg);
                        isNewMessage = true;
                    }
                }

                if (isNewMessage) {
                    std::cout << "\n[GOSSIP RECEIVED - NEW] " << msg << "\n";
                    logToFile("outputfile.txt", "[GOSSIP RECEIVED - NEW] " + msg);
                }
            }
            close(clientSocket);
        }
    }
}

// Gossip Broadcast Thread
void startGossiping() { 
    for (int i = 1; i <= 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::string gossipMsg = std::to_string(timestamp) + ":" + myGlobalIP + ":" + std::to_string(i);

        std::cout << "\n[BROADCASTING] " << gossipMsg << "\n";

        for (const auto& neighbor : myNeighbors) {
            size_t colonPos = neighbor.find(':');
            if (colonPos != std::string::npos) {
                std::string nIP = neighbor.substr(0, colonPos);
                int nPort = std::stoi(neighbor.substr(colonPos + 1));

                int sock = socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in neighborAddr;
                neighborAddr.sin_family = AF_INET;
                neighborAddr.sin_port = htons(nPort);
                inet_pton(AF_INET, nIP.c_str(), &neighborAddr.sin_addr);

                if (connect(sock, (struct sockaddr*)&neighborAddr, sizeof(neighborAddr)) >= 0) {
                    send(sock, gossipMsg.c_str(), gossipMsg.length(), 0);
                }
                close(sock);
            }
        }
    }
}

// Liveness Detection Thread
void startLivenessDetection() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10)); 

        for (auto it = myNeighbors.begin(); it != myNeighbors.end(); ) {
            std::string neighbor = *it;
            size_t colonPos = neighbor.find(':');
            
            if (colonPos != std::string::npos) {
                std::string nIP = neighbor.substr(0, colonPos);
                int nPort = std::stoi(neighbor.substr(colonPos + 1));

                int sock = socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in neighborAddr;
                neighborAddr.sin_family = AF_INET;
                neighborAddr.sin_port = htons(nPort);
                inet_pton(AF_INET, nIP.c_str(), &neighborAddr.sin_addr);

                // Simulate ping
                if (connect(sock, (struct sockaddr*)&neighborAddr, sizeof(neighborAddr)) < 0) {
                    std::cout << "\n[LIVENESS] Neighbor " << neighbor << " is un-responsive! Suspecting dead.\n";
                    logToFile("outputfile.txt", "[LIVENESS] Neighbor " + neighbor + " is un-responsive! Suspecting dead.");
                    
                    auto now = std::chrono::system_clock::now();
                    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                    std::string deadMsg = "Dead Node:" + nIP + ":" + std::to_string(nPort) + ":" + std::to_string(timestamp) + ":" + myGlobalIP;
                    
                    // Forward to seeds
                    std::vector<std::pair<std::string, int>> seeds = parseConfig("config.txt");
                    for (const auto& seed : seeds) {
                        int seedSock = socket(AF_INET, SOCK_STREAM, 0);
                        sockaddr_in seedAddr;
                        seedAddr.sin_family = AF_INET;
                        seedAddr.sin_port = htons(seed.second);
                        inet_pton(AF_INET, seed.first.c_str(), &seedAddr.sin_addr);
                        
                        if (connect(seedSock, (struct sockaddr*)&seedAddr, sizeof(seedAddr)) >= 0) {
                            send(seedSock, deadMsg.c_str(), deadMsg.length(), 0);
                            close(seedSock);
                        }
                    }
                    it = myNeighbors.erase(it);
                } else {
                    close(sock);
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <My_IP> <My_Port>\n";
        return 1;
    }

    std::string myIP = argv[1];
    int myPort = std::stoi(argv[2]);

    myGlobalIP = myIP;
    myGlobalPort = myPort;

    std::vector<std::pair<std::string, int>> seeds = parseConfig("config.txt");
    int n = seeds.size();
    if (n == 0) return 1;

    int majority = (n / 2) + 1;
    std::cout << "Total seeds (n) = " << n << ". Need to connect to " << majority << " seeds.\n";

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(seeds.begin(), seeds.end(), g);
    
    std::vector<std::pair<std::string, int>> selectedSeeds(seeds.begin(), seeds.begin() + majority);

    // Connect to seeds
    for (const auto& seed : selectedSeeds) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        sockaddr_in seedAddr;
        seedAddr.sin_family = AF_INET;
        seedAddr.sin_port = htons(seed.second);
        inet_pton(AF_INET, seed.first.c_str(), &seedAddr.sin_addr);

        if (connect(sock, (struct sockaddr*)&seedAddr, sizeof(seedAddr)) < 0) {
            std::cerr << "Connection failed to Seed " << seed.first << ":" << seed.second << "\n";
        } else {
            std::cout << "Successfully connected to Seed " << seed.first << ":" << seed.second << "\n";
            
            std::string regMsg = "REGISTER:" + myIP + ":" + std::to_string(myPort);
            send(sock, regMsg.c_str(), regMsg.length(), 0);

            char buffer[2048] = {0};
            int valread = read(sock, buffer, 2048);
            if (valread > 0) {
                std::string response(buffer);
                if (response.find("PL:") == 0) {
                    std::string plData = response.substr(3);
                    
                    std::cout << "[LOG] Received Peer List from seed: " << plData << "\n";
                    logToFile("outputfile.txt", "[LOG] Received Peer List from seed: " + plData);
                    
                    std::stringstream ss(plData);
                    std::string token;
                    std::string myAddress = myIP + ":" + std::to_string(myPort);
                    
                    while (std::getline(ss, token, ',')) {
                        if (token != myAddress && !token.empty()) {
                            myNeighbors.insert(token);
                            std::cout << "[LOG] Added neighbor: " << token << "\n";
                            logToFile("outputfile.txt", "[LOG] Added neighbor: " + token);
                        }
                    }
                }
            }
        }
        close(sock);
    }
    
    std::set<std::string> myNeighbors; // Local placeholder, shadowed from global

    // Launch threads
    std::thread serverThread(startPeerServer);
    serverThread.detach();

    std::thread gossipThread(startGossiping);
    gossipThread.detach();

    std::thread livenessThread(startLivenessDetection);
    livenessThread.detach();

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}