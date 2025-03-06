/* Start Header
*****************************************************************/
/*!
\file echoclient.cpp
\author weijie.soh
\par CSD2161/CSD2160/CS260/MET3302 Assignment 2
\date 21 February 2025
\brief
     This file implements a multi-threaded echo client using Winsock and std::thread.
     The client supports both Script Mode and Manual Mode. It sends commands to the server,
     such as REQ_ECHO, REQ_QUIT, and REQ_LISTUSERS, based on user input, and receives responses
     concurrently from the server.
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
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <cstring>
#include <cstdlib>

// Winsock version and maximum string length
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         9000

// Command definitions
#define CMD_UNKNOWN         0x0
#define CMD_REQ_QUIT        0x1
#define CMD_REQ_ECHO        0x2
#define CMD_RSP_ECHO        0x3
#define CMD_REQ_LISTUSERS   0x4
#define CMD_RSP_LISTUSERS   0x5
#define CMD_CMDTEST         0x20
#define CMD_ECHO_ERROR      0x30

// ---------------------------------------------------------------------------
// Helper: Convert a hex string (without spaces) into a vector of bytes.
// Used for the /t command.
std::vector<uint8_t> hexStringToBytes(const std::string& hexString) 
{
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hexString.length(); i += 2) {
        std::string byteString = hexString.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// ---------------------------------------------------------------------------
// Thread function: Continuously receives data from the server and processes messages.
void receiveFromServer(SOCKET clientSocket) 
{
    std::vector<uint8_t> recvBuffer;
    uint8_t tempBuffer[1024];

    while (true) 
    {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(tempBuffer), sizeof(tempBuffer), 0);
        // Check for graceful shutdown.
        if (bytesReceived == 0)
        {
            std::cerr << "Graceful shutdown." << std::endl;
            break;
        }
        if (bytesReceived < 0)
        {
            std::cerr << "disconnection..." << std::endl;
            break;
        }

        // Append received bytes to our buffer.
        recvBuffer.insert(recvBuffer.end(), tempBuffer, tempBuffer + bytesReceived);

        // Process complete messages from recvBuffer.
        while (!recvBuffer.empty()) 
        {
            uint8_t command = recvBuffer[0];
            // Process echo response messages.
            if (command == CMD_RSP_ECHO) 
            {
                // Check that the header is complete (11 bytes).
                if (recvBuffer.size() < 11)
                    break;  // Wait for more data.

                uint8_t ipBytes[4];
                memcpy(ipBytes, &recvBuffer[1], 4);
                char ipStr[INET_ADDRSTRLEN] = { 0 };
                in_addr addr;
                memcpy(&addr, ipBytes, 4);
                inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
                uint16_t netPort;
                memcpy(&netPort, &recvBuffer[5], 2);
                uint16_t sourcePort = ntohs(netPort);
                uint32_t netTextLen;
                memcpy(&netTextLen, &recvBuffer[7], 4);
                uint32_t textLen = ntohl(netTextLen);
                if (recvBuffer.size() < 11 + textLen)
                    break;  // Incomplete message.

                // Extract the text payload.
                std::string text(reinterpret_cast<char*>(&recvBuffer[11]), textLen);

                std::cout << "==========RECV START==========" << std::endl;
                std::cout << ipStr << ":" << sourcePort << std::endl;
                std::cout << text << std::endl;
                std::cout << "==========RECV END==========" << std::endl;

                // Remove the processed message from the buffer.
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + 11 + textLen);
            }
            // Process echo request message (destination client receiving REQ_ECHO).
            else if (command == CMD_REQ_ECHO)
            {
                // Check that the header is complete (11 bytes).
                if (recvBuffer.size() < 11)
                    break;  // Wait for more data.

                // Extract the echo requester's IP (4 bytes).
                uint8_t reqIp[4];
                memcpy(reqIp, &recvBuffer[1], 4);
                char reqIpStr[INET_ADDRSTRLEN] = { 0 };
                in_addr reqAddr;
                memcpy(&reqAddr, reqIp, 4);
                inet_ntop(AF_INET, &reqAddr, reqIpStr, INET_ADDRSTRLEN);

                // Extract the echo requester's port (2 bytes).
                uint16_t reqNetPort;
                memcpy(&reqNetPort, &recvBuffer[5], 2);
                uint16_t reqPort = ntohs(reqNetPort);

                // Extract text length (4 bytes).
                uint32_t netTextLen;
                memcpy(&netTextLen, &recvBuffer[7], 4);
                uint32_t textLen = ntohl(netTextLen);

                if (recvBuffer.size() < 11 + textLen)
                    break;  // Incomplete message.

                // Extract the text payload.
                std::string text(reinterpret_cast<char*>(&recvBuffer[11]), textLen);

                // Display the received echo request.
                std::cout << "==========RECV START==========" << std::endl;
                std::cout << reqIpStr << ":" << reqPort << std::endl;
                std::cout << text << std::endl;
                std::cout << "==========RECV END==========" << std::endl;

                // Remove the processed REQ_ECHO message from the buffer.
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + 11 + textLen);

                // Construct the RSP_ECHO reply.
                // The reply follows the same format, with:
                // 1 byte command, 4 bytes destination IP (echo requester),
                // 2 bytes destination port (echo requester), 4 bytes text length, and text.
                std::vector<uint8_t> rspBuffer;
                rspBuffer.push_back(CMD_RSP_ECHO);
                // Destination: echo requester's IP.
                rspBuffer.insert(rspBuffer.end(), reqIp, reqIp + 4);
                // Destination: echo requester's port.
                uint16_t netReqPort = htons(reqPort);
                uint8_t portArr[2];
                memcpy(portArr, &netReqPort, 2);
                rspBuffer.insert(rspBuffer.end(), portArr, portArr + 2);
                // Text length.
                uint32_t netTextLenRsp = htonl(textLen);
                uint8_t textLenArr[4];
                memcpy(textLenArr, &netTextLenRsp, 4);
                rspBuffer.insert(rspBuffer.end(), textLenArr, textLenArr + 4);
                // Text payload.
                rspBuffer.insert(rspBuffer.end(), text.begin(), text.end());

                // Send the RSP_ECHO message back to the server.
                send(clientSocket, reinterpret_cast<const char*>(rspBuffer.data()), static_cast<int>(rspBuffer.size()), 0);
            }
            // Process list users response.
            else if (command == CMD_RSP_LISTUSERS) 
            {
                // Format: 1 byte command, 2 bytes number of users,
                // then for each user: 4 bytes IP, 2 bytes port.
                if (recvBuffer.size() < 3)
                    break;
                uint16_t netNumUsers;
                memcpy(&netNumUsers, &recvBuffer[1], 2);
                uint16_t numUsers = ntohs(netNumUsers);
                size_t expectedSize = 1 + 2 + numUsers * (4 + 2);
                if (recvBuffer.size() < expectedSize)
                    break;

                std::cout << "==========RECV START==========" << std::endl;
                std::cout << "Users:" << std::endl;
                size_t offset = 3;
                for (int i = 0; i < numUsers; i++) 
                {
                    if (offset + 6 > recvBuffer.size())
                        break;
                    char ipStr[INET_ADDRSTRLEN] = { 0 };
                    inet_ntop(AF_INET, &recvBuffer[offset], ipStr, INET_ADDRSTRLEN);
                    offset += 4;
                    uint16_t netPort;
                    memcpy(&netPort, &recvBuffer[offset], 2);
                    uint16_t port = ntohs(netPort);
                    offset += 2;
                    std::cout  << ipStr << ":" << port << std::endl;
                }
                std::cout << "==========RECV END==========" << std::endl;

                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + expectedSize);
            }
            // Process echo error message.
            else if (command == CMD_ECHO_ERROR) 
            {
                std::cout << "==========RECV START==========" << std::endl;
                std::cout << "Echo error" << std::endl;
                std::cout << "==========RECV END==========" << std::endl;
                recvBuffer.erase(recvBuffer.begin());
            }
            else 
            {
                // Unknown command: remove one byte and continue.
                recvBuffer.erase(recvBuffer.begin());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: Parse a destination in the form "ip:port" into separate variables.
bool parseDestination(const std::string& dest, std::string& ip, uint16_t& port) 
{
    size_t colonPos = dest.find(':');
    if (colonPos == std::string::npos)
        return false;
    ip = dest.substr(0, colonPos);
    std::string portStr = dest.substr(colonPos + 1);
    port = static_cast<uint16_t>(std::atoi(portStr.c_str()));
    return true;
}

// ---------------------------------------------------------------------------
// Main thread function: Reads user input, constructs messages, and sends them.
void handleUserInput(SOCKET clientSocket) 
{
    std::string input;
    while (std::getline(std::cin, input)) 
    {
        if (input.empty())
            continue;

        // /q command: Quit.
        if (input == "/q") 
        {
            uint8_t msg = CMD_REQ_QUIT;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
            break;
        }
        // /t command: Send hexadecimal data.
        else if (input.rfind("/t", 0) == 0)
        {
            // Expect format: "/t <hexstring>"
            if (input.size() > 3 && input[2] == ' ') 
            {
                std::string hexPart = input.substr(3);
                std::vector<uint8_t> message = hexStringToBytes(hexPart);

                if (!message.empty())
                    send(clientSocket, reinterpret_cast<const char*>(message.data()), static_cast<int>(message.size()), 0);
            }
        }
        // /e command: Echo request.
        else if (input.rfind("/e", 0) == 0) 
        {
            // Expected format: /e <dest_ip:dest_port> <message>
            std::istringstream iss(input);
            std::string command, destField, text;
            iss >> command >> destField;
            std::getline(iss, text); // Remainder is the message.
            if (destField.empty() || text.empty()) 
            {
                std::cerr << "Invalid /e command format. Usage: /e <dest_ip:dest_port> <message>" << std::endl;
                continue;
            }
            // Trim any leading spaces from text.
            size_t start = text.find_first_not_of(" ");
            if (start != std::string::npos)
                text = text.substr(start);
            std::string destIP;
            uint16_t destPort;
            if (!parseDestination(destField, destIP, destPort)) 
            {
                std::cerr << "Invalid destination format. Use ip:port" << std::endl;
                continue;
            }
            // Construct the REQ_ECHO message:
            // 1 byte: command, 4 bytes: destination IP, 2 bytes: destination port,
            // 4 bytes: text length, then text payload.
            std::vector<uint8_t> buffer;
            buffer.push_back(CMD_REQ_ECHO);
            in_addr addr;
            if (inet_pton(AF_INET, destIP.c_str(), &addr) != 1) 
            {
                std::cerr << "Invalid IP address." << std::endl;
                continue;
            }

            uint8_t* ipBytes = reinterpret_cast<uint8_t*>(&addr.s_addr);
            buffer.insert(buffer.end(), ipBytes, ipBytes + 4);
            uint16_t netPort = htons(destPort);
            uint8_t portBytes[2];
            memcpy(portBytes, &netPort, 2);
            buffer.insert(buffer.end(), portBytes, portBytes + 2);
            uint32_t textLen = static_cast<uint32_t>(text.size());
            uint32_t netTextLen = htonl(textLen);
            uint8_t textLenBytes[4];
            memcpy(textLenBytes, &netTextLen, 4);
            buffer.insert(buffer.end(), textLenBytes, textLenBytes + 4);
            buffer.insert(buffer.end(), text.begin(), text.end());
            send(clientSocket, reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
        }
        // /l command: Request list of users.
        else if (input == "/l")
        {
            uint8_t msg = CMD_REQ_LISTUSERS;
            send(clientSocket, reinterpret_cast<const char*>(&msg), 1, 0);
        }
        else 
        {
            std::cerr << "Unknown command. Supported commands: /q, /t, /e, /l" << std::endl;
        }
    }
}

int main(int argc, char** argv) 
{
    // Prompt for server IP address.
    std::string serverIP;
    std::cout << "Server IP Address: ";
    std::getline(std::cin, serverIP);

    // Prompt for server port number.
    std::string portNumber;
    std::cout << "\nServer Port Number: ";
    std::getline(std::cin, portNumber);
    std::cout << "\n";

    // Initialize Winsock.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData) != 0) 
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return 1;
    }

    // Resolve server address.
    addrinfo hints{}, * info = nullptr;
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int errorCode = getaddrinfo(serverIP.c_str(), portNumber.c_str(), &hints, &info);
    if (errorCode != 0 || info == nullptr) 
    {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return 1;
    }

    // Create socket.
    SOCKET clientSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (clientSocket == INVALID_SOCKET) 
    {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

    // Connect to the server.
    errorCode = connect(clientSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (errorCode == SOCKET_ERROR) 
    {
        std::cerr << "connect() failed." << std::endl;
        freeaddrinfo(info);
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(info);

    // Launch a receiver thread.
    std::thread receiverThread(receiveFromServer, clientSocket);

    // Main thread handles user input.
    handleUserInput(clientSocket);

    // If /q was entered, close socket and wait for the receiver thread to finish.
    closesocket(clientSocket);
    receiverThread.join();
    WSACleanup();

    return 0;
}
#endif