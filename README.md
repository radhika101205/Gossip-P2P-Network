# CSL3080 Assignment 1: Gossip-Based P2P Network

**Completed by:**
    Radhika Agarwal (B23ES1027)

## Description
This project implements a decentralized, gossip-based Peer-to-Peer (P2P) network using multithreaded POSIX sockets in C++. The architecture consists of two primary node types:
* **Seed Nodes (`seed.cpp`):** Act as the network's gatekeepers. They maintain the official Peer List (PL), handle incoming peer registrations, and remove dead nodes upon receiving consensus reports.
* **Peer Nodes (`peer.cpp`):** The active participants of the network. They connect to a majority of seed nodes to register, retrieve the active PL, and establish connections with neighbors.

### Key Features Implemented:
* **Multithreaded Architecture:** Both seeds and peers utilize `std::thread` to handle concurrent connections without blocking the main execution loop.
* **Gossip Protocol:** Peers broadcast a uniquely hashed message (`<timestamp>:<IP>:<Msg#>`) to all active neighbors every 5 seconds (up to 10 messages).
* **Message Tracking (ML):** Peers maintain a thread-safe Message List to identify and drop duplicate gossip messages, preventing infinite network loops.
* **Liveness Detection:** Peers run a background thread that pings neighbors every 10 seconds. If a TCP connection fails, the node is suspected dead, and a `Dead Node` report is forwarded to the seeds.
* **Persistent Logging:** All consensus outcomes, peer list retrievals, and gossip messages are logged to both the terminal console and `outputfile.txt`.

## File Structure
* `peer.cpp` - Implementation of the Peer node client/server logic.
* `seed.cpp` - Implementation of the Seed node server logic.
* `utils.h` / `utils.cpp` - Shared utilities for parsing the configuration file and writing logs.
* `config.txt` - Configuration file containing the IP and Port pairs of the available Seed nodes.
* `outputfile.txt` - Dynamically generated file containing the required network event logs.

## Code Architecture

[cite_start]The implementation follows a modular, multithreaded approach to ensure high availability and non-blocking operations. [cite: 10, 11]

### Seed Node (`seed.cpp`)
* [cite_start]**`main()`**: Initializes the TCP server socket, binds to the specified port from the config, and enters an infinite `accept()` loop. [cite: 15, 23]
* [cite_start]**`handlePeerConnection(int clientSocket)`**: A worker thread function that parses incoming messages. [cite: 18]
    * [cite_start]**Registration**: Implements "Consensus-based registration" by adding peers to the Peer List (PL) and returning the current PL union to the requester. [cite: 16, 17, 18]
    * [cite_start]**Dead Node Removal**: Implements "Consensus-based removal" by processing death reports and deleting the specified peer from the PL. [cite: 19, 20, 32]

### Peer Node (`peer.cpp`)
* [cite_start]**`main()`**: Bootstraps the peer by reading `config.txt`, randomly selecting seeds, and initiating the registration handshake. [cite: 5, 23, 24]
* [cite_start]**`startPeerServer()`**: A background thread that listens for gossip messages from neighbors. [cite: 25] [cite_start]It maintains the **Message List (ML)** to prevent redundant forwarding. [cite: 26, 40, 41]
* [cite_start]**`startGossiping()`**: A background thread that generates a unique message every 5 seconds (max 10) and broadcasts it to all neighbors. [cite: 35, 37, 39]
* [cite_start]**`startLivenessDetection()`**: A background thread that periodically pings neighbors via TCP connection attempts. [cite: 27] [cite_start]If a neighbor fails to respond, it generates a `Dead Node` report for the seeds. [cite: 28, 29, 30]

### Shared Utilities (`utils.cpp`)
* [cite_start]**`parseConfig()`**: Standardizes the reading of IP-Port pairs for seeds. [cite: 5, 23]
* [cite_start]**`logToFile()`**: Ensures all required logs (registrations, gossip, and removals) are persisted to `outputfile.txt`. [cite: 45, 46]

## Prerequisites
* A Linux/macOS environment (or WSL on Windows).
* GCC compiler with C++11 support.
* POSIX Threads (`pthread`) library.

## Compilation Instructions
Open your terminal and run the following commands to compile the executables:

**Compile the Seed Node:**
```bash
g++ -std=c++11 seed.cpp utils.cpp -o seed -pthread
```

**Compile the Peer Node:**
```bash
g++ -std=c++11 peer.cpp utils.cpp -o peer -pthread
```
## Execution Instructions
Note: Ensure `config.txt` is present in the same directory before running.* [cite: 60]

**1. Start the Seed Nodes**
Open separate terminal windows for each seed defined in your `config.txt` and start them by passing their assigned port:
```bash
./seed 8000
./seed 8001
```
**2. Start the Peer Nodes**
Open separate terminal windows for each peer. Pass the peer's own IP address and a unique port number to start it:
```bash
./peer 127.0.0.1 8080
./peer 127.0.0.1 8081
./peer 127.0.0.1 8082
```
**3. Simulating a Node Failure**
To test the liveness detection and consensus removal, forcefully terminate one of the running Peer terminals using `Ctrl+C`. Within 10 seconds, the neighboring peers will detect the unresponsiveness, log a `[LIVENESS]` warning, and instruct the seeds to remove the dead node from the official network list.

