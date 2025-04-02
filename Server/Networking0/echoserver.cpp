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
         It receives JOIN_REQUEST and PLAYER_UPDATE packets from clients,
         updates the master game state (players, asteroids, bullets, etc.),
         and periodically broadcasts a GAME_UPDATE packet to all connected clients.
         The server does not simulate player movement; it only relays the playership's
         position and rotation as received from the clients.

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
constexpr int MAX_PLAYERS = 4000;
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
    SCORE_INCREMENT = 0x06,
    SCORE_UPDATE = 0x07,
    FINAL_SCOREBOARD = 0x08

};

#pragma pack(push, 1)
struct JoinRequestPacket {
    uint8_t type = JOIN_REQUEST;
};

struct JoinAcceptPacket {
    uint8_t type = JOIN_ACCEPT;
    uint32_t playerId;
};

// New packet sent by the client containing its latest position and rotation.
struct PlayerUpdatePacket {
    uint8_t type = PLAYER_UPDATE;
    uint32_t playerId;
    float pos_x;
    float pos_y;
    float angle;  // Orientation (in radians) for rendering
};

//
// Structure for sending game updates.
// The first field is the number of game objects that follow, then an array of object data.
struct GameObjectData {
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
    float pos_x;
    float pos_y;
    float rotation; // For players, this is the angle (as updated by the client)
    float scale;
};

struct GameUpdatePacket {
    uint8_t type = GAME_UPDATE;
    uint32_t objectCount;
    // Followed by an array of GameObjectData (variable length)
    // (We build this packet dynamically.)
    GameObjectData objects[4000]; // Not used in transmission; legacy placeholder.
};

struct ScoreIncrementPacket {
    uint8_t type;
    uint32_t playerId;
    uint32_t increment;
};

struct PlayerScore {
    uint32_t playerId;
    uint32_t score;
};

struct ScoreUpdatePacket {
    uint8_t type = SCORE_UPDATE;
    uint32_t scoreCount;
    PlayerScore scores[MAX_PLAYERS];
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

struct PlayerEntity {
    uint32_t playerId;
    float pos_x, pos_y;
    float rotation;      // Angle (in radians) for rendering the ship
    float scale;
    float health;
};

struct AsteroidEntity {
    float pos_x, pos_y;
    float rotation;
    float scale;
    float vel_x, vel_y;
    int health;
};

struct BulletEntity {
    uint32_t ownerId;
    float pos_x, pos_y;
    float rotation;
    float scale;
    float vel_x, vel_y;
};

//---------------------------------------------------------------------------------
// Global State and Synchronization
//---------------------------------------------------------------------------------
std::mutex clientsMutex;
std::vector<sockaddr_in> clientAddresses;  // one per client
std::atomic<uint32_t> nextPlayerId{ 1 };

std::mutex gameStateMutex;
std::map<uint32_t, PlayerEntity> players;
std::vector<AsteroidEntity> asteroids;
std::vector<BulletEntity> bullets;

// Score board
std::map<uint32_t, uint32_t> gScoreBoard;
std::mutex gScoreMutex;

std::atomic<bool> running{ true };

// Global UDP socket for sending broadcast messages.
SOCKET g_serverSocket = INVALID_SOCKET;

// Helper function to compare two sockaddr_in addresses.
bool addressesEqual(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

//---------------------------------------------------------------------------------
// Game Logic Functions
//---------------------------------------------------------------------------------

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

// The server no longer computes player physics.
// It simply relays the updated player states as received from the clients.
void updateGameState(float dt)
{
    std::lock_guard<std::mutex> lock(gameStateMutex);
    float windowWidth = 1600.0f;
    float windowHeight = 900.0f;

    // Asteroids update.
    for (auto& asteroid : asteroids)
    {
        asteroid.pos_x += asteroid.vel_x * dt;
        asteroid.pos_y += asteroid.vel_y * dt;

        // Wrap horizontally.
        if (asteroid.pos_x < -windowWidth / 2 - asteroid.scale / 2)
            asteroid.pos_x += windowWidth + asteroid.scale;
        else if (asteroid.pos_x > windowWidth / 2 + asteroid.scale / 2)
            asteroid.pos_x -= windowWidth + asteroid.scale;

        // Wrap vertically.
        if (asteroid.pos_y < -windowHeight / 2 - asteroid.scale / 2)
            asteroid.pos_y += windowHeight + asteroid.scale;
        else if (asteroid.pos_y > windowHeight / 2 + asteroid.scale / 2)
            asteroid.pos_y -= windowHeight + asteroid.scale;
    }
    // Bullets update.
    for (auto& bullet : bullets)
    {
        bullet.pos_x += bullet.vel_x * dt;
        bullet.pos_y += bullet.vel_y * dt;
        // (Remove bullets if off-screen, etc.)
    }
    // Player entities are updated solely by incoming network updates.
}

void broadcastGameState()
{
    std::vector<GameObjectData> gameObjects;

    {
        std::lock_guard<std::mutex> lock(gameStateMutex);
        // Pack player entities.
        for (const auto& kv : players)
        {
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Player);
            data.pos_x = kv.second.pos_x;
            data.pos_y = kv.second.pos_y;
            data.rotation = kv.second.rotation; // Use the client-updated rotation.
            data.scale = kv.second.scale;
            gameObjects.push_back(data);
        }
        // Pack asteroids.
        for (const auto& ast : asteroids)
        {
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Asteroid);
            data.pos_x = ast.pos_x;
            data.pos_y = ast.pos_y;
            data.rotation = ast.rotation;
            data.scale = ast.scale;
            gameObjects.push_back(data);
        }
        // Pack bullets.
        for (const auto& b : bullets)
        {
            GameObjectData data;
            data.objectType = static_cast<uint8_t>(Bullet);
            data.pos_x = b.pos_x;
            data.pos_y = b.pos_y;
            data.rotation = b.rotation;
            data.scale = b.scale;
            gameObjects.push_back(data);
        }
    }

    // Determine the packet size.
    // Header consists of: type (1 byte) and objectCount (4 bytes)
    size_t headerSize = sizeof(uint8_t) + sizeof(uint32_t);
    size_t objectsSize = gameObjects.size() * sizeof(GameObjectData);
    size_t packetSize = headerSize + objectsSize;

    // Create a dynamic buffer to hold the packet.
    std::vector<char> packetBuffer(packetSize);

    // Write the header.
    uint8_t packetType = GAME_UPDATE;
    uint32_t objectCount = static_cast<uint32_t>(gameObjects.size());
    memcpy(packetBuffer.data(), &packetType, sizeof(packetType));
    memcpy(packetBuffer.data() + sizeof(packetType), &objectCount, sizeof(objectCount));

    // Write the object data if any.
    if (!gameObjects.empty())
    {
        memcpy(packetBuffer.data() + headerSize, gameObjects.data(), objectsSize);
    }

    // Send the packet to every connected client.
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& addr : clientAddresses) {
        int bytesSent = sendto(g_serverSocket,
            packetBuffer.data(),
            static_cast<int>(packetSize),
            0,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[ERROR] GAME_UPDATE sendto failed: " << WSAGetLastError() << std::endl;
        }
    }
}

void broadcastScoreUpdate()
{
    std::vector<PlayerScore> scoreEntries;

    {
        std::lock_guard<std::mutex> lock(gScoreMutex);
        for (const auto& [id, score] : gScoreBoard) {
            PlayerScore entry;
            entry.playerId = id;
            entry.score = score;
            scoreEntries.push_back(entry);
        }
    }

    // Prepare header
    uint8_t packetType = SCORE_UPDATE;
    uint32_t scoreCount = static_cast<uint32_t>(scoreEntries.size());

    size_t headerSize = sizeof(packetType) + sizeof(scoreCount);
    size_t scoresSize = scoreEntries.size() * sizeof(PlayerScore);
    size_t totalPacketSize = headerSize + scoresSize;

    // Create buffer
    std::vector<char> buffer(totalPacketSize);

    // Copy header
    memcpy(buffer.data(), &packetType, sizeof(packetType));
    memcpy(buffer.data() + sizeof(packetType), &scoreCount, sizeof(scoreCount));

    // Copy score entries
    if (!scoreEntries.empty())
    {
        memcpy(buffer.data() + headerSize, scoreEntries.data(), scoresSize);
    }

    std::lock_guard<std::mutex> clientsLock(clientsMutex);
    for (const auto& addr : clientAddresses)
    {
        std::cout << "[BROADCAST] Sending SCORE_UPDATE with " << scoreCount << " entries\n";
        for (const auto& entry : scoreEntries)
        {
            std::cout << "  -> Player " << entry.playerId << " = " << entry.score << "\n";
        }

        int bytesSent = sendto(g_serverSocket,
            buffer.data(),
            static_cast<int>(totalPacketSize),
            0,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));

        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[ERROR] SCORE_UPDATE sendto failed: " << WSAGetLastError() << std::endl;
        }
    }
}

void gameLoop()
{
    const float dt = UPDATE_RATE;
    while (running) {
        auto start = std::chrono::high_resolution_clock::now();
        updateGameState(dt);
        broadcastGameState();
        // For demonstration, spawn an asteroid every 5 seconds.
        static float spawnTimer = 0.0f;
        spawnTimer += dt;
        if (spawnTimer >= 5.0f) {
            spawnAsteroid();
            spawnTimer = 0.0f;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = end - start;
        if (elapsed.count() < dt)
            std::this_thread::sleep_for(std::chrono::duration<float>(dt) - elapsed);
    }
}

//---------------------------------------------------------------------------------
// Network (UDP) Receive Thread
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

        uint8_t packetType = buffer[0];
        if (packetType == JOIN_REQUEST)
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            // Check if this client is already connected.
            bool exists = false;
            for (const auto& addr : clientAddresses)
            {
                if (addressesEqual(addr, clientAddr))
                {
                    exists = true;
                    break;
                }
            }
            if (exists) continue;
            if (clientAddresses.size() >= MAX_PLAYERS) {
                std::cout << "[WARN] Max players reached. Ignoring join request." << std::endl;
                continue;
            }
            uint32_t assignedId = nextPlayerId++;
            clientAddresses.push_back(clientAddr);

            // Create a new player entity with default values.
            {
                std::lock_guard<std::mutex> gameLock(gameStateMutex);
                PlayerEntity newPlayer;
                newPlayer.playerId = assignedId;
                newPlayer.pos_x = 400.0f;
                newPlayer.pos_y = 300.0f;
                newPlayer.rotation = 0.0f;
                newPlayer.scale = 100.0f;
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
        else if (packetType == PLAYER_UPDATE)
        {
            // Client sends its own updated position and rotation.
            PlayerUpdatePacket* updatePkt = reinterpret_cast<PlayerUpdatePacket*>(buffer);
            std::lock_guard<std::mutex> lock(gameStateMutex);
            auto it = players.find(updatePkt->playerId);
            if (it != players.end()) {
                it->second.pos_x = updatePkt->pos_x;
                it->second.pos_y = updatePkt->pos_y;
                it->second.rotation = updatePkt->angle;
            }
           // std::cout << "[UPDATE] Player " << updatePkt->playerId
          //      << " pos: (" << updatePkt->pos_x << ", " << updatePkt->pos_y << ")"
          //      << " angle: " << updatePkt->angle << std::endl;
        }
        else if (packetType == SCORE_INCREMENT)
        {
            ScoreIncrementPacket* scorePkt = reinterpret_cast<ScoreIncrementPacket*>(buffer);

          
            gScoreBoard[scorePkt->playerId] += scorePkt->increment;

            std::cout << "[SCORE] Player " << scorePkt->playerId
                << " scored +" << scorePkt->increment
                << " (Total: " << gScoreBoard[scorePkt->playerId] << ")\n";

            broadcastScoreUpdate();
        }
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

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET)
    {
        std::cerr << "Failed to create UDP socket." << std::endl;
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
        std::cerr << "Bind failed." << std::endl;
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
