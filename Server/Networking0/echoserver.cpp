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
         This file implements a robust multi-threaded server for an asteroid shooter.
         It supports client joining and player input via UDP, maintains game state
         (spawning players, asteroids, bullets), and broadcasts asteroid spawn events.

         Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header
*******************************************************************/

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
#include <chrono>
#include <cmath>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

//---------------------------------------------------------------------------------
// Network and Server Constants
//---------------------------------------------------------------------------------
constexpr uint16_t SERVER_PORT = 9000;
constexpr int MAX_PLAYERS = 4;
constexpr int BUFFER_SIZE = 1024;

//---------------------------------------------------------------------------------
// Packet Types
//---------------------------------------------------------------------------------
enum PacketType : uint8_t
{
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_INPUT = 0x04,
    ACK = 0x05,
    ASTEROID_SPAWN = 0x06  // New packet type for asteroid spawn
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
    float rotate;   // rotation input (e.g., left/right)
    float shoot;    // nonzero if shooting
};

struct AsteroidSpawnPacket {
    uint8_t type = ASTEROID_SPAWN;
    float posX;
    float posY;
    float velX;
    float velY;
    int health;
};
#pragma pack(pop)

//---------------------------------------------------------------------------------
// Client and Game Entity Structures
//---------------------------------------------------------------------------------
struct ClientInfo {
    sockaddr_in address;
    uint32_t playerId;
};

struct PlayerEntity {
    uint32_t playerId;
    float posX, posY;
    float velX, velY;
    float rotation;
    float health;
};

struct AsteroidEntity {
    float posX, posY;
    float velX, velY;
    int health;
};

struct BulletEntity {
    uint32_t ownerId;
    float posX, posY;
    float velX, velY;
};

//---------------------------------------------------------------------------------
// Global State and Synchronization
//---------------------------------------------------------------------------------
std::mutex clientsMutex;
std::vector<ClientInfo> clients;
std::atomic<uint32_t> nextPlayerId{ 1 };

std::mutex gameStateMutex;
std::map<uint32_t, PlayerEntity> players;
std::vector<AsteroidEntity> asteroids;
std::vector<BulletEntity> bullets;

std::atomic<bool> running{ true };

// Global UDP socket for sending broadcast messages.
SOCKET g_serverSocket = INVALID_SOCKET;

// Helper: Compare two sockaddr_in addresses.
bool addressesEqual(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

//---------------------------------------------------------------------------------
// Game Logic Functions
//---------------------------------------------------------------------------------
void spawnAsteroid()
{
    // Create a new asteroid entity.
    AsteroidEntity asteroid;
    asteroid.posX = static_cast<float>(rand() % 800);
    asteroid.posY = static_cast<float>(rand() % 600);
    float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.1415926f;
    float speed = 50.0f + static_cast<float>(rand() % 50);
    asteroid.velX = cosf(angle) * speed;
    asteroid.velY = sinf(angle) * speed;
    asteroid.health = 100;

    {
        std::lock_guard<std::mutex> lock(gameStateMutex);
        asteroids.push_back(asteroid);
    }

    // Build the asteroid spawn packet.
    AsteroidSpawnPacket spawnPacket;
    spawnPacket.posX = asteroid.posX;
    spawnPacket.posY = asteroid.posY;
    spawnPacket.velX = asteroid.velX;
    spawnPacket.velY = asteroid.velY;
    spawnPacket.health = asteroid.health;

    // Broadcast the spawn packet to all connected clients.
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& client : clients)
    {
        int bytesSent = sendto(g_serverSocket,
            reinterpret_cast<char*>(&spawnPacket),
            sizeof(spawnPacket),
            0,
            reinterpret_cast<const sockaddr*>(&client.address),
            sizeof(client.address));
        if (bytesSent == SOCKET_ERROR)
        {
            std::cerr << "[ERROR] Failed to send asteroid spawn packet to player "
                << client.playerId << " (Error: " << WSAGetLastError() << ")" << std::endl;
        }
    }
}

void updateGameState(float dt)
{
    std::lock_guard<std::mutex> lock(gameStateMutex);
    // Update player positions
    for (auto& kv : players)
    {
        PlayerEntity& player = kv.second;
        player.posX += player.velX * dt;
        player.posY += player.velY * dt;
        // Here you can add friction or boundary checking.
    }

    // Update asteroids
    for (auto& asteroid : asteroids)
    {
        asteroid.posX += asteroid.velX * dt;
        asteroid.posY += asteroid.velY * dt;
        // Example: wrap-around logic for a screen of size 800x600.
        if (asteroid.posX < 0) asteroid.posX += 800;
        if (asteroid.posX > 800) asteroid.posX -= 800;
        if (asteroid.posY < 0) asteroid.posY += 600;
        if (asteroid.posY > 600) asteroid.posY -= 600;
    }

    // Update bullets
    for (auto& bullet : bullets)
    {
        bullet.posX += bullet.velX * dt;
        bullet.posY += bullet.velY * dt;
        // Optionally mark bullets expired if off-screen.
    }

    // (Collision detection and health updates would be handled here.)
}

// The game loop thread updates game state at a fixed time step.
void gameLoop()
{
    const float dt = 0.016f; // ~60 FPS
    float spawnTimer = 0.0f;
    while (running)
    {
        auto start = std::chrono::high_resolution_clock::now();

        updateGameState(dt);

        // Spawn an asteroid every 5 seconds.
        spawnTimer += dt;
        if (spawnTimer >= 5.0f)
        {
            spawnAsteroid();
            spawnTimer = 0.0f;
        }

        // In a full server, you might broadcast the complete game state to clients here.

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = end - start;
        if (elapsed.count() < dt)
            std::this_thread::sleep_for(std::chrono::duration<float>(dt) - elapsed);
    }
}

//---------------------------------------------------------------------------------
// Network (UDP) Receive Thread
//---------------------------------------------------------------------------------
void serverLoop(SOCKET serverSocket)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (running)
    {
        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0,
            reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

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

            // Check if client already connected.
            bool alreadyExists = false;
            for (const auto& client : clients) {
                if (addressesEqual(client.address, clientAddr)) {
                    alreadyExists = true;
                    break;
                }
            }
            if (alreadyExists)
                continue;

            uint32_t assignedId = nextPlayerId++;
            clients.push_back({ clientAddr, assignedId });

            // Also spawn a new player entity.
            {
                std::lock_guard<std::mutex> gameLock(gameStateMutex);
                PlayerEntity newPlayer;
                newPlayer.playerId = assignedId;
                newPlayer.posX = 400.0f;  // Center of screen
                newPlayer.posY = 300.0f;
                newPlayer.velX = 0.0f;
                newPlayer.velY = 0.0f;
                newPlayer.rotation = 0.0f;
                newPlayer.health = 100.0f;
                players[assignedId] = newPlayer;
            }

            JoinAcceptPacket response;
            response.type = JOIN_ACCEPT;
            response.playerId = assignedId;

            sendto(serverSocket, reinterpret_cast<char*>(&response), sizeof(response), 0,
                reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr));

            std::cout << "[INFO] Player " << assignedId << " joined from "
                << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
        }
        else if (packetType == PLAYER_INPUT)
        {
            PlayerInputPacket* input = reinterpret_cast<PlayerInputPacket*>(buffer);
            {
                std::lock_guard<std::mutex> gameLock(gameStateMutex);
                auto it = players.find(input->playerId);
                if (it != players.end()) {
                    PlayerEntity& player = it->second;
                    // Update velocity based on input (simple direct assignment).
                    player.velX = input->moveX * 200.0f;
                    player.velY = input->moveY * 200.0f;
                    player.rotation = input->rotate;
                    // If shooting, create a bullet.
                    if (fabs(input->shoot) > 0.1f)
                    {
                        BulletEntity bullet;
                        bullet.ownerId = input->playerId;
                        bullet.posX = player.posX;
                        bullet.posY = player.posY;
                        float rad = player.rotation;
                        bullet.velX = cosf(rad) * 400.0f;
                        bullet.velY = sinf(rad) * 400.0f;
                        bullets.push_back(bullet);
                    }
                }
            }

            std::cout << "[INPUT] Player " << input->playerId
                << " MoveX: " << input->moveX
                << " MoveY: " << input->moveY << std::endl;
        }
        // Additional packet types (such as ACK or GAME_UPDATE) can be handled here.
    }
}

//---------------------------------------------------------------------------------
// Console Attachment Helper
//---------------------------------------------------------------------------------
void AttachConsoleIfNeeded()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
    {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
        std::cout << "[INFO] Console attached." << std::endl;
    }
}

//---------------------------------------------------------------------------------
// Main Entry Point
//---------------------------------------------------------------------------------
int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // Create UDP socket.
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }
    g_serverSocket = serverSocket;  // Make socket available for broadcast in spawnAsteroid()

    // Prepare server address and bind.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Print local IP address (resolved from hostname).
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR) {
        addrinfo hints{}, * info = nullptr;
        hints.ai_family = AF_INET;      // IPv4
        hints.ai_socktype = SOCK_DGRAM; // For UDP
        hints.ai_flags = AI_PASSIVE;    // Fill in my IP

        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0 && info != nullptr) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
            std::cout << "[SERVER] Local IP Address: " << ip << std::endl;
            freeaddrinfo(info);
        }
        else {
            std::cerr << "getaddrinfo() failed to resolve hostname." << std::endl;
        }
    }
    else {
        std::cerr << "gethostname() failed." << std::endl;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[SERVER] UDP server listening on port " << SERVER_PORT << std::endl;

    // Start the network and game loop threads.
    std::thread networkThread(serverLoop, serverSocket);
    std::thread gameThread(gameLoop);

    // Wait for threads to finish (in a real server, you would have proper shutdown handling).
    networkThread.join();
    gameThread.join();

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
#endif
