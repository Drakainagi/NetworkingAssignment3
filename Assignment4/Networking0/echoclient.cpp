/* Start Header
*********************************************************************
  \file    ftpclient.cpp
  \authors weijie.soh (Soh Wei Jie)
           lee.v (Victor Lee)
  \par     DigiPen Institute of Technology
  \date    16 March 2025
  \brief
         This file implements a multi-threaded FTP client that communicates
         with the server via TCP for control messages and via UDP for file
         downloading. The client supports:
           - Requesting the file list (/l)
           - Requesting a file download (/d <client_ip:udpPort> <filename>)
           - Quitting (/q)
         The UDP file transfer uses a basic reliable protocol (selective repeat)
         to ensure data integrity over a lossy channel.

         Robustness improvements include enhanced error checking, a RAII socket
         wrapper, consistent logging, and modularized code sections.

         THIS FILE IS STILL IN DEVELOPMENT AND MIGHT NOT WORK AS INTENDED

  Copyright (C) 2025 DigiPen Institute of Technology.
/* End Header
*********************************************************************/

// echoclient.cpp
// Multiplayer Spaceships Client (UDP Only)
// Connects to server and sends input packets in real-time

#if 0
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;  // Each client should pick a unique port
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
struct JoinRequestPacket 
{
    uint8_t type = JOIN_REQUEST;
};

struct JoinAcceptPacket 
{
    uint8_t type = JOIN_ACCEPT;
    uint32_t playerId;
};

struct PlayerInputPacket 
{
    uint8_t type = PLAYER_INPUT;
    uint32_t playerId;
    float moveX;
    float moveY;
};
#pragma pack(pop)

std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;

void receiveThread(SOCKET udpSocket) 
{
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    while (running) 
    {
        int bytes = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (sockaddr*)&fromAddr, &fromLen);
        if (bytes <= 0) continue;

        uint8_t packetType = buffer[0];
        if (packetType == JOIN_ACCEPT) 
        {
            JoinAcceptPacket* pkt = reinterpret_cast<JoinAcceptPacket*>(buffer);
            myPlayerId = pkt->playerId;
            std::cout << "[JOINED] Assigned Player ID: " << myPlayerId << std::endl;
        }
    }
}

int main() 
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    int clientId = 0;
    std::cout << "Enter client ID (1-4): ";
    std::cin >> clientId;
    if (clientId < 1 || clientId > 4) clientId = 1;

    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(CLIENT_PORT_START + clientId - 1);

    if (bind(udpSocket, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) 
    {
        std::cerr << "Bind failed." << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    char serverIpStr[INET_ADDRSTRLEN];
    std::cout << "Enter server IP address: ";
    std::cin >> serverIpStr;
    inet_pton(AF_INET, serverIpStr, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);

    JoinRequestPacket join{};
    sendto(udpSocket, reinterpret_cast<char*>(&join), sizeof(join), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::thread recvThread(receiveThread, udpSocket);

    std::cout << "Use WASD to move. Press Q to quit.\n";

    while (running) {
        if (_kbhit()) {
            char input = _getch();
            if (input == 'q' || input == 'Q') {
                running = false;
                break;
            }

            float dx = 0, dy = 0;
            if (input == 'w' || input == 'W') dy = -1;
            else if (input == 's' || input == 'S') dy = 1;
            else if (input == 'a' || input == 'A') dx = -1;
            else if (input == 'd' || input == 'D') dx = 1;

            PlayerInputPacket pkt;
            pkt.type = PLAYER_INPUT;
            pkt.playerId = myPlayerId;
            pkt.moveX = dx;
            pkt.moveY = dy;

            sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
                (sockaddr*)&serverAddr, sizeof(serverAddr));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Slight delay to avoid flooding
    }

    recvThread.join();
    closesocket(udpSocket);
    WSACleanup();
    return 0;
}

#endif