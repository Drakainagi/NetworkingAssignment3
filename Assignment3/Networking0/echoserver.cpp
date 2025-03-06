/* Start Header
*****************************************************************/
/*!
\file echoserver.cpp
\author weijie.soh
\par CSD2161/CSD2160/CS260/MET3302 Assignment 2
\date 21 February 2025
\brief
    This file implements a multi-threaded echo server using Winsock and a thread pool
    (via taskqueue.h/taskqueue.hpp). The server accepts multiple client connections,
    maintains a list of connected clients, and forwards echo messages according to a
    defined protocol.
Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header
*******************************************************************/



#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if 1
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <cstring>   // for memcpy
#include <cstdlib>   // for atoi
#include <optional>

// Link with Winsock library.
#pragma comment(lib, "ws2_32.lib")

// Include taskqueue headers.
#include "taskqueue.h"

// ---------------------------------------------------------------------------
// Command ID definitions as per the PDF.
#define CMD_UNKNOWN         0x0
#define CMD_REQ_QUIT        0x1
#define CMD_REQ_ECHO        0x2
#define CMD_RSP_ECHO        0x3
#define CMD_REQ_LISTUSERS   0x4
#define CMD_RSP_LISTUSERS   0x5
#define CMD_CMDTEST         0x20
#define CMD_ECHO_ERROR      0x30

// Echo message header size: 1 (command) + 4 (IP) + 2 (port) + 4 (text length)
#define ECHO_HEADER_SIZE    11  
#define MAX_TEXT_LEN        9000
#define NUM_OF_THREADS      10
#define NUM_OF_TASK_SLOTS   10
// ---------------------------------------------------------------------------
// Global container for connected clients.
struct ClientInfo
{
    SOCKET socket;
    std::string ip;    // In dotted-decimal format.
    uint16_t port;     // In host order.
};

std::vector<ClientInfo> gClients;
std::mutex gClientsMutex;

// ---------------------------------------------------------------------------
// Helper: Convert 4 bytes from buffer to a dotted IP string.
std::string convertIP(const uint8_t* buf)
{
    in_addr addr;
    memcpy(&addr, buf, 4);
    char ipStr[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
    return std::string(ipStr);
}

// ---------------------------------------------------------------------------
// Helper: Find a client in gClients by IP and port.
ClientInfo* findClient(const std::string& ip, uint16_t port)
{
    std::lock_guard<std::mutex> lock(gClientsMutex);
    for (auto& client : gClients)
    {
        if (client.ip == ip && client.port == port)
            return &client;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: Remove a client from gClients based on socket.
void removeClient(SOCKET sock)
{
    std::lock_guard<std::mutex> lock(gClientsMutex);
    for (auto it = gClients.begin(); it != gClients.end(); ++it)
    {
        if (it->socket == sock)
        {
            gClients.erase(it);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Function: handleClient
//
// Handles communication with a connected client. This function processes
// incoming messages and performs the following:
//   - For REQ_QUIT (0x1): Closes the connection.
//   - For REQ_ECHO (0x2): Looks up the destination client; if found, replaces
//     the destination IP and port with the source client’s info and forwards
//     the message. If not, sends an ECHO_ERROR (0x30).
//   - For RSP_ECHO (0x3): Looks up the original requester and forwards the
//     response after modifying the fields.
void handleClient(ClientInfo client)
{
    std::vector<uint8_t> messageBuffer;
    uint8_t recvBuffer[1024] = { 0 };

    while (true)
    {
        int bytesReceived = recv(client.socket, reinterpret_cast<char*>(recvBuffer), sizeof(recvBuffer), 0);
        if (bytesReceived <= 0)
        {
            std::cout << "Client disconnected: " << client.ip << ":" << client.port << std::endl;
            break;
        }
        // Append received data to the message buffer.
        messageBuffer.insert(messageBuffer.end(), recvBuffer, recvBuffer + bytesReceived);

        // Process complete messages from the buffer.
        while (!messageBuffer.empty())
        {
            uint8_t command = messageBuffer[0];

            // Handle quit command.
            if (command == CMD_REQ_QUIT)
            {
                //std::cout << "REQ_QUIT received from " << client.ip << ":" << client.port << std::endl;
                goto CLEANUP;
            }
            // Handle REQ_ECHO and RSP_ECHO commands.
            else if (command == CMD_REQ_ECHO || command == CMD_RSP_ECHO)
            {
                // Check if the header is complete.
                if (messageBuffer.size() < ECHO_HEADER_SIZE)
                    break;  // Wait for more data.

                // Extract destination IP (4 bytes) from message.
                std::string destIP = convertIP(&messageBuffer[1]);

                // Extract destination port (2 bytes).
                uint16_t netPort;
                memcpy(&netPort, &messageBuffer[5], 2);
                uint16_t destPort = ntohs(netPort);

                // Extract text length (4 bytes).
                uint32_t netTextLen;
                memcpy(&netTextLen, &messageBuffer[7], 4);
                uint32_t textLen = ntohl(netTextLen);

                // Check if the complete message has been received.
                if (messageBuffer.size() < ECHO_HEADER_SIZE + textLen)
                    break;  // Incomplete message; wait for more.

                // Extract the text payload.
                std::string text(reinterpret_cast<char*>(&messageBuffer[ECHO_HEADER_SIZE]), textLen);

                if (command == CMD_REQ_ECHO)
                {
                    //std::cout << "REQ_ECHO from " << client.ip << ":" << client.port
                    //    << " destined for " << destIP << ":" << destPort << std::endl;
                    // Look up the destination client.
                    auto destOpt = findClient(destIP, destPort);
                    if (!destOpt)
                    {
                        std::cout << "Destination client not found. Sending ECHO_ERROR." << std::endl;
                        uint8_t errorMsg = CMD_ECHO_ERROR;
                        send(client.socket, reinterpret_cast<const char*>(&errorMsg), 1, 0);
                    }
                    else
                    {
                        // Show the recv message
                        std::cout << "==========RECV START==========" << std::endl;
                        std::cout << destIP << ":" << destPort << std::endl;
                        std::cout << text << std::endl;
                        std::cout << "==========RECV END==========" << std::endl;

                        // Forward the REQ_ECHO message to the destination.
                        std::vector<uint8_t> fwdMsg(messageBuffer.begin(), messageBuffer.begin() + ECHO_HEADER_SIZE + textLen);
                        // Replace destination fields with source client info.
                        in_addr srcAddr;
                        inet_pton(AF_INET, client.ip.c_str(), &srcAddr);
                        memcpy(&fwdMsg[1], &srcAddr, 4);
                        uint16_t netSourcePort = htons(client.port);
                        memcpy(&fwdMsg[5], &netSourcePort, 2);
                        send(destOpt->socket, reinterpret_cast<const char*>(fwdMsg.data()), static_cast<int>(fwdMsg.size()), 0);
                    }
                }
                else if (command == CMD_RSP_ECHO)
                {
                    //std::cout << "RSP_ECHO from " << client.ip << ":" << client.port << std::endl;
                    // Look up the original requester using the destination fields.
                    auto requesterOpt = findClient(destIP, destPort);
                    if (requesterOpt)
                    {
                        // Show the recv message
                        //std::cout << "==========RECV START==========" << std::endl;
                        //std::cout << destIP << ":" << destPort << std::endl;
                        //std::cout << text << std::endl;
                        //std::cout << "==========RECV END==========" << std::endl;

                        std::vector<uint8_t> fwdMsg(messageBuffer.begin(), messageBuffer.begin() + ECHO_HEADER_SIZE + textLen);
                        // Replace destination fields with responder's info.
                        in_addr respAddr;
                        inet_pton(AF_INET, client.ip.c_str(), &respAddr);
                        memcpy(&fwdMsg[1], &respAddr, 4);
                        uint16_t netRespPort = htons(client.port);
                        memcpy(&fwdMsg[5], &netRespPort, 2);
                        send(requesterOpt->socket, reinterpret_cast<const char*>(fwdMsg.data()), static_cast<int>(fwdMsg.size()), 0);
                    }
                    else
                    {
                        std::cout << "Original requester not found for RSP_ECHO." << std::endl;
                    }
                }
                // Remove the processed message from the buffer.
                messageBuffer.erase(messageBuffer.begin(), messageBuffer.begin() + ECHO_HEADER_SIZE + textLen);
            }
            // Handle REQ_LISTUSERS and RSP_LISTUSERS commands.
            else if (command == CMD_REQ_LISTUSERS || CMD_RSP_LISTUSERS) //CMD_RSP_LISTUSERS just in case for some weird reason client sends back
            {
                uint8_t listCommand = messageBuffer[0];
                messageBuffer.erase(messageBuffer.begin());  // Remove the 1-byte command.

                if (listCommand == CMD_REQ_LISTUSERS)
                {
                    // Build the response message with RSP_LISTUSERS command.
                    std::vector<uint8_t> listMsg;
                    listMsg.push_back(CMD_RSP_LISTUSERS);

                    // Lock the client list to read the current number of connected users.
                    uint16_t numUsers;
                    {
                        std::lock_guard<std::mutex> lock(gClientsMutex);
                        numUsers = static_cast<uint16_t>(gClients.size());
                    }
                    // Convert the number of users to network byte order.
                    uint16_t netNumUsers = htons(numUsers);
                    // Append the 2 bytes of netNumUsers.
                    uint8_t* pNum = reinterpret_cast<uint8_t*>(&netNumUsers);
                    listMsg.insert(listMsg.end(), pNum, pNum + sizeof(netNumUsers));

                    // Append each client's IP (4 bytes) and port (2 bytes).
                    {
                        std::lock_guard<std::mutex> lock(gClientsMutex);
                        for (const auto& cl : gClients)
                        {
                            // Convert the IP string to binary (in network order).
                            in_addr addr;
                            inet_pton(AF_INET, cl.ip.c_str(), &addr);
                            uint8_t* pAddr = reinterpret_cast<uint8_t*>(&addr);
                            listMsg.insert(listMsg.end(), pAddr, pAddr + 4);

                            // Convert the port to network byte order.
                            uint16_t netPort = htons(cl.port);
                            uint8_t* pPort = reinterpret_cast<uint8_t*>(&netPort);
                            listMsg.insert(listMsg.end(), pPort, pPort + 2);
                        }
                    }
                    // Send the constructed RSP_LISTUSERS message back to the requesting client.
                    send(client.socket, reinterpret_cast<const char*>(listMsg.data()),
                        static_cast<int>(listMsg.size()), 0);
                }
            }
            else
            {
                std::cout << "Unknown command received: " << static_cast<int>(command) << std::endl;
                messageBuffer.clear();
            }
        }
    }

CLEANUP:
    removeClient(client.socket);
    closesocket(client.socket);
    //std::cout << "Connection closed for " << client.ip << ":" << client.port << std::endl;
}

// ---------------------------------------------------------------------------
// TAction function for the TaskQueue; wraps handleClient().
bool processClient(ClientInfo client)
{
    handleClient(client);
    return true; // Continue processing further tasks.
}

// TOnDisconnect callback (empty; can be expanded if needed).
auto onDisconnect = []() {};

// ---------------------------------------------------------------------------
// Main entry point.
int main()
{
    // Initialize Winsock.
    WSADATA wsaData;
    int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorCode != 0)
    {
        std::cerr << "WSAStartup() failed: " << errorCode << std::endl;
        return errorCode;
    }

    // Prompt for the port number.
    std::string portNumber;
    std::cout << "Server Port Number: ";
    std::getline(std::cin, portNumber);

    // Set up address information.
    addrinfo hints{}, * info = nullptr;
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    errorCode = getaddrinfo(NULL, portNumber.c_str(), &hints, &info);
    if (errorCode != 0 || info == nullptr)
    {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return errorCode;
    }

    // Retrieve and print the server's IP address.
    char localHostName[256] = { 0 };
    if (gethostname(localHostName, sizeof(localHostName)) == SOCKET_ERROR)
    {
        std::cerr << "gethostname() failed." << std::endl;
        WSACleanup();
        return 1;
    }
    addrinfo hints2 = {};
    hints2.ai_family = AF_INET; // IPv4
    hints2.ai_socktype = SOCK_STREAM;
    hints2.ai_protocol = IPPROTO_TCP;
    addrinfo* localInfo = nullptr;
    errorCode = getaddrinfo(localHostName, portNumber.c_str(), &hints2, &localInfo);
    if (errorCode != 0 || localInfo == nullptr)
    {
        std::cerr << "getaddrinfo() for local host failed." << std::endl;
        WSACleanup();
        return errorCode;
    }
    sockaddr_in* localAddr = reinterpret_cast<sockaddr_in*>(localInfo->ai_addr);
    char localIP[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &(localAddr->sin_addr), localIP, INET_ADDRSTRLEN);

    std::cout << "\nServer IP Address: " << localIP << std::endl;
    std::cout << "Server Port Number: " << portNumber << std::endl;

    // Create the listening socket.
    SOCKET listenerSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (listenerSocket == INVALID_SOCKET)
    {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

    errorCode = bind(listenerSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (errorCode != 0)
    {
        std::cerr << "bind() failed." << std::endl;
        closesocket(listenerSocket);
        freeaddrinfo(info);
        WSACleanup();
        return 2;
    }
    freeaddrinfo(info);

    errorCode = listen(listenerSocket, SOMAXCONN);
    if (errorCode != 0)
    {
        std::cerr << "listen() failed." << std::endl;
        closesocket(listenerSocket);
        WSACleanup();
        return 3;
    }

    // Create a TaskQueue for handling client connections.
    TaskQueue<ClientInfo, decltype(processClient), decltype(onDisconnect)> clientQueue(NUM_OF_THREADS, NUM_OF_TASK_SLOTS, processClient, onDisconnect);

    while (true)
    {
        sockaddr clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenerSocket, &clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept() failed." << std::endl;
            continue; // Continue accepting new connections.
        }

        // Retrieve the client's IP and port.
        char clientIP[INET_ADDRSTRLEN] = { 0 };
        char clientPortStr[NI_MAXSERV] = { 0 };
        getnameinfo(&clientAddr, clientAddrLen, clientIP, sizeof(clientIP),
            clientPortStr, sizeof(clientPortStr), NI_NUMERICHOST | NI_NUMERICSERV);
        uint16_t clientPort = static_cast<uint16_t>(atoi(clientPortStr));
        std::cout << "\nClient IP Address: " << clientIP << std::endl;
        std::cout << "Client Port Number: " << clientPort << std::endl;

        // Create a ClientInfo object.
        ClientInfo clientInfo;
        clientInfo.socket = clientSocket;
        clientInfo.ip = clientIP;
        clientInfo.port = clientPort;

        // Add the client to the global client list.
        {
            std::lock_guard<std::mutex> lock(gClientsMutex);
            gClients.push_back(clientInfo);
        }

        // Enqueue the client for processing by the thread pool.
        clientQueue.produce(clientInfo);
    }

    // Cleanup (not reached in this infinite loop).
    shutdown(listenerSocket, SD_BOTH);
    closesocket(listenerSocket);
    WSACleanup();
    return 0;
}
#endif