/* Start Header
*****************************************************************/
/*!
  \file   echoserver.cpp
  \authors weijie.soh (Soh Wei Jie)
           lee.v (Victor Lee)
           joshuayuechen.sim (Joshua Sim Yue Chen)
  \par    DigiPen Institute of Technology
  \date   19 March 2025
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
// Multiplayer Spaceships Server (UDP Only)
// Handles up to 4 clients over LAN

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#if 1
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

constexpr uint16_t SERVER_PORT = 9000;
constexpr int MAX_PLAYERS = 4;
constexpr int BUFFER_SIZE = 1024;



enum PacketType : uint8_t 
{
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_INPUT = 0x04,
    ACK = 0x05
};

#pragma pack(push, 1)
struct JoinRequestPacket {
    uint8_t type = JOIN_REQUEST;
};

struct JoinAcceptPacket {
    uint8_t type = JOIN_ACCEPT;
    uint32_t playerId;
};

struct PlayerInputPacket {
    uint8_t type = PLAYER_INPUT;
    uint32_t playerId;
    float moveX;
    float moveY;
    float RotateX; // Turn left or right
    float shoot; // whether a bullet is being shot
};
#pragma pack(pop)

struct ClientInfo {
    sockaddr_in address;
    uint32_t playerId;
};

std::mutex clientsMutex;
std::vector<ClientInfo> clients;
std::atomic<uint32_t> nextPlayerId{ 1 };

bool addressesEqual(const sockaddr_in& a, const sockaddr_in& b) 
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

void serverLoop(SOCKET serverSocket) 
{
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (true) 
    {
        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&clientAddr, &clientAddrLen);

        if (bytesReceived <= 0)
            continue;

        uint8_t packetType = buffer[0];

        if (packetType == JOIN_REQUEST) 
        {
            std::lock_guard<std::mutex> lock(clientsMutex);

            if (clients.size() >= MAX_PLAYERS) {
                std::cout << "[WARN] Max players reached. Ignoring JOIN." << std::endl;
                continue;
            }

            // Check if already connected
            bool alreadyExists = false;
            for (const auto& client : clients) {
                if (addressesEqual(client.address, clientAddr)) {
                    alreadyExists = true;
                    break;
                }
            }
            if (alreadyExists) continue;

            uint32_t assignedId = nextPlayerId++;
            clients.push_back({ clientAddr, assignedId });

            JoinAcceptPacket response;
            response.type = JOIN_ACCEPT;
            response.playerId = assignedId;

            sendto(serverSocket, reinterpret_cast<char*>(&response), sizeof(response), 0,
                (sockaddr*)&clientAddr, sizeof(clientAddr));

            std::cout << "[INFO] Player " << assignedId << " joined from "
                << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
        }
        else if (packetType == PLAYER_INPUT) 
        {
            PlayerInputPacket* input = reinterpret_cast<PlayerInputPacket*>(buffer);
            std::cout << "[INPUT] Player " << input->playerId
                << " MoveX: " << input->moveX
                << " MoveY: " << input->moveY << std::endl;

            // TODO: Queue this input to the game loop logic
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[SERVER] UDP server listening on port " << SERVER_PORT << std::endl;

    std::thread networkThread(serverLoop, serverSocket);
    networkThread.join();

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

#endif