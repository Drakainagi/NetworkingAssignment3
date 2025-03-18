/* Start Header
*****************************************************************/
/*!
  \file   ftpserver.cpp
  \authors weijie.soh (Soh Wei Jie)
           lee.v (Victor Lee)
  \par    DigiPen Institute of Technology
  \date   16 March 2025
  \brief
         This file implements a robust multi-threaded FTP server using Winsock and a thread pool.
         It supports listing available files (/l), file download requests (/d <client_ip:udpPort> <filename>),
         and quitting (/q) over TCP. File transfers occur via UDP using a selective repeat protocol.
         Important parameters (e.g. CHUNK_SIZE, ACK_TIMEOUT, MAX_RETRIES, WINDOW_SIZE) are configurable via a configuration file.
         A console control handler allows graceful server shutdown.

         Note: More advanced improvements (such as asynchronous I/O or zero-copy techniques) are possible,
         but selective repeat is chosen here for its balance between performance and ease of integration.

         Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header
*******************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#endif

#if 1
// Enable compilation

#if 1
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <unordered_map>

//#include <optional>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <algorithm> // For std::sort
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

// --------------------- Global Configurable Parameters --------------------------
// These parameters are now global variables that can be set via a configuration file.
int CHUNK_SIZE = 32768;     // UDP chunk size (bytes). //Higher means faster download speed
int ACK_TIMEOUT = 200;    // Timeout in ms for receiving an ACK.
int MAX_RETRIES = 15;       // Maximum number of retransmissions per window.
int WINDOW_SIZE = 64;       // Number of packets allowed in the pipeline window.

// --------------------- Configuration File Loader -----------------------------
// Reads key=value pairs from a configuration file ("server_config.txt").
// Expected keys: CHUNK_SIZE, ACK_TIMEOUT, MAX_RETRIES, WINDOW_SIZE
void loadConfiguration(const std::string& configFilePath)
{
    std::ifstream configFile(configFilePath);
    if (!configFile.is_open())
    {
        std::cerr << "[WARN] Could not open config file: " << configFilePath << ". Using default parameters." << std::endl;
        return;
    }
    std::string line;
    while (std::getline(configFile, line))
    {
        // Remove comments and whitespace.
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream iss(line);
        std::string key, equalSign;
        int value;
        if (iss >> key >> equalSign >> value && equalSign == "=")
        {
            if (key == "CHUNK_SIZE")
                CHUNK_SIZE = value;
            else if (key == "ACK_TIMEOUT")
                ACK_TIMEOUT = value;
            else if (key == "MAX_RETRIES")
                MAX_RETRIES = value;
            else if (key == "WINDOW_SIZE")
                WINDOW_SIZE = value;
        }
    }
    configFile.close();
    std::cout << "[INFO] Configuration loaded: CHUNK_SIZE=" << CHUNK_SIZE
        << ", ACK_TIMEOUT=" << ACK_TIMEOUT
        << ", MAX_RETRIES=" << MAX_RETRIES
        << ", WINDOW_SIZE=" << WINDOW_SIZE << std::endl;
}

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
class SocketRAII
{
public:
    SocketRAII(SOCKET s = INVALID_SOCKET) : sock(s) {}
    ~SocketRAII()
    {
        if (sock != INVALID_SOCKET)
        {
            closesocket(sock);
        }
    }
    SOCKET get() const { return sock; }
    void reset(SOCKET s = INVALID_SOCKET)
    {
        if (sock != INVALID_SOCKET)
        {
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


// Structure to store session details
struct SessionInfo {
    uint32_t sessionId;
    sockaddr_in clientAddr; // Stores the client's UDP IP & Port
    std::vector<bool> acked; // Track received ACKs
    uint32_t totalPackets;
};

// Global session storage
std::unordered_map<uint32_t, SessionInfo> g_sessions;
std::mutex g_sessionMutex;
std::mutex sessionMutex;
std::unordered_map<uint32_t, bool> activeSessions;

std::atomic<uint32_t> g_sessionCounter{ 1 }; // Global session ID counter.
std::mutex g_udpMutex;                        // Mutex to serialize UDP send/receive.

std::atomic<bool> serverRunning(true);       // Global flag for server run state.

// --------------------- Console Control Handler ---------------------------
// This handler will catch CTRL+C or console close events and signal the server to shut down.
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        serverRunning = false;
        Log("INFO", "Shutdown signal received. Stopping server...");
        return TRUE;
    }
    return FALSE;
}

// --------------------- Helper Functions -------------------------------

// Function: getFileList
// Scans the provided directory and returns a sorted vector of filenames (excluding directories).
std::vector<std::string> getFileList(const std::string& directory)
{
    std::vector<std::string> fileList;
    std::string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            // Exclude directories.
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                fileList.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    else
    {
        Log("ERROR", "FindFirstFileA failed for directory: " + directory);
    }
    // Sort the file list to guarantee consistency among clients.
    std::sort(fileList.begin(), fileList.end());
    return fileList;
}

// Function: fileExistsAndSize
// Checks if a file exists and retrieves its size.
bool fileExistsAndSize(const std::string& filePath, uint32_t& fileSize)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    fileSize = static_cast<uint32_t>(file.tellg());
    return true;
}

// --------------------- UDP File Transfer with Selective Repeat -----------------------------
// This function sends the file specified by filePath to the client (clientAddr) using the shared UDP socket.
// It implements a selective repeat protocol: each packet is sent with a sequence number, and only missing
// packets (those not acknowledged) within the current window are retransmitted. ACK messages from the client
// are expected to be 5 bytes long: 1 byte flag (LSB indicates ACK) and 4 bytes (network order) representing the sequence number.
void sendFileViaUDP_SelectiveRepeat(SOCKET udpSocket, const sockaddr_in& clientAddr, const std::string& filePath,
    uint32_t sessionId, uint32_t fileSize)
{

    uint32_t totalPackets = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        g_sessions[sessionId] = { sessionId, clientAddr, std::vector<bool>(totalPackets, false), totalPackets };

    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        activeSessions[sessionId] = true;  // ✅ Mark session as active

    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        Log("ERROR", "Failed to open file for UDP selective repeat transfer: " + filePath);
        return;
    }

    totalPackets = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    std::vector<bool> acked(totalPackets, false);
    uint32_t base_seq = 0;
    int retransmitCount = 0;

    int timeout = ACK_TIMEOUT;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    while (base_seq < totalPackets && serverRunning)
    {
        uint32_t windowEnd = std::min(base_seq + static_cast<uint32_t>(WINDOW_SIZE), totalPackets);
        for (uint32_t seq = base_seq; seq < windowEnd; seq++)
        {
            if (!acked[seq])
            {
                // Debug TEST: Introduce artificial packet loss (simulating Ethernet break)
                if (rand() % 100 < 10) {  // 10% chance to drop a packet
                    Log("TEST", "Artificially dropping packet " + std::to_string(seq) + " to simulate network loss.");
                    continue;  // Simulate a lost packet by not sending it
                }

                uint32_t offset = seq * CHUNK_SIZE;
                uint32_t remainingBytes = fileSize - offset;
                uint32_t dataSize = std::min(remainingBytes, static_cast<uint32_t>(CHUNK_SIZE));

                file.seekg(offset);
                std::vector<char> buffer(dataSize, 0);
                file.read(buffer.data(), dataSize);
                std::streamsize bytesRead = file.gcount();
                if (bytesRead <= 0) break;

                uint32_t netSessionId = htonl(sessionId);
                uint32_t netFileSize = htonl(fileSize);
                uint32_t netOffset = htonl(offset);
                uint32_t netDataLength = htonl(static_cast<uint32_t>(bytesRead));
                std::vector<uint8_t> packet;
                packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netSessionId), reinterpret_cast<uint8_t*>(&netSessionId) + 4);
                packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netFileSize), reinterpret_cast<uint8_t*>(&netFileSize) + 4);
                packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netOffset), reinterpret_cast<uint8_t*>(&netOffset) + 4);
                packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&netDataLength), reinterpret_cast<uint8_t*>(&netDataLength) + 4);
                packet.insert(packet.end(), buffer.begin(), buffer.begin() + bytesRead);

                {
                    std::lock_guard<std::mutex> lock(g_udpMutex);
                    int sentBytes = sendto(udpSocket, reinterpret_cast<const char*>(packet.data()),
                        static_cast<int>(packet.size()), 0,
                        reinterpret_cast<const sockaddr*>(&clientAddr), sizeof(clientAddr));
                    if (sentBytes == SOCKET_ERROR) {
                        Log("ERROR", "sendto() failed for session " + std::to_string(sessionId) +
                            " for packet " + std::to_string(seq));
                    }
                }
                Log("DEBUG", "Sent packet " + std::to_string(seq) +
                    " for session " + std::to_string(sessionId));
            }
        }

        bool gotAck = false;
        while (true)
        {
            uint8_t ackBuffer[5] = { 0 };
            sockaddr_in senderAddr;
            int senderAddrLen = sizeof(senderAddr);
            int ackBytes = recvfrom(udpSocket, reinterpret_cast<char*>(ackBuffer), sizeof(ackBuffer), 0,
                reinterpret_cast<sockaddr*>(&senderAddr), &senderAddrLen);

            if (ackBytes == 5)
            {
                std::lock_guard<std::mutex> lock(g_sessionMutex);

                // Retrieve session information
                auto it = g_sessions.find(sessionId);
                if (it == g_sessions.end()) {
                    Log("ERROR", "Received ACK for unknown session " + std::to_string(sessionId));
                    continue;
                }

                SessionInfo& session = it->second;

                // Ensure buffer size is sufficient for IPv4 (16 bytes)
                char clientIpStr[INET_ADDRSTRLEN];
                char senderIpStr[INET_ADDRSTRLEN];

                // Convert client and sender IPs to readable string format
                inet_ntop(AF_INET, &session.clientAddr.sin_addr, clientIpStr, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, INET_ADDRSTRLEN);

                // Ensure ACK is from the correct client
                if (senderAddr.sin_addr.s_addr != session.clientAddr.sin_addr.s_addr ||
                    senderAddr.sin_port != session.clientAddr.sin_port)
                {
                    Log("WARN", "Ignoring ACK from wrong client. Expected: " +
                        std::string(clientIpStr) + ":" + std::to_string(ntohs(session.clientAddr.sin_port)) +
                        " but got " + std::string(senderIpStr) + ":" + std::to_string(ntohs(senderAddr.sin_port)));
                    continue;
                }


                uint8_t flag = ackBuffer[0];
                uint32_t netSeq;
                memcpy(&netSeq, ackBuffer + 1, 4);
                uint32_t ackSeq = ntohl(netSeq);

                if (ackSeq < fileSize && ackSeq % CHUNK_SIZE == 0)
                {
                    uint32_t seqIndex = ackSeq / CHUNK_SIZE;

                    if (seqIndex < session.totalPackets && !session.acked[seqIndex])
                    {
                        session.acked[seqIndex] = true;
                        Log("DEBUG", "Received valid ACK for packet " + std::to_string(seqIndex) +
                            " from client " + std::to_string(sessionId));

                    if (seqIndex < totalPackets && !acked[seqIndex]) {
                        acked[seqIndex] = true;
                        Log("DEBUG", "Received valid ACK for packet " + std::to_string(seqIndex));

                        gotAck = true;
                    }
                }
                else
                {
                    Log("WARN", "Received invalid or duplicate ACK for packet " +
                        std::to_string(ackSeq) + " from client " + std::to_string(sessionId));
                }
            }
            else
            {
                break;
            }
        }

        if (!gotAck)
        {
            retransmitCount++;
            Log("WARN", "Timeout waiting for ACKs in session " + std::to_string(sessionId) +
                ". Retransmitting window starting at packet " + std::to_string(base_seq));

            // 🔥 TEST: Introduce artificial retransmission delay
            if (rand() % 100 < 5) {  // 5% chance to add delay before retransmitting
                Log("TEST", "Artificial delay before retransmitting packet " + std::to_string(base_seq));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 500ms delay
            }

            if (retransmitCount > MAX_RETRIES)
            {
                Log("ERROR", "Max retransmissions reached in session " + std::to_string(sessionId) +
                    ". Aborting transfer.");

                // ✅ Ensure session is removed on failure
                {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    activeSessions.erase(sessionId);
                }
                file.close();
                return;
            }
        }
        else
        {
            retransmitCount = 0;
        }


        //  Fix: Move base_seq forward

        while (base_seq < totalPackets && acked[base_seq]) {
            base_seq++;
        }
    }

    file.close();

    // ✅ Ensure session is removed after successful completion
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (activeSessions.count(sessionId)) {
            activeSessions.erase(sessionId);
            Log("INFO", "Successfully closed session " + std::to_string(sessionId));
        }
    }

    Log("INFO", "Completed selective repeat file transfer for session " + std::to_string(sessionId));
}


// --------------------- ClientInfo Structure -----------------------------
// For TCP control connections.
struct ClientInfo
{
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

    while (true)
    {
        int bytesReceived = recv(client.socket, reinterpret_cast<char*>(recvBuffer),
            sizeof(recvBuffer), 0);
        if (bytesReceived <= 0)
        {
            Log("INFO", "Client disconnected: " + client.ip + ":" + std::to_string(client.port));
            break;
        }
        // Append received data.
        messageBuffer.insert(messageBuffer.end(), recvBuffer, recvBuffer + bytesReceived);

        // Process complete messages.
        while (!messageBuffer.empty())
        {
            uint8_t command = messageBuffer[0];

            // CMD_REQ_QUIT (/q): Quit command.
            if (command == CMD_REQ_QUIT)
            {
                goto CLEANUP;
            }
            // CMD_REQ_LISTFILES (/l): Request for file list.
            else if (command == CMD_REQ_LISTFILES)
            {
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
                for (const auto& file : files)
                {
                    uint32_t fileLen = static_cast<uint32_t>(file.size());
                    uint32_t netFileLen = htonl(fileLen);
                    listMsg.insert(listMsg.end(), reinterpret_cast<uint8_t*>(&netFileLen),
                        reinterpret_cast<uint8_t*>(&netFileLen) + 4);
                    listMsg.insert(listMsg.end(), file.begin(), file.end());
                }
                int sent = send(client.socket, reinterpret_cast<const char*>(listMsg.data()),
                    static_cast<int>(listMsg.size()), 0);
                if (sent == SOCKET_ERROR)
                {
                    Log("ERROR", "Failed to send file list to client " + client.ip);
                }
                // Remove the command byte from the buffer.
                messageBuffer.erase(messageBuffer.begin());
            }
            // CMD_REQ_DOWNLOAD (/d): Request to download a file.
            else if (command == CMD_REQ_DOWNLOAD)
            {
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
                if (!fileExistsAndSize(fullPath, fileSize))
                {
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

                // Launch selective repeat file transfer in a separate thread.
                std::thread(sendFileViaUDP_SelectiveRepeat, g_udpSocket.get(), clientUDPStruct, fullPath, sessionId, fileSize).detach();
            }
            else
            {
                // Unknown command; discard one byte and continue.
                messageBuffer.erase(messageBuffer.begin());
            }
        }
    }
CLEANUP:
    closesocket(client.socket);
    Log("INFO", "Closed TCP connection with client " + client.ip + ":" + std::to_string(client.port));
}

// --------------------- TaskQueue Processing Function --------------------
bool processClient(ClientInfo client)
{
    handleClient(client);
    return true; // Continue processing tasks.
}

auto onDisconnect = []() {}; // (Optional) Disconnection callback.

// --------------------- Main Function ------------------------------------
int main()
{
    // Set the console control handler for graceful shutdown.
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        Log("ERROR", "Could not set control handler");
        return 1;
    }

    // Load configuration parameters from a file.
    loadConfiguration("server_config.txt");

    // Initialize Winsock.
    WSADATA wsaData;
    int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorCode != 0)
    {
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
    if (udpSock == INVALID_SOCKET)
    {
        Log("ERROR", "Failed to create UDP socket.");
        WSACleanup();
        return 1;
    }
    g_udpSocket.reset(udpSock);
    sockaddr_in udpAddr = {};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    udpAddr.sin_port = htons(g_serverUDPPort);
    if (bind(g_udpSocket.get(), reinterpret_cast<sockaddr*>(&udpAddr), sizeof(udpAddr)) == SOCKET_ERROR)
    {
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
    if (errorCode != 0 || info == nullptr)
    {
        Log("ERROR", "getaddrinfo() failed.");
        closesocket(g_udpSocket.get());
        WSACleanup();
        return errorCode;
    }

    SOCKET listenerSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (listenerSocket == INVALID_SOCKET)
    {
        Log("ERROR", "socket() failed.");
        freeaddrinfo(info);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 1;
    }

    errorCode = bind(listenerSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (errorCode != 0)
    {
        Log("ERROR", "bind() failed for TCP listener.");
        closesocket(listenerSocket);
        freeaddrinfo(info);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 2;
    }
    freeaddrinfo(info);

    errorCode = listen(listenerSocket, SOMAXCONN);
    if (errorCode != 0)
    {
        Log("ERROR", "listen() failed.");
        closesocket(listenerSocket);
        closesocket(g_udpSocket.get());
        WSACleanup();
        return 3;
    }

    // Determine and Display Server IP.
    char localHostName[256] = { 0 };
    if (gethostname(localHostName, sizeof(localHostName)) == SOCKET_ERROR)
    {
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
    if (errorCode != 0 || localInfo == nullptr)
    {
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
    while (serverRunning)
    {
        sockaddr clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenerSocket, &clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET)
        {
            if (!serverRunning) break; // Break if shutdown was signaled.
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

    // Begin Cleanup
    Log("INFO", "Server shutting down. Cleaning up resources...");
    shutdown(listenerSocket, SD_BOTH);
    closesocket(listenerSocket);
    closesocket(g_udpSocket.get());
    WSACleanup();
    // Optionally, if your TaskQueue supports graceful shutdown, call that here.

    return 0;
}

#endif

