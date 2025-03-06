/* Start Header
*****************************************************************/
/*!
  \file   ftpserver.cpp
  \author weijie.soh
  \par    DigiPen Institute of Technology
  \date   6 March 2025
  \brief
         This file implements a multi-threaded FTP server using Winsock and a thread pool.
         The server supports:
           - Listing available files for download (/l)
           - Requesting a file download (/d <client_ip:udpPort> <filename>)
           - Quitting (/q)
         Control messages are exchanged over TCP while the actual file transfer is done over UDP.
         The UDP file transfer implements a basic stop-and-wait protocol for reliability.
         File list messages (RSP_LISTFILES) and download messages (RSP_DOWNLOAD) follow the
         defined message formats.

         Robustness improvements include enhanced error checking, a RAII socket wrapper,
         a retry limit for UDP transfers, and modularized functions.

         THIS FILE IS STILL IN DEVELOPMENT AND MIGHT NOT WORK AS INTENDED
  Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header
*******************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if 0

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <chrono>
#include <iomanip>  // For std::put_time
// Link with Winsock library.
#pragma comment(lib, "ws2_32.lib")

// Include TaskQueue headers.
#include "taskqueue.h"

// --------------------- Command ID Definitions ---------------------------
#define CMD_REQ_QUIT        0x1    // /q: Request to quit.
#define CMD_REQ_DOWNLOAD    0x2    // /d: Request file download.
#define CMD_RSP_DOWNLOAD    0x3    // Server reply with download info.
#define CMD_REQ_LISTFILES   0x4    // /l: Request file list.
#define CMD_RSP_LISTFILES   0x5    // Server reply with file list.
#define CMD_DOWNLOAD_ERROR  0x30   // Error: file not found.

// --------------------- Configuration Constants --------------------------
constexpr int CHUNK_SIZE = 1024;     // UDP chunk size.
constexpr int ACK_TIMEOUT = 1000;    // Timeout in ms for receiving an ACK.
constexpr int MAX_RETRIES = 5;       // Maximum number of retransmissions per packet.

// --------------------- Simple Logger Helper -----------------------------
void Log(const std::string& level, const std::string& message)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm timeInfo;
    localtime_s(&timeInfo, &now_c);
    std::cout << "[" << level << "] "
        << std::put_time(&timeInfo, "%F %T")
        << " : " << message << std::endl;
}

// --------------------- RAII Socket Wrapper ------------------------------
class SocketRAII {
public:
    SocketRAII(SOCKET s = INVALID_SOCKET) : sock(s) {}
    ~SocketRAII() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
    }
    SOCKET get() const { return sock; }
    void reset(SOCKET s = INVALID_SOCKET) {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        sock = s;
    }
    SOCKET release() { SOCKET tmp = sock; sock = INVALID_SOCKET; return tmp; }
private:
    SOCKET sock;
};

// --------------------- Global Variables -------------------------------
std::string g_fileDirectory;   // Directory path for available files.
std::string g_serverIP;        // Server IP address (as a string).
uint16_t g_serverUDPPort = 0;  // Server UDP port.
SocketRAII g_udpSocket;        // Global UDP socket for file transfers.

std::atomic<uint32_t> g_sessionCounter{ 1 }; // Global session ID counter.
std::mutex g_udpMutex;                     // Mutex to serialize UDP send/receive.

// --------------------- Helper Functions -------------------------------

// Function: getFileList
// Scans the provided directory and returns a vector of filenames (excluding directories).
std::vector<std::string> getFileList(const std::string& directory) {
    std::vector<std::string> fileList;
    std::string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Exclude directories.
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                fileList.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    else {
        Log("ERROR", "FindFirstFileA failed for directory: " + directory);
    }
    return fileList;
}

// Function: fileExistsAndSize
// Checks if a file exists and retrieves its size.
bool fileExistsAndSize(const std::string& filePath, uint32_t& fileSize) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    fileSize = static_cast<uint32_t>(file.tellg());
    return true;
}

// Function: sendFileViaUDP
// Sends the file specified by filePath to the client (clientAddr) using the shared UDP socket.
// Implements a stop-and-wait protocol with a maximum number of retransmissions per packet.
void sendFileViaUDP(SOCKET udpSocket, const sockaddr_in& clientAddr, const std::string& filePath,
    uint32_t sessionId, uint32_t fileSize)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        Log("ERROR", "Failed to open file for UDP transfer: " + filePath);
        return;
    }

    uint32_t offset = 0;
    char dataBuffer[CHUNK_SIZE];

    // Set a receive timeout for ACKs.
    int timeout = ACK_TIMEOUT;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    while (offset < fileSize) {
        file.seekg(offset);
        file.read(dataBuffer, CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0)
            break;

        // Build UDP packet header.
        uint32_t netSessionId = htonl(sessionId);
        uint32_t netFileSize = htonl(fileSize);
        uint32_t netOffset = htonl(offset);
        uint32_t netDataLength = htonl(static_cast<uint32_t>(bytesRead));
        std::vector<uint8_t> packet;
        packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netSessionId),
            reinterpret_cast<uint8_t*>(&netSessionId) + 4);
        packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netFileSize),
            reinterpret_cast<uint8_t*>(&netFileSize) + 4);
        packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netOffset),
            reinterpret_cast<uint8_t*>(&netOffset) + 4);
        packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netDataLength),
            reinterpret_cast<uint8_t*>(&netDataLength) + 4);
        packet.insert(packet.end(), dataBuffer, dataBuffer + bytesRead);

        bool ackReceived = false;
        int retries = 0;
        while (!ackReceived && retries < MAX_RETRIES) {
            {
                std::lock_guard<std::mutex> lock(g_udpMutex);
                int sentBytes = sendto(udpSocket, reinterpret_cast<const char*>(packet.data()),
                    static_cast<int>(packet.size()), 0,
                    reinterpret_cast<const sockaddr*>(&clientAddr), sizeof(clientAddr));
                if (sentBytes == SOCKET_ERROR) {
                    Log("ERROR", "sendto() failed for session " + std::to_string(sessionId));
                    // Optionally, you can break or retry.
                }
                // Wait for ACK.
                char ackBuffer[9] = { 0 };
                int ackBytes = recvfrom(udpSocket, ackBuffer, sizeof(ackBuffer), 0, nullptr, nullptr);
                if (ackBytes == 9) {
                    // ACK format: 1 byte flag, 4 bytes ACK number, 4 bytes sequence number.
                    uint8_t flag = ackBuffer[0];
                    uint32_t netAckNumber;
                    memcpy(&netAckNumber, ackBuffer + 1, 4);
                    uint32_t ackNumber = ntohl(netAckNumber);
                    if ((flag & 0x1) && (ackNumber == offset)) {
                        ackReceived = true;
                    }
                    else {
                        Log("WARN", "Incorrect ACK received. Expected offset " + std::to_string(offset) +
                            " but got " + std::to_string(ackNumber));
                    }
                }
                else {
                    Log("WARN", "No ACK received for session " + std::to_string(sessionId) +
                        " at offset " + std::to_string(offset));
                }
            }
            if (!ackReceived) {
                ++retries;
            }
        }
        if (!ackReceived) {
            Log("ERROR", "Max retries reached for session " + std::to_string(sessionId) +
                " at offset " + std::to_string(offset));
            break; // Abort file transfer.
        }
        offset += static_cast<uint32_t>(bytesRead);
    }
    file.close();
    Log("INFO", "Completed file transfer for session " + std::to_string(sessionId));
}

// --------------------- ClientInfo Structure -----------------------------
// For TCP control connections.
struct ClientInfo {
    SOCKET socket;
    std::string ip;    // Client IP in dotted-decimal format.
    uint16_t port;     // Client TCP port (host order).
};

// --------------------- Updated handleClient Function --------------------
// Processes commands from a connected client over TCP.
void handleClient(ClientInfo client)
{
    std::vector<uint8_t> messageBuffer;
    uint8_t recvBuffer[1024] = { 0 };

    while (true) {
        int bytesReceived = recv(client.socket, reinterpret_cast<char*>(recvBuffer),
            sizeof(recvBuffer), 0);
        if (bytesReceived <= 0) {
            Log("INFO", "Client disconnected: " + client.ip + ":" + std::to_string(client.port));
            break;
        }
        // Append received data.
        messageBuffer.insert(messageBuffer.end(), recvBuffer, recvBuffer + bytesReceived);

        // Process complete messages.
        while (!messageBuffer.empty()) {
            uint8_t command = messageBuffer[0];

            // CMD_REQ_QUIT (/q): Quit command.
            if (command == CMD_REQ_QUIT) {
                goto CLEANUP;
            }
            // CMD_REQ_LISTFILES (/l): Request for file list.
            else if (command == CMD_REQ_LISTFILES) {
                std::vector<uint8_t> listMsg;
                listMsg.push_back(CMD_RSP_LISTFILES);

                // Get file list from the designated directory.
                std::vector<std::string> files = getFileList(g_fileDirectory);
                uint16_t numFiles = static_cast<uint16_t>(files.size());
                uint16_t netNumFiles = htons(numFiles);
                listMsg.insert(listMsg.end(), reinterpret_cast<uint8_t*>(&netNumFiles),
                    reinterpret_cast<uint8_t*>(&netNumFiles) + sizeof(netNumFiles));

                // Compute total length of file list data.
                uint32_t totalListLen = 0;
                for (const auto& file : files)
                    totalListLen += 4 + static_cast<uint32_t>(file.size());
                uint32_t netTotalListLen = htonl(totalListLen);
                listMsg.insert(listMsg.end(), reinterpret_cast<uint8_t*>(&netTotalListLen),
                    reinterpret_cast<uint8_t*>(&netTotalListLen) + 4);

                // Append each file: 4 bytes filename length and filename string.
                for (const auto& file : files) {
                    uint32_t fileLen = static_cast<uint32_t>(file.size());
                    uint32_t netFileLen = htonl(fileLen);
                    listMsg.insert(listMsg.end(), reinterpret_cast<uint8_t*>(&netFileLen),
                        reinterpret_cast<uint8_t*>(&netFileLen) + 4);
                    listMsg.insert(listMsg.end(), file.begin(), file.end());
                }
                int sent = send(client.socket, reinterpret_cast<const char*>(listMsg.data()),
                    static_cast<int>(listMsg.size()), 0);
                if (sent == SOCKET_ERROR) {
                    Log("ERROR", "Failed to send file list to client " + client.ip);
                }
                messageBuffer.erase(messageBuffer.begin());
            }
            // CMD_REQ_DOWNLOAD (/d): Request to download a file.
            else if (command == CMD_REQ_DOWNLOAD) {
                const size_t headerSize = 1 + 4 + 2 + 4;
                if (messageBuffer.size() < headerSize)
                    break; // Wait for complete header.

                // Extract client UDP IP.
                uint32_t netClientUDPIP;
                memcpy(&netClientUDPIP, &messageBuffer[1], 4);
                in_addr clientUDPAddr;
                clientUDPAddr.s_addr = netClientUDPIP;
                char clientUDPIPStr[INET_ADDRSTRLEN] = { 0 };
                inet_ntop(AF_INET, &clientUDPAddr, clientUDPIPStr, INET_ADDRSTRLEN);

                // Extract client UDP port.
                uint16_t netClientUDPPort;
                memcpy(&netClientUDPPort, &messageBuffer[5], 2);
                uint16_t clientUDPPort = ntohs(netClientUDPPort);

                // Extract filename length.
                uint32_t netFilenameLen;
                memcpy(&netFilenameLen, &messageBuffer[7], 4);
                uint32_t filenameLen = ntohl(netFilenameLen);

                if (messageBuffer.size() < headerSize + filenameLen)
                    break;

                std::string filename(reinterpret_cast<char*>(&messageBuffer[headerSize]), filenameLen);
                messageBuffer.erase(messageBuffer.begin(), messageBuffer.begin() + headerSize + filenameLen);

                std::string fullPath = g_fileDirectory + "\\" + filename;
                uint32_t fileSize = 0;
                if (!fileExistsAndSize(fullPath, fileSize)) {
                    uint8_t errorMsg = CMD_DOWNLOAD_ERROR;
                    send(client.socket, reinterpret_cast<const char*>(&errorMsg), 1, 0);
                    Log("WARN", "File not found: " + fullPath);
                    continue;
                }

                uint32_t sessionId = g_sessionCounter.fetch_add(1);

                // Build RSP_DOWNLOAD message:
                //   1 byte command,
                //   4 bytes: server IP,
                //   2 bytes: server UDP port,
                //   4 bytes: session ID,
                //   4 bytes: file length.
                std::vector<uint8_t> rspMsg;
                rspMsg.push_back(CMD_RSP_DOWNLOAD);
                in_addr serverAddr;
                inet_pton(AF_INET, g_serverIP.c_str(), &serverAddr);
                uint8_t* ipBytes = reinterpret_cast<uint8_t*>(&serverAddr.s_addr);
                rspMsg.insert(rspMsg.end(), ipBytes, ipBytes + 4);
                uint16_t netServerUDPPort = htons(g_serverUDPPort);
                rspMsg.push_back((netServerUDPPort >> 8) & 0xff);
                rspMsg.push_back(netServerUDPPort & 0xff);
                uint32_t netSessionId = htonl(sessionId);
                rspMsg.insert(rspMsg.end(), reinterpret_cast<uint8_t*>(&netSessionId),
                    reinterpret_cast<uint8_t*>(&netSessionId) + 4);
                uint32_t netFileSize = htonl(fileSize);
                rspMsg.insert(rspMsg.end(), reinterpret_cast<uint8_t*>(&netFileSize),
                    reinterpret_cast<uint8_t*>(&netFileSize) + 4);
                send(client.socket, reinterpret_cast<const char*>(rspMsg.data()),
                    static_cast<int>(rspMsg.size()), 0);

                Log("INFO", "Initiating file transfer: " + filename +
                    " (" + std::to_string(fileSize) + " bytes) for session " + std::to_string(sessionId));

                sockaddr_in clientUDPStruct = {};
                clientUDPStruct.sin_family = AF_INET;
                inet_pton(AF_INET, clientUDPIPStr, &clientUDPStruct.sin_addr);
                clientUDPStruct.sin_port = htons(clientUDPPort);

                std::thread(sendFileViaUDP, g_udpSocket.get(), clientUDPStruct, fullPath, sessionId, fileSize).detach();
            }
            else {
                // Unknown command; discard one byte.
                messageBuffer.erase(messageBuffer.begin());
            }
        }
    }
CLEANUP:
    closesocket(client.socket);
    Log("INFO", "Closed TCP connection with client " + client.ip + ":" + std::to_string(client.port));
}

// --------------------- TaskQueue Processing Function --------------------
bool processClient(ClientInfo client) {
    handleClient(client);
    return true; // Continue processing tasks.
}

auto onDisconnect = []() {}; // (Optional) Disconnection callback.

// --------------------- Main Function ------------------------------------
int main()
{
    // Initialize Winsock.
    WSADATA wsaData;
    int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorCode != 0) {
        Log("ERROR", "WSAStartup() failed: " + std::to_string(errorCode));
        return errorCode;
    }

    // Prompt for Server Parameters.
    std::string tcpPortStr, udpPortStr;
    std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, tcpPortStr);
    std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, udpPortStr);
    g_serverUDPPort = static_cast<uint16_t>(atoi(udpPortStr.c_str()));
    std::cout << "Path (file directory): ";
    std::getline(std::cin, g_fileDirectory);

    // Create UDP Socket for File Transfers.
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSock == INVALID_SOCKET) {
        Log("ERROR", "Failed to create UDP socket.");
        WSACleanup();
        return 1;
    }
    g_udpSocket.reset(udpSock);
    sockaddr_in udpAddr = {};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    udpAddr.sin_port = htons(g_serverUDPPort);
    if (bind(g_udpSocket.get(), reinterpret_cast<sockaddr*>(&udpAddr), sizeof(udpAddr)) == SOCKET_ERROR) {
        Log("ERROR", "Failed to bind UDP socket.");
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 1;
    }

    // Set Up TCP Listening Socket.
    addrinfo hints{}, * info = nullptr;
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4.
    hints.ai_socktype = SOCK_STREAM; // TCP.
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    errorCode = getaddrinfo(NULL, tcpPortStr.c_str(), &hints, &info);
    if (errorCode != 0 || info == nullptr) {
        Log("ERROR", "getaddrinfo() failed.");
        closesocket(g_udpSocket.get());
        WSACleanup();
        return errorCode;
    }

    SOCKET listenerSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (listenerSocket == INVALID_SOCKET) {
        Log("ERROR", "socket() failed.");
        freeaddrinfo(info);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 1;
    }

    errorCode = bind(listenerSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (errorCode != 0) {
        Log("ERROR", "bind() failed for TCP listener.");
        closesocket(listenerSocket);
        freeaddrinfo(info);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 2;
    }
    freeaddrinfo(info);

    errorCode = listen(listenerSocket, SOMAXCONN);
    if (errorCode != 0) {
        Log("ERROR", "listen() failed.");
        closesocket(listenerSocket);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 3;
    }

    // Determine and Display Server IP.
    char localHostName[256] = { 0 };
    if (gethostname(localHostName, sizeof(localHostName)) == SOCKET_ERROR) {
        Log("ERROR", "gethostname() failed.");
        closesocket(listenerSocket);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 1;
    }
    addrinfo hints2 = {};
    hints2.ai_family = AF_INET;
    hints2.ai_socktype = SOCK_STREAM;
    hints2.ai_protocol = IPPROTO_TCP;
    addrinfo* localInfo = nullptr;
    errorCode = getaddrinfo(localHostName, tcpPortStr.c_str(), &hints2, &localInfo);
    if (errorCode != 0 || localInfo == nullptr) {
        Log("ERROR", "getaddrinfo() for local host failed.");
        closesocket(listenerSocket);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return errorCode;
    }
    sockaddr_in* localAddr = reinterpret_cast<sockaddr_in*>(localInfo->ai_addr);
    char localIP[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &(localAddr->sin_addr), localIP, INET_ADDRSTRLEN);
    g_serverIP = localIP;
    std::cout << std::endl;
    std::cout << "Server IP Address: " << g_serverIP << std::endl;
    std::cout << "Server TCP Port Number: " << tcpPortStr << std::endl;
    std::cout << "Server UDP Port Number: " << g_serverUDPPort << std::endl;
    std::cout << "File Directory: " << g_fileDirectory << std::endl;
    freeaddrinfo(localInfo);

    // Create TaskQueue for TCP Client Connections.
    const int NUM_OF_THREADS = 10;
    const int NUM_OF_TASK_SLOTS = 10;
    TaskQueue<ClientInfo, decltype(processClient), decltype(onDisconnect)>
        clientQueue(NUM_OF_THREADS, NUM_OF_TASK_SLOTS, processClient, onDisconnect);

    // Main Accept Loop.
    while (true) {
        sockaddr clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenerSocket, &clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            Log("ERROR", "accept() failed.");
            continue;
        }
        char clientIP[INET_ADDRSTRLEN] = { 0 };
        char clientPortStr[NI_MAXSERV] = { 0 };
        getnameinfo(&clientAddr, clientAddrLen, clientIP, sizeof(clientIP),
            clientPortStr, sizeof(clientPortStr),
            NI_NUMERICHOST | NI_NUMERICSERV);
        uint16_t clientPort = static_cast<uint16_t>(atoi(clientPortStr));
        Log("INFO", "Client connected: " + std::string(clientIP) + ":" + std::to_string(clientPort));

        ClientInfo clientInfo;
        clientInfo.socket = clientSocket;
        clientInfo.ip = clientIP;
        clientInfo.port = clientPort;

        clientQueue.produce(clientInfo);
    }

    // Cleanup (unreachable).
    shutdown(listenerSocket, SD_BOTH);
    closesocket(listenerSocket);
    closesocket(g_udpSocket.get());
    WSACleanup();
    return 0;
}
#endif
