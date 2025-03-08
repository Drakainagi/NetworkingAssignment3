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

         Robustness improvements include enhanced error checking, a RAII socket
         wrapper, consistent logging, and modularized code sections.

         THIS FILE IS STILL IN DEVELOPMENT AND MIGHT NOT WORK AS INTENDED

  Copyright (C) 2025 DigiPen Institute of Technology.
/* End Header
*********************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if 0
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
#include <chrono>
#include <iomanip>  // For std::put_time

// ---------------------- Configuration Constants --------------------------
constexpr int TCP_RECV_BUFFER_SIZE = 1024;
constexpr int UDP_BUFFER_SIZE = 1500;

// ---------------------- Command ID Definitions --------------------------
#define CMD_REQ_QUIT         0x1    // /q
#define CMD_REQ_DOWNLOAD     0x2    // /d
#define CMD_RSP_DOWNLOAD     0x3    // Server reply: download info (IP, UDP port, session, file length)
#define CMD_REQ_LISTFILES    0x4    // /l
#define CMD_RSP_LISTFILES    0x5    // Server reply: file list
#define CMD_CMDTEST          0x20
#define CMD_DOWNLOAD_ERROR   0x30

// ---------------------- RAII Socket Wrapper ------------------------------
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

// ---------------------- Simple Logger ------------------------------
void Log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm timeInfo;
    localtime_s(&timeInfo, &now_c);
    std::cout << "[" << level << "] "
        << std::put_time(&timeInfo, "%F %T")
        << " : " << message << std::endl;
}

// ---------------------- Global Variables -------------------------------
std::string g_downloadPath;  // Local storage path for downloaded files.
std::mutex g_sessionMutex;
std::map<uint32_t, std::string> g_downloadFilenames; // Map sessionID to filename pending.
std::mutex g_pendingMutex;
std::string g_pendingFilename;  // Temporary store for the filename requested via /d.

// Structure to manage file download session (for writing data to file).
struct FileDownloadSession {
    std::string filename;      // Full path for the downloaded file.
    std::ofstream file;        // Output file stream.
    uint32_t expectedFileLength; // Total file size in bytes.
    uint32_t receivedBytes;    // Count of bytes received so far.
};
std::mutex g_fileSessionMutex;
std::map<uint32_t, FileDownloadSession> g_sessions;

// ---------------------- Helper Functions -------------------------------

// Parse a destination string in the form "ip:port" into separate IP and port.
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
// Expected UDP packet header layout (all multi-byte values in network byte order):
//   - Session ID [4 bytes]
//   - File Length [4 bytes]
//   - File Offset [4 bytes]
//   - Data Length [4 bytes]
//   - File Data [variable length]
// After writing data to the correct file offset, the client sends an ACK.
void udpReceiverThread(SOCKET udpSocket) {
    char buffer[UDP_BUFFER_SIZE];
    sockaddr_in senderAddr;
    int senderAddrLen = sizeof(senderAddr);

    while (true) {
        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &senderAddrLen);
        if (bytesReceived <= 0) {
            Log("WARN", "UDP recvfrom() returned error or zero bytes.");
            continue; // Error or no data received.
        }
        // Check that packet is at least 16 bytes for header.
        if (bytesReceived < 16) {
            Log("WARN", "Received UDP packet too short.");
            continue;
        }
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

        if (bytesReceived < 16 + dataLength) {
            Log("WARN", "Incomplete UDP packet for session " + std::to_string(sessionId));
            continue;
        }
        char* fileData = buffer + 16;

        {
            std::lock_guard<std::mutex> lock(g_fileSessionMutex);
            auto it = g_sessions.find(sessionId);
            if (it == g_sessions.end()) {
                Log("INFO", "No active session for sessionID " + std::to_string(sessionId));
                continue;
            }
            FileDownloadSession& session = it->second;
            session.file.seekp(fileOffset, std::ios::beg);
            session.file.write(fileData, dataLength);
            session.receivedBytes += dataLength;
            if (session.receivedBytes >= session.expectedFileLength) {
                session.file.close();
                Log("INFO", "Download complete: " + session.filename +
                    " (" + std::to_string(session.expectedFileLength) + " bytes)");
                g_sessions.erase(it);
            }
        }

        // Send ACK: 1 byte flag and duplicate 4 bytes of fileOffset.
        char ackPacket[9];
        ackPacket[0] = 0x1; // ACK flag.
        uint32_t netAck = htonl(fileOffset);
        memcpy(ackPacket + 1, &netAck, 4);
        memcpy(ackPacket + 5, &netAck, 4);
        sendto(udpSocket, ackPacket, 9, 0,
            reinterpret_cast<sockaddr*>(&senderAddr), senderAddrLen);
    }
}

// ---------------------- TCP Receiver Thread ----------------------------
// This function continuously receives TCP messages from the server and processes them.
void receiveFromServer(SOCKET clientSocket, SOCKET udpSocket) {
    std::vector<uint8_t> recvBuffer;
    uint8_t tempBuffer[TCP_RECV_BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(tempBuffer), sizeof(tempBuffer), 0);
        if (bytesReceived == 0) {
            Log("INFO", "Server closed the connection.");
            break;
        }
        if (bytesReceived < 0) {
            Log("ERROR", "Error receiving data from server.");
            break;
        }
        recvBuffer.insert(recvBuffer.end(), tempBuffer, tempBuffer + bytesReceived);
        while (!recvBuffer.empty()) {
            uint8_t command = recvBuffer[0];
            if (command == CMD_RSP_LISTFILES) {
                // File list message: 1 byte command, 2 bytes num files, 4 bytes total length, then pairs of [4 bytes filename length, filename].
                if (recvBuffer.size() < 7)
                    break; // Not enough data.
                uint16_t netNumFiles;
                memcpy(&netNumFiles, &recvBuffer[1], 2);
                uint16_t numFiles = ntohs(netNumFiles);

                uint32_t netListLength;
                memcpy(&netListLength, &recvBuffer[3], 4);
                uint32_t listLength = ntohl(netListLength);

                if (recvBuffer.size() < 7 + listLength)
                    break;

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
            else if (command == CMD_RSP_DOWNLOAD) {
                // Download response: 1 byte command, 4 bytes server IP, 2 bytes server UDP port, 4 bytes session ID, 4 bytes file length.
                if (recvBuffer.size() < 15)
                    break;
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

                // Retrieve pending filename.
                std::string filename;
                {
                    std::lock_guard<std::mutex> lock(g_pendingMutex);
                    filename = g_pendingFilename;
                    g_pendingFilename = "";
                }
                if (filename.empty())
                    filename = "downloaded_file";

                std::string fullPath = g_downloadPath + "\\" + filename;
                FileDownloadSession session;
                session.filename = fullPath;
                session.expectedFileLength = fileLength;
                session.receivedBytes = 0;
                session.file.open(fullPath, std::ios::binary | std::ios::out);
                if (!session.file.is_open()) {
                    Log("ERROR", "Unable to open file for download: " + fullPath);
                }
                else {
                    Log("INFO", "Saving file to: " + fullPath);
                    std::lock_guard<std::mutex> lock(g_fileSessionMutex);
                    g_sessions[sessionID] = std::move(session);
                }
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + 15);
            }
            else if (command == CMD_DOWNLOAD_ERROR) {
                Log("ERROR", "Download error received from server.");
                recvBuffer.erase(recvBuffer.begin());
            }
            else if (command == CMD_REQ_QUIT) {
                Log("INFO", "Server requested quit.");
                recvBuffer.erase(recvBuffer.begin());
                break;
            }
            else {
                // Unknown command; remove one byte.
                recvBuffer.erase(recvBuffer.begin());
            }
        }
    }
}

// ---------------------- User Input Handler -----------------------------
// Reads user commands from the console and sends appropriate TCP messages.
void handleUserInput(SOCKET clientSocket) {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input.empty())
            continue;
        if (input == "/q") {
            uint8_t msg = CMD_REQ_QUIT;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
            break;
        }
        else if (input == "/l") {
            uint8_t msg = CMD_REQ_LISTFILES;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
        }
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
            {
                std::lock_guard<std::mutex> lock(g_pendingMutex);
                g_pendingFilename = filename;
            }
            send(clientSocket, reinterpret_cast<const char*>(buffer.data()),
                static_cast<int>(buffer.size()), 0);
        }
        else {
            std::cerr << "Unknown command. Supported commands: /q, /l, /d" << std::endl;
        }
    }
}

// ---------------------- Main Function ----------------------------------
int main() {
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

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup() failed." << std::endl;
        return 1;
    }

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

    std::thread udpThread(udpReceiverThread, udpSocket);
    std::thread tcpReceiverThread(receiveFromServer, clientSocket, udpSocket);
    handleUserInput(clientSocket);

    closesocket(clientSocket);
    closesocket(udpSocket);
    tcpReceiverThread.join();
    udpThread.join();
    WSACleanup();
    return 0;
}
#endif
