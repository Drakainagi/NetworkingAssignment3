/* Start Header
*********************************************************************
  \file    ftpclient.cpp
  \author  weijie.soh
  \par     DigiPen Institute of Technology
  \date    6 March 2025
  \brief
         This file implements a multi-threaded FTP client that communicates
         with the server via TCP for control messages and via UDP for file
         downloading. The client supports:
           - Requesting the file list (/l)
           - Requesting a file download (/d <client_ip:udpPort> <filename>)
           - Quitting (/q)
         The UDP file transfer uses a basic reliable protocol (stop-and-wait
         with ACKs) to ensure data integrity over a lossy channel.

         Note: This implementation re-uses code portions from Assignment 2
         where applicable and includes clear commenting and variable naming.

         THIS FILE IS STILL IN DEVELOPMENT AND MIGHT NOT WORK AS INTENDED
  Copyright (C) 2025 DigiPen Institute of Technology.
*********************************************************************/
/* End Header
*********************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>
#include <cstring>
#include <cstdlib>

// ---------------------- Command ID Definitions --------------------------
#define CMD_REQ_QUIT         0x1    // /q
#define CMD_REQ_DOWNLOAD     0x2    // /d
#define CMD_RSP_DOWNLOAD     0x3    // Server reply: download info (IP, UDP port, session, file length)
#define CMD_REQ_LISTFILES    0x4    // /l
#define CMD_RSP_LISTFILES    0x5    // Server reply: file list
#define CMD_CMDTEST          0x20
#define CMD_DOWNLOAD_ERROR   0x30

// ---------------------- Global Variables -------------------------------

// Global download storage path (provided at startup)
std::string g_downloadPath;

// Global structure for a file download session
struct FileDownloadSession {
    std::string filename;      // Full path of the file being downloaded
    std::ofstream file;        // Output file stream
    uint32_t expectedFileLength; // Total file size in bytes
    uint32_t receivedBytes;    // Count of bytes received so far
};

// Map of active download sessions: key = session ID
std::mutex g_sessionMutex;
std::map<uint32_t, FileDownloadSession> g_sessions;

// For handling pending filename from a /d command (assumes one active download at a time)
std::mutex g_pendingMutex;
std::string g_pendingFilename;

// ---------------------- Helper Functions -------------------------------

// Parse a destination string in the form "ip:port" into separate IP and port variables.
bool parseDestination(const std::string& dest, std::string& ip, uint16_t& port) {
    size_t colonPos = dest.find(':');
    if (colonPos == std::string::npos)
        return false;
    ip = dest.substr(0, colonPos);
    std::string portStr = dest.substr(colonPos + 1);
    port = static_cast<uint16_t>(std::atoi(portStr.c_str()));
    return true;
}

// ---------------------- UDP Receiver Thread ----------------------------
// This function receives file data from the server over UDP.
// It expects each UDP packet to contain:
//   - Session ID [4 bytes]
//   - File Length [4 bytes]
//   - File Offset [4 bytes]
//   - File Data Length [4 bytes]
//   - File Data [variable length]
// After writing the file data at the correct offset, the function sends an ACK back.
void udpReceiverThread(SOCKET udpSocket) {
    char buffer[1500]; // Buffer size for UDP datagrams
    sockaddr_in senderAddr;
    int senderAddrLen = sizeof(senderAddr);

    while (true) {
        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&senderAddr), &senderAddrLen);
        if (bytesReceived <= 0)
            continue; // Error or no data received

        // Ensure packet is large enough for header fields (16 bytes minimum)
        if (bytesReceived < 16)
            continue;

        // Parse header fields (all in network byte order)
        uint32_t netSessionId;
        memcpy(&netSessionId, buffer, 4);
        uint32_t sessionId = ntohl(netSessionId);

        uint32_t netFileLength;
        memcpy(&netFileLength, buffer + 4, 4);
        uint32_t fileLength = ntohl(netFileLength);

        uint32_t netFileOffset;
        memcpy(&netFileOffset, buffer + 8, 4);
        uint32_t fileOffset = ntohl(netFileOffset);

        uint32_t netDataLength;
        memcpy(&netDataLength, buffer + 12, 4);
        uint32_t dataLength = ntohl(netDataLength);

        // Verify that the packet contains all file data bytes
        if (bytesReceived < 16 + dataLength)
            continue;

        char* fileData = buffer + 16;

        // Write file data to the appropriate download session
        {
            std::lock_guard<std::mutex> lock(g_sessionMutex);
            auto it = g_sessions.find(sessionId);
            if (it == g_sessions.end()) {
                // No active session with this ID; ignore packet.
                continue;
            }
            FileDownloadSession& session = it->second;
            session.file.seekp(fileOffset, std::ios::beg);
            session.file.write(fileData, dataLength);
            session.receivedBytes += dataLength;

            // If the complete file has been received, finalize the session.
            if (session.receivedBytes >= session.expectedFileLength) {
                session.file.close();
                std::cout << "Download complete: " << session.filename << " ("
                    << session.expectedFileLength << " bytes)" << std::endl;
                g_sessions.erase(it);
            }
        }

        // Construct and send an ACK over UDP.
        // ACK packet format: 1 byte Flags, 4 bytes ACK Number, 4 bytes Sequence Number.
        // For simplicity, we use fileOffset as the ACK number.
        char ackPacket[9];
        ackPacket[0] = 0x1; // LSB = 1 indicates ACK.
        uint32_t netAck = htonl(fileOffset);
        memcpy(ackPacket + 1, &netAck, 4);
        memcpy(ackPacket + 5, &netAck, 4); // Duplicate for Sequence Number.
        sendto(udpSocket, ackPacket, 9, 0, reinterpret_cast<sockaddr*>(&senderAddr), senderAddrLen);
    }
}

// ---------------------- TCP Receiver Thread ----------------------------
// This function continuously receives TCP messages from the server and processes them.
void receiveFromServer(SOCKET clientSocket, SOCKET udpSocket) {
    std::vector<uint8_t> recvBuffer;
    uint8_t tempBuffer[1024];

    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(tempBuffer), sizeof(tempBuffer), 0);
        if (bytesReceived == 0) {
            std::cerr << "Server closed the connection." << std::endl;
            break;
        }
        if (bytesReceived < 0) {
            std::cerr << "Error receiving data from server." << std::endl;
            break;
        }
        // Append received bytes to our buffer.
        recvBuffer.insert(recvBuffer.end(), tempBuffer, tempBuffer + bytesReceived);

        // Process complete messages in the buffer.
        while (!recvBuffer.empty()) {
            uint8_t command = recvBuffer[0];

            // --- Process RSP_LISTFILES ---
            if (command == CMD_RSP_LISTFILES) {
                // Message format:
                //   1 byte command,
                //   2 bytes: number of files,
                //   4 bytes: total length of file list data,
                //   then, for each file:
                //       4 bytes: filename length,
                //       filename string.
                if (recvBuffer.size() < 1 + 2 + 4)
                    break; // Wait for more data

                uint16_t netNumFiles;
                memcpy(&netNumFiles, &recvBuffer[1], 2);
                uint16_t numFiles = ntohs(netNumFiles);

                uint32_t netListLength;
                memcpy(&netListLength, &recvBuffer[3], 4);
                uint32_t listLength = ntohl(netListLength);

                if (recvBuffer.size() < 1 + 2 + 4 + listLength)
                    break; // Incomplete message

                std::cout << "----- Available Files -----" << std::endl;
                size_t offset = 7;
                for (int i = 0; i < numFiles; i++) {
                    if (offset + 4 > recvBuffer.size())
                        break;
                    uint32_t netFilenameLen;
                    memcpy(&netFilenameLen, &recvBuffer[offset], 4);
                    uint32_t filenameLen = ntohl(netFilenameLen);
                    offset += 4;
                    if (offset + filenameLen > recvBuffer.size())
                        break;
                    std::string filename(reinterpret_cast<char*>(&recvBuffer[offset]), filenameLen);
                    offset += filenameLen;
                    std::cout << filename << std::endl;
                }
                std::cout << "---------------------------" << std::endl;
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + 7 + listLength);
            }
            // --- Process RSP_DOWNLOAD ---
            else if (command == CMD_RSP_DOWNLOAD) {
                // Message format:
                //   1 byte command,
                //   4 bytes: server IP,
                //   2 bytes: server UDP port,
                //   4 bytes: session ID,
                //   4 bytes: file length.
                if (recvBuffer.size() < 1 + 4 + 2 + 4 + 4)
                    break; // Wait for more data

                uint32_t netServerIP;
                memcpy(&netServerIP, &recvBuffer[1], 4);
                in_addr serverAddr;
                serverAddr.s_addr = netServerIP;
                char serverIPStr[INET_ADDRSTRLEN] = { 0 };
                inet_ntop(AF_INET, &serverAddr, serverIPStr, INET_ADDRSTRLEN);

                uint16_t netServerUDPPort;
                memcpy(&netServerUDPPort, &recvBuffer[5], 2);
                uint16_t serverUDPPort = ntohs(netServerUDPPort);

                uint32_t netSessionID;
                memcpy(&netSessionID, &recvBuffer[7], 4);
                uint32_t sessionID = ntohl(netSessionID);

                uint32_t netFileLength;
                memcpy(&netFileLength, &recvBuffer[11], 4);
                uint32_t fileLength = ntohl(netFileLength);

                std::cout << "Download starting from server " << serverIPStr << ":" << serverUDPPort
                    << ", session: " << sessionID << ", file size: " << fileLength << " bytes" << std::endl;

                // Retrieve the pending filename from the /d command.
                std::string filename;
                {
                    std::lock_guard<std::mutex> lock(g_pendingMutex);
                    filename = g_pendingFilename;
                    g_pendingFilename = ""; // Clear pending filename
                }
                if (filename.empty())
                    filename = "downloaded_file";

                // Construct the full path for the downloaded file.
                std::string fullPath = g_downloadPath + "\\" + filename;

                // Create a new download session.
                FileDownloadSession session;
                session.filename = fullPath;
                session.expectedFileLength = fileLength;
                session.receivedBytes = 0;
                session.file.open(fullPath, std::ios::binary | std::ios::out);
                if (!session.file.is_open()) {
                    std::cerr << "Error opening file for download: " << fullPath << std::endl;
                }
                else {
                    std::cout << "Saving file to: " << fullPath << std::endl;
                    std::lock_guard<std::mutex> lock(g_sessionMutex);
                    g_sessions[sessionID] = std::move(session);
                }
                // Erase the processed RSP_DOWNLOAD message.
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + 1 + 4 + 2 + 4 + 4);
            }
            // --- Process DOWNLOAD_ERROR ---
            else if (command == CMD_DOWNLOAD_ERROR) {
                std::cout << "Download error received from server." << std::endl;
                recvBuffer.erase(recvBuffer.begin()); // Remove command byte.
            }
            // --- Process Server-Initiated Quit ---
            else if (command == CMD_REQ_QUIT) {
                std::cout << "Server requested quit." << std::endl;
                recvBuffer.erase(recvBuffer.begin());
                break;
            }
            else {
                // Unknown command: remove one byte and continue.
                recvBuffer.erase(recvBuffer.begin());
            }
        }
    }
}

// ---------------------- User Input Handler -----------------------------
// This function reads user input from the console and constructs TCP messages
// according to the directives:
//   - /q: Quit
//   - /l: Request file list
//   - /d: Request file download (format: /d <client_ip:udpPort> <filename>)
void handleUserInput(SOCKET clientSocket) {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input.empty())
            continue;

        // /q: Quit command.
        if (input == "/q") {
            uint8_t msg = CMD_REQ_QUIT;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
            break;
        }
        // /l: Request file list.
        else if (input == "/l") {
            uint8_t msg = CMD_REQ_LISTFILES;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
        }
        // /d: Request file download.
        // Expected format: /d <client_ip:udpPort> <filename>
        else if (input.rfind("/d", 0) == 0) {
            std::istringstream iss(input);
            std::string command, destField, filename;
            iss >> command >> destField >> filename;
            if (destField.empty() || filename.empty()) {
                std::cerr << "Invalid /d command format. Usage: /d <client_ip:udpPort> <filename>" << std::endl;
                continue;
            }
            std::string clientIP;
            uint16_t clientUDPPort;
            if (!parseDestination(destField, clientIP, clientUDPPort)) {
                std::cerr << "Invalid destination format. Use ip:port" << std::endl;
                continue;
            }
            // Construct REQ_DOWNLOAD message:
            //   1 byte: CMD_REQ_DOWNLOAD,
            //   4 bytes: client IP,
            //   2 bytes: client UDP port,
            //   4 bytes: filename length,
            //   variable: filename string.
            std::vector<uint8_t> buffer;
            buffer.push_back(CMD_REQ_DOWNLOAD);

            in_addr addr;
            if (inet_pton(AF_INET, clientIP.c_str(), &addr) != 1) {
                std::cerr << "Invalid IP address: " << clientIP << std::endl;
                continue;
            }
            uint8_t* ipBytes = reinterpret_cast<uint8_t*>(&addr.s_addr);
            buffer.insert(buffer.end(), ipBytes, ipBytes + 4);

            uint16_t netUDPPort = htons(clientUDPPort);
            uint8_t portBytes[2];
            memcpy(portBytes, &netUDPPort, 2);
            buffer.insert(buffer.end(), portBytes, portBytes + 2);

            uint32_t filenameLen = static_cast<uint32_t>(filename.size());
            uint32_t netFilenameLen = htonl(filenameLen);
            uint8_t filenameLenBytes[4];
            memcpy(filenameLenBytes, &netFilenameLen, 4);
            buffer.insert(buffer.end(), filenameLenBytes, filenameLenBytes + 4);

            buffer.insert(buffer.end(), filename.begin(), filename.end());

            // Store the filename as pending so it can be associated with the download session.
            {
                std::lock_guard<std::mutex> lock(g_pendingMutex);
                g_pendingFilename = filename;
            }

            send(clientSocket, reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
        }
        else {
            std::cerr << "Unknown command. Supported commands: /q, /l, /d" << std::endl;
        }
    }
}

// ---------------------- Main Function ----------------------------------
int main() {
    // -------------------- Prompt for Initial Parameters ------------------
    std::string serverIP;
    std::cout << "Server IP Address: ";
    std::getline(std::cin, serverIP);

    std::string serverTCPPort;
    std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, serverTCPPort);

    std::string serverUDPPort;
    std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, serverUDPPort);

    std::string clientUDPPortStr;
    std::cout << "Client UDP Port Number: ";
    std::getline(std::cin, clientUDPPortStr);
    uint16_t clientUDPPort = static_cast<uint16_t>(std::atoi(clientUDPPortStr.c_str()));

    std::cout << "Download Storage Path: ";
    std::getline(std::cin, g_downloadPath);

    // -------------------- Initialize Winsock -------------------------------
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup() failed." << std::endl;
        return 1;
    }

    // -------------------- Resolve Server Address (TCP) ---------------------
    addrinfo hints{}, * info = nullptr;
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int errorCode = getaddrinfo(serverIP.c_str(), serverTCPPort.c_str(), &hints, &info);
    if (errorCode != 0 || info == nullptr) {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return 1;
    }

    // -------------------- Create and Connect TCP Socket --------------------
    SOCKET clientSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

    errorCode = connect(clientSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (errorCode == SOCKET_ERROR) {
        std::cerr << "connect() failed." << std::endl;
        freeaddrinfo(info);
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(info);

    // -------------------- Create UDP Socket for File Downloads -------------
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    sockaddr_in clientUDPAddr;
    clientUDPAddr.sin_family = AF_INET;
    clientUDPAddr.sin_addr.s_addr = INADDR_ANY;
    clientUDPAddr.sin_port = htons(clientUDPPort);
    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&clientUDPAddr), sizeof(clientUDPAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind UDP socket." << std::endl;
        closesocket(udpSocket);
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // -------------------- Launch Receiver Threads --------------------------
    // UDP thread for handling file data over UDP.
    std::thread udpThread(udpReceiverThread, udpSocket);

    // TCP thread for processing server control messages.
    std::thread tcpReceiverThread(receiveFromServer, clientSocket, udpSocket);

    // Main thread handles user input and sends commands over TCP.
    handleUserInput(clientSocket);

    // Cleanup: close sockets and join threads.
    closesocket(clientSocket);
    closesocket(udpSocket);
    tcpReceiverThread.join();
    udpThread.join();
    WSACleanup();

    return 0;
}
