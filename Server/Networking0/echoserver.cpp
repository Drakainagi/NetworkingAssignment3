/* Start Header
*****************************************************************/
/*!
  \file   server.cpp
  \authors weijie.soh (Soh Wei Jie)
           lee.v (Victor Lee)
           joshuayuechen.sim (Joshua Sim Yue Chen)
  \par    DigiPen Institute of Technology
  \date   19 March 2025
  \brief
         This file implements the server for an asteroid shooter.
         It receives JOIN_REQUEST, PLAYER_UPDATE, and BULLET_SPAWN packets
         from clients, updates the master game state (players, asteroids,
         bullets, etc.), and periodically broadcasts a GAME_UPDATE packet
         to all connected clients. The server relays the playership's state
         as received from the clients. Velocity fields have been added to allow
         more accurate extrapolation.

         Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header
*******************************************************************/

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

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
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

//---------------------------------------------------------------------------------
// Network and Server Constants
//---------------------------------------------------------------------------------
constexpr uint16_t SERVER_PORT = 9000;
constexpr int MAX_PLAYERS = 4;
constexpr int BUFFER_SIZE = 1024;
constexpr float UPDATE_RATE = 0.033f; // ~30 updates per second

//---------------------------------------------------------------------------------
// Packet Types
//---------------------------------------------------------------------------------
enum PacketType : uint8_t
{
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_UPDATE = 0x04,
    ACK = 0x05,
    BULLET_SPAWN = 0x06  // New packet type for bullet spawn.
};

#pragma pack(push, 1)
struct JoinRequestPacket {
    uint8_t type = JOIN_REQUEST;
};

struct JoinAcceptPacket {
    uint8_t type = JOIN_ACCEPT;
    uint32_t playerId;
};

// Updated PLAYER_UPDATE packet includes velocity.
struct PlayerUpdatePacket {
    uint8_t type = PLAYER_UPDATE;
    uint32_t playerId;
    float pos_x;
    float pos_y;
    float angle;   // Orientation (in radians)
    float vel_x;   // Linear velocity X
    float vel_y;   // Linear velocity Y
};

// New BULLET_SPAWN packet: contains bullet type, spawn position, velocity, and damage.
struct BulletSpawnPacket {
    uint8_t type = BULLET_SPAWN;
    uint32_t playerId;   // Owner of the bullet.
    uint8_t bulletType;  // e.g., 0 = standard.
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
    float damage;
};

// Updated GameObjectData includes velocity fields.
struct GameObjectData {
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet.
    uint32_t playerId;  // Valid for players; 0 for others.
    float pos_x;
    float pos_y;
    float rotation;     // For players, this is the angle.
    float scale;
    float vel_x;
    float vel_y;
};

struct GameUpdatePacket {
    uint8_t type = GAME_UPDATE;
    uint32_t objectCount;
    // Followed by an array of GameObjectData.
    GameObjectData objects[4000]; // Legacy placeholder.
};
#pragma pack(pop)

//---------------------------------------------------------------------------------
// Game Entity Structures
//---------------------------------------------------------------------------------
enum ObjectType {
    Player,
    Asteroid,
    Bullet
};

// Extended PlayerEntity now has velocity fields.
struct PlayerEntity {
    uint32_t playerId;
    float pos_x, pos_y;
    float rotation;  // Angle (in radians)
    float scale;
    float health;
    float vel_x, vel_y;
};

struct AsteroidEntity {
    float pos_x, pos_y;
    float rotation;
    float scale;
    float vel_x, vel_y;
    int health;
};

// Updated BulletEntity now includes damage.
struct BulletEntity {
    uint32_t ownerId;
    float pos_x, pos_y;
    float rotation;
    float scale;
    float vel_x, vel_y;
    float damage;
};

//---------------------------------------------------------------------------------
// Global State and Synchronization
//---------------------------------------------------------------------------------
std::mutex clientsMutex;
std::vector<sockaddr_in> clientAddresses;  // One per client.
std::atomic<uint32_t> nextPlayerId{ 1 };

std::mutex gameStateMutex;
std::map<uint32_t, PlayerEntity> players;
std::vector<AsteroidEntity> asteroids;
std::vector<BulletEntity> bullets;

std::atomic<bool> running{ true };

SOCKET g_serverSocket = INVALID_SOCKET;

// Helper function to compare two sockaddr_in addresses.
bool addressesEqual(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

//---------------------------------------------------------------------------------
// Game Logic Functions
//---------------------------------------------------------------------------------

// Spawns an asteroid with random properties.
void spawnAsteroid()
{
    AsteroidEntity asteroid;
    asteroid.pos_x = static_cast<float>(rand() % 800);
    asteroid.pos_y = static_cast<float>(rand() % 600);
    asteroid.scale = static_cast<float>(rand() % 40 + 30);
    asteroid.rotation = (static_cast<float>(rand()) / RAND_MAX) * 2.0f * 3.1415926f;
    float speed = 50.0f + static_cast<float>(rand() % 50);
    asteroid.vel_x = cosf(asteroid.rotation) * speed;
    asteroid.vel_y = sinf(asteroid.rotation) * speed;
    asteroid.health = 100;

    std::lock_guard<std::mutex> lock(gameStateMutex);
    asteroids.push_back(asteroid);
}

// Updates the game state by moving asteroids, bullets, etc.
void updateGameState(float dt)
{
    std::lock_guard<std::mutex> lock(gameStateMutex);
    float windowWidth = 1600.0f;
    float windowHeight = 900.0f;

    // Update asteroids.
    for (auto& asteroid : asteroids)
    {
        asteroid.pos_x += asteroid.vel_x * dt;
        asteroid.pos_y += asteroid.vel_y * dt;
        if (asteroid.pos_x < -windowWidth / 2 - asteroid.scale / 2)
            asteroid.pos_x += windowWidth + asteroid.scale;
        else if (asteroid.pos_x > windowWidth / 2 + asteroid.scale / 2)
            asteroid.pos_x -= windowWidth + asteroid.scale;
        if (asteroid.pos_y < -windowHeight / 2 - asteroid.scale / 2)
            asteroid.pos_y += windowHeight + asteroid.scale;
        else if (asteroid.pos_y > windowHeight / 2 + asteroid.scale / 2)
            asteroid.pos_y -= windowHeight + asteroid.scale;
    }
    // Update bullets.
    for (auto& bullet : bullets)
    {
        bullet.pos_x += bullet.vel_x * dt;
        bullet.pos_y += bullet.vel_y * dt;
        // Optionally remove bullets if off-screen.
    }
    // Player entities are updated solely from PLAYER_UPDATE packets.
}

// Broadcasts the current game state to all connected clients.
void broadcastGameState()
{
    std::vector<GameObjectData> gameObjects;
    {
        std::lock_guard<std::mutex> lock(gameStateMutex);
        // Pack players.
        for (const auto& kv : players)
        {
            const PlayerEntity& p = kv.second;
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Player);
            data.playerId = p.playerId;
            data.pos_x = p.pos_x;
            data.pos_y = p.pos_y;
            data.rotation = p.rotation;
            data.scale = p.scale;
            data.vel_x = p.vel_x;
            data.vel_y = p.vel_y;
            gameObjects.push_back(data);
        }
        // Pack asteroids.
        for (const auto& ast : asteroids)
        {
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Asteroid);
            data.playerId = 0;
            data.pos_x = ast.pos_x;
            data.pos_y = ast.pos_y;
            data.rotation = ast.rotation;
            data.scale = ast.scale;
            data.vel_x = ast.vel_x;
            data.vel_y = ast.vel_y;
            gameObjects.push_back(data);
        }
        // Pack bullets.
        for (const auto& b : bullets)
        {
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Bullet);
            data.playerId = b.ownerId;
            data.pos_x = b.pos_x;
            data.pos_y = b.pos_y;
            data.rotation = b.rotation;
            data.scale = b.scale;
            data.vel_x = b.vel_x;
            data.vel_y = b.vel_y;
            gameObjects.push_back(data);
        }
    }

    size_t headerSize = sizeof(uint8_t) + sizeof(uint32_t);
    size_t objectsSize = gameObjects.size() * sizeof(GameObjectData);
    size_t packetSize = headerSize + objectsSize;
    std::vector<char> packetBuffer(packetSize);
    uint8_t packetType = GAME_UPDATE;
    uint32_t objectCount = static_cast<uint32_t>(gameObjects.size());
    memcpy(packetBuffer.data(), &packetType, sizeof(packetType));
    memcpy(packetBuffer.data() + sizeof(packetType), &objectCount, sizeof(objectCount));
    if (!gameObjects.empty())
    {
        memcpy(packetBuffer.data() + headerSize, gameObjects.data(), objectsSize);
    }

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& addr : clientAddresses)
    {
        int bytesSent = sendto(g_serverSocket,
            packetBuffer.data(),
            static_cast<int>(packetSize),
            0,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
        if (bytesSent == SOCKET_ERROR)
        {
            std::cerr << "[ERROR] GAME_UPDATE sendto failed: " << WSAGetLastError() << std::endl;
        }
    }
}

// Main game loop that updates game state and broadcasts it.
void gameLoop()
{
    const float dtFixed = UPDATE_RATE;
    float accumulator = 0.0f;
    auto previousTime = std::chrono::high_resolution_clock::now();

    while (running)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - previousTime;
        previousTime = currentTime;
        accumulator += deltaTime.count();

        while (accumulator >= dtFixed)
        {
            updateGameState(dtFixed);
            broadcastGameState();

            // Spawn an asteroid every 5 seconds.
            static float spawnTimer = 0.0f;
            spawnTimer += dtFixed;
            if (spawnTimer >= 5.0f)
            {
                spawnAsteroid();
                spawnTimer = 0.0f;
            }
            accumulator -= dtFixed;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

//---------------------------------------------------------------------------------
// Network Receive Loop for Server
//---------------------------------------------------------------------------------
void serverReceiveLoop(SOCKET serverSocket)
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

        uint8_t packetType = static_cast<uint8_t>(buffer[0]);
        if (packetType == JOIN_REQUEST)
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            bool exists = false;
            for (const auto& addr : clientAddresses)
            {
                if (addressesEqual(addr, clientAddr))
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;
            if (clientAddresses.size() >= MAX_PLAYERS)
            {
                std::cout << "[WARN] Max players reached. Ignoring join request." << std::endl;
                continue;
            }
            uint32_t assignedId = nextPlayerId++;
            clientAddresses.push_back(clientAddr);

            {
                std::lock_guard<std::mutex> gameLock(gameStateMutex);
                PlayerEntity newPlayer;
                newPlayer.playerId = assignedId;
                newPlayer.pos_x = 400.0f;
                newPlayer.pos_y = 300.0f;
                newPlayer.rotation = 0.0f;
                newPlayer.scale = 100.0f;
                newPlayer.health = 100.0f;
                newPlayer.vel_x = 0.0f;
                newPlayer.vel_y = 0.0f;
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
        else if (packetType == PLAYER_UPDATE)
        {
            PlayerUpdatePacket* updatePkt = reinterpret_cast<PlayerUpdatePacket*>(buffer);
            std::lock_guard<std::mutex> lock(gameStateMutex);
            auto it = players.find(updatePkt->playerId);
            if (it != players.end())
            {
                it->second.pos_x = updatePkt->pos_x;
                it->second.pos_y = updatePkt->pos_y;
                it->second.rotation = updatePkt->angle;
                it->second.vel_x = updatePkt->vel_x;
                it->second.vel_y = updatePkt->vel_y;
            }
            std::cout << "[UPDATE] Player " << updatePkt->playerId
                << " pos: (" << updatePkt->pos_x << ", " << updatePkt->pos_y << ")"
                << " angle: " << updatePkt->angle << std::endl;
        }
        else if (packetType == BULLET_SPAWN)
        {
            // Process bullet spawn packet.
            // Define the bullet spawn structure here to match the client's structure.
            struct BulletSpawnPacket {
                uint8_t type;
                uint32_t playerId;
                uint8_t bulletType;
                float pos_x;
                float pos_y;
                float vel_x;
                float vel_y;
                float damage;
            };

            BulletSpawnPacket* bulletPkt = reinterpret_cast<BulletSpawnPacket*>(buffer);
            BulletEntity newBullet;
            newBullet.ownerId = bulletPkt->playerId;
            newBullet.pos_x = bulletPkt->pos_x;
            newBullet.pos_y = bulletPkt->pos_y;
            newBullet.vel_x = bulletPkt->vel_x;
            newBullet.vel_y = bulletPkt->vel_y;
            newBullet.damage = bulletPkt->damage;
            // Set bullet scale (e.g., 10 units).
            newBullet.scale = 10.0f;
            // Compute bullet rotation from its velocity.
            newBullet.rotation = atan2f(newBullet.vel_y, newBullet.vel_x);

            {
                std::lock_guard<std::mutex> lock(gameStateMutex);
                bullets.push_back(newBullet);
            }

            std::cout << "[BULLET SPAWN] Player " << bulletPkt->playerId
                << " spawned bullet at (" << bulletPkt->pos_x << ", " << bulletPkt->pos_y << ")"
                << " damage: " << bulletPkt->damage << std::endl;
        }
    }
}

//---------------------------------------------------------------------------------
// Main Entry Point
//---------------------------------------------------------------------------------
int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET)
    {
        std::cerr << "[ERROR] Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }
    g_serverSocket = serverSocket;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Print local IP address.
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR)
    {
        addrinfo hints{}, * info = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0 && info != nullptr)
        {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
            std::cout << "[SERVER] Local IP Address: " << ip << std::endl;
            freeaddrinfo(info);
        }
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "[ERROR] Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[SERVER] UDP server listening on port " << SERVER_PORT << std::endl;

    std::thread netThread(serverReceiveLoop, serverSocket);
    std::thread gameThread(gameLoop);

    netThread.join();
    gameThread.join();

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
