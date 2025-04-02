/*
 *****************************************************************
 *  File:      client.cpp
 *  Author:    Joshua Sim Yue Chen (modified by ChatGPT)
 *  Brief:     Implements the client for the asteroid shooter.
 *             The client computes its own ship movement using
 *             simplified physics (client-side prediction) and
 *             sends PLAYER_UPDATE packets (including velocity).
 *             It also receives GAME_UPDATE packets containing
 *             all game objects. All entities (players, asteroids,
 *             bullets) are stored in a unified pool: index 0 is the
 *             local player and indices [1, remoteCount+1) are for
 *             remote objects. Additionally, when the player fires
 *             a bullet, a BULLET_SPAWN packet is sent to the server.
 *****************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <crtdbg.h>
#include "AEEngine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

 // Constants
constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_GAMEOBJECTS = 4000; // Maximum remote objects (local player is at index 0)

// Packet types (must match server definitions)
enum PacketType : uint8_t {
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_UPDATE = 0x04,
    ACK = 0x05,
    BULLET_SPAWN = 0x06  // New packet type for bullet spawning.
};

#pragma pack(push, 1)

// Packet for join request.
struct JoinRequestPacket {
    uint8_t type = JOIN_REQUEST;
};

// Packet for join acceptance.
struct JoinAcceptPacket {
    uint8_t type = JOIN_ACCEPT;
    uint32_t playerId;
};

// PLAYER_UPDATE packet: now includes velocity.
struct PlayerUpdatePacket {
    uint8_t type = PLAYER_UPDATE;
    uint32_t playerId;
    float pos_x;
    float pos_y;
    float angle;   // Orientation in radians.
    float vel_x;   // Linear velocity X.
    float vel_y;   // Linear velocity Y.
};

// Bullet spawn packet: sent once when the client spawns a bullet.
struct BulletSpawnPacket {
    uint8_t type = BULLET_SPAWN;
    uint32_t playerId; // Owner's ID.
    uint8_t bulletType; // Bullet type identifier.
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
    float damage;
};

// Game object data received from the server (for remote objects).
struct GameObjectData {
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet.
    uint32_t playerId;  // For player objects; for others, can be 0.
    float pos_x;
    float pos_y;
    float rotation;
    float scale;
    float vel_x;      // Velocity X component.
    float vel_y;      // Velocity Y component.
};

struct GameUpdatePacket {
    uint8_t type = GAME_UPDATE;
    uint32_t objectCount;
    GameObjectData objects[4000];
};

#pragma pack(pop)

// Global networking variables.
std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;
SOCKET udpSocket = INVALID_SOCKET;
sockaddr_in serverAddr{};

// ----------------------------------------------------------------------
// Unified Entity Pool
// ----------------------------------------------------------------------
// We reserve index 0 for the local player; remote entities (including bullets)
// are stored in indices [1, remoteCount+1).
struct GameObject {
    AEMtx33 transform;
    float pos_x, pos_y;
    float vel_x, vel_y;
    float scale;
    float rotation;
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet.
    uint32_t playerId;  // Valid for player objects.
};

GameObject gEntityPool[MAX_GAMEOBJECTS + 1];    // Index 0: local player; others: remote/spawned.
GameObject gServerEntityPool[MAX_GAMEOBJECTS];  // Temporary buffer for server updates.
std::atomic<uint32_t> gRemoteCount{ 0 };          // Number of remote entities.
std::mutex gPoolMutex;

// ----------------------------------------------------------------------
// Local Player Physics Variables (Client-Side Prediction)
// ----------------------------------------------------------------------
float playerPosX = 400.0f;
float playerPosY = 300.0f;
float playerAngle = 0.0f;           // Orientation in radians.
float playerVelX = 0.0f;
float playerVelY = 0.0f;
float playerAngularVelocity = 0.0f;
const float playerRenderScale = 100.0f;  // For rendering scale.

// AE-Engine mesh and textures
AEGfxVertexList* pMesh = nullptr;
AEGfxTexture* AsteroidTexture = nullptr;
AEGfxTexture* PlayerTexture = nullptr;
AEGfxTexture* BulletTexture = nullptr;

// ----------------------------------------------------------------------
// Helper: Linear Interpolation Function
// ----------------------------------------------------------------------
inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// ----------------------------------------------------------------------
// Receive Thread: Processes JOIN_ACCEPT, GAME_UPDATE packets.
// ----------------------------------------------------------------------
void ReceiveThread(SOCKET socket)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr{};
    int fromLen = sizeof(fromAddr);
    while (running)
    {
        int bytesReceived = recvfrom(socket, buffer, BUFFER_SIZE, 0,
            reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (bytesReceived == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                continue;
            std::cerr << "[ERROR] recvfrom failed: " << err << std::endl;
            continue;
        }
        if (bytesReceived <= 0)
            continue;

        uint8_t packetType = static_cast<uint8_t>(buffer[0]);
        if (packetType == JOIN_ACCEPT)
        {
            JoinAcceptPacket* pkt = reinterpret_cast<JoinAcceptPacket*>(buffer);
            myPlayerId = pkt->playerId;
            std::cout << "[JOINED] Assigned Player ID: " << myPlayerId << std::endl;
        }
        else if (packetType == GAME_UPDATE)
        {
            GameUpdatePacket* update = reinterpret_cast<GameUpdatePacket*>(buffer);
            uint32_t count = update->objectCount;
            if (count > 4000)
                count = 4000;

            std::lock_guard<std::mutex> lock(gPoolMutex);
            uint32_t remoteIndex = 0;
            for (uint32_t i = 0; i < count; i++)
            {
                const GameObjectData& src = update->objects[i];
                // Skip local player's update.
                if (src.objectType == 0 && src.playerId == myPlayerId)
                    continue;
                gServerEntityPool[remoteIndex].pos_x = src.pos_x;
                gServerEntityPool[remoteIndex].pos_y = src.pos_y;
                gServerEntityPool[remoteIndex].rotation = src.rotation;
                gServerEntityPool[remoteIndex].scale = src.scale;
                gServerEntityPool[remoteIndex].objectType = src.objectType;
                gServerEntityPool[remoteIndex].playerId = src.playerId;
                gServerEntityPool[remoteIndex].vel_x = src.vel_x;
                gServerEntityPool[remoteIndex].vel_y = src.vel_y;
                remoteIndex++;
            }
            gRemoteCount.store(remoteIndex);
        }
    }
}

// ----------------------------------------------------------------------
// UpdateLerpingAndPredict: Updates remote entities in the unified pool.
// ----------------------------------------------------------------------
void UpdateLerpingAndPredict(float dt)
{
    const float lerpFactor = 0.1f;
    const float posThreshold = 50.0f;
    const float extrapolationFactor = 1.0f; // Predict 1 second ahead.

    std::lock_guard<std::mutex> lock(gPoolMutex);
    uint32_t remoteCount = gRemoteCount.load();
    // Remote entities are stored in gEntityPool starting at index 1.
    for (uint32_t i = 0; i < remoteCount; i++)
    {
        float targetPosX = gServerEntityPool[i].pos_x + gServerEntityPool[i].vel_x * extrapolationFactor;
        float targetPosY = gServerEntityPool[i].pos_y + gServerEntityPool[i].vel_y * extrapolationFactor;
        float targetRot = gServerEntityPool[i].rotation;
        float targetScale = gServerEntityPool[i].scale;

        float diffX = fabs(targetPosX - gEntityPool[i + 1].pos_x);
        if (diffX > posThreshold)
            gEntityPool[i + 1].pos_x = targetPosX;
        else
            gEntityPool[i + 1].pos_x = Lerp(gEntityPool[i + 1].pos_x, targetPosX, lerpFactor);

        float diffY = fabs(targetPosY - gEntityPool[i + 1].pos_y);
        if (diffY > posThreshold)
            gEntityPool[i + 1].pos_y = targetPosY;
        else
            gEntityPool[i + 1].pos_y = Lerp(gEntityPool[i + 1].pos_y, targetPosY, lerpFactor);

        gEntityPool[i + 1].rotation = targetRot;
        gEntityPool[i + 1].scale = targetScale;
        gEntityPool[i + 1].objectType = gServerEntityPool[i].objectType;
        gEntityPool[i + 1].playerId = gServerEntityPool[i].playerId;
        gEntityPool[i + 1].vel_x = gServerEntityPool[i].vel_x;
        gEntityPool[i + 1].vel_y = gServerEntityPool[i].vel_y;
    }
}

// ----------------------------------------------------------------------
// SpawnBullet: Sends a bullet spawn packet and spawns the bullet locally.
// ----------------------------------------------------------------------
void SpawnBullet()
{
    // Define bullet properties.
    BulletSpawnPacket pkt{};
    pkt.playerId = myPlayerId;
    pkt.bulletType = 0;   // Standard bullet.
    // Spawn bullet at tip of player's ship (adjust as needed).
    pkt.pos_x = playerPosX;
    pkt.pos_y = playerPosY;
    float bulletSpeed = 500.0f;
    pkt.vel_x = cosf(playerAngle) * bulletSpeed;
    pkt.vel_y = sinf(playerAngle) * bulletSpeed;
    pkt.damage = 10.0f;

    // Send BULLET_SPAWN packet to server.
    int sentBytes = sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (sentBytes == SOCKET_ERROR) {
        std::cerr << "[ERROR] sendto BULLET_SPAWN failed: " << WSAGetLastError() << std::endl;
    }

    // Spawn bullet locally in the unified entity pool.
    std::lock_guard<std::mutex> lock(gPoolMutex);
    uint32_t newIndex = gRemoteCount.load() + 1; // After current remote objects.
    if (newIndex < MAX_GAMEOBJECTS + 1) {
        gEntityPool[newIndex].objectType = 2; // Bullet.
        gEntityPool[newIndex].playerId = myPlayerId;
        gEntityPool[newIndex].pos_x = pkt.pos_x;
        gEntityPool[newIndex].pos_y = pkt.pos_y;
        gEntityPool[newIndex].vel_x = pkt.vel_x;
        gEntityPool[newIndex].vel_y = pkt.vel_y;
        gEntityPool[newIndex].rotation = playerAngle;
        gEntityPool[newIndex].scale = 10.0f; // Arbitrary bullet size.
        // Increase remote count so this bullet is rendered.
        gRemoteCount++;
    }
}

// ----------------------------------------------------------------------
// UpdateLocalSimulation: Updates local simulation and the local player's entity.
// Also checks for bullet spawn input.
// ----------------------------------------------------------------------
void UpdateLocalSimulation(float dt)
{
    float thrustInput = 0.0f;
    float rotationInput = 0.0f;
    if (AEInputCheckCurr(AEVK_W))
        thrustInput = 1.0f;
    if (AEInputCheckCurr(AEVK_S))
        thrustInput = -1.0f;
    if (AEInputCheckCurr(AEVK_D))
        rotationInput = -1.0f;
    if (AEInputCheckCurr(AEVK_A))
        rotationInput = 1.0f;

    const float thrustForce = 150.0f;
    const float torqueForce = 7.0f;
    const float linearDamping = 0.7f;
    const float angularDamping = 0.95f;

    float accel = thrustForce * thrustInput;
    playerVelX += cosf(playerAngle) * accel * dt;
    playerVelY += sinf(playerAngle) * accel * dt;
    playerVelX *= (1.0f - linearDamping * dt);
    playerVelY *= (1.0f - linearDamping * dt);
    playerPosX += playerVelX * dt;
    playerPosY += playerVelY * dt;

    playerAngularVelocity *= (1.0f - angularDamping * dt);
    playerAngularVelocity += torqueForce * rotationInput * dt;
    playerAngle += playerAngularVelocity * dt;

    float windowWidth = 1600.0f;
    float windowHeight = 900.0f;
    if (playerPosX < -windowWidth / 2 - playerRenderScale)
        playerPosX += windowWidth + playerRenderScale * 2;
    else if (playerPosX > windowWidth / 2 + playerRenderScale)
        playerPosX -= windowWidth + playerRenderScale * 2;
    if (playerPosY < -windowHeight / 2 - playerRenderScale)
        playerPosY += windowHeight + playerRenderScale * 2;
    else if (playerPosY > windowHeight / 2 + playerRenderScale)
        playerPosY -= windowHeight + playerRenderScale * 2;

    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        gEntityPool[0].pos_x = playerPosX;
        gEntityPool[0].pos_y = playerPosY;
        gEntityPool[0].vel_x = playerVelX;
        gEntityPool[0].vel_y = playerVelY;
        gEntityPool[0].rotation = playerAngle;
        gEntityPool[0].scale = playerRenderScale;
        gEntityPool[0].objectType = 0; // Local player.
        gEntityPool[0].playerId = myPlayerId;
    }

    // Check for bullet spawn input (fire key, e.g., SPACE).
    if (AEInputCheckTriggered(AEVK_SPACE))
    {
        SpawnBullet();
    }
}

// ----------------------------------------------------------------------
// SendPlayerUpdate: Sends the local player's state to the server.
// ----------------------------------------------------------------------
void SendPlayerUpdate(int clientId)
{
    PlayerUpdatePacket pkt{};
    pkt.type = PLAYER_UPDATE;
    pkt.playerId = (myPlayerId != 0) ? myPlayerId : static_cast<uint32_t>(clientId);
    pkt.pos_x = playerPosX;
    pkt.pos_y = playerPosY;
    pkt.angle = playerAngle;
    pkt.vel_x = playerVelX;
    pkt.vel_y = playerVelY;

    int sentBytes = sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (sentBytes == SOCKET_ERROR)
    {
        std::cerr << "[ERROR] sendto PLAYER_UPDATE failed: " << WSAGetLastError() << std::endl;
    }
}

// ----------------------------------------------------------------------
// Render: Renders all entities from the unified entity pool.
// ----------------------------------------------------------------------
void Render()
{
    std::lock_guard<std::mutex> lock(gPoolMutex);
    uint32_t totalEntities = 1 + gRemoteCount.load(); // Index 0 (local) + remote.
    for (uint32_t i = 0; i < totalEntities; i++)
    {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gEntityPool[i].scale, gEntityPool[i].scale);
        AEMtx33Rot(&rotMtx, gEntityPool[i].rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gEntityPool[i].pos_x, gEntityPool[i].pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

        switch (gEntityPool[i].objectType)
        {
        case 0:
            AEGfxTextureSet(PlayerTexture, 0, 0);
            break;
        case 1:
            AEGfxTextureSet(AsteroidTexture, 0, 0);
            break;
        case 2:
            AEGfxTextureSet(BulletTexture, 0, 0);
            break;
        default:
            AEGfxTextureSet(AsteroidTexture, 0, 0);
            break;
        }
        AEGfxSetTransform(finalMtx.m);
        AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);
    }
}

// ----------------------------------------------------------------------
// Main Entry Point
// ----------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);

    AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, true, nullptr);
    AESysSetWindowTitle("Asteroid Shooter - Client");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed." << std::endl;
        return 1;
    }
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
    {
        std::cerr << "[ERROR] Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }
    u_long nonBlockingMode = 1;
    if (ioctlsocket(udpSocket, FIONBIO, &nonBlockingMode) != 0)
    {
        std::cerr << "[ERROR] ioctlsocket failed: " << WSAGetLastError() << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }

    int clientId = 0;
    std::cout << "Enter client ID (1-4): ";
    std::cin >> clientId;
    if (clientId < 1 || clientId > 4)
        clientId = 1;

    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(CLIENT_PORT_START + clientId - 1);
    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr)) == SOCKET_ERROR)
    {
        std::cerr << "[ERROR] Bind failed." << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    char serverIpStr[INET_ADDRSTRLEN];
    std::cout << "Enter server IP address: ";
    std::cin >> serverIpStr;
    if (inet_pton(AF_INET, serverIpStr, &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "[ERROR] Invalid server IP address." << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }
    serverAddr.sin_port = htons(SERVER_PORT);

    // Send join request.
    JoinRequestPacket joinReq{};
    sendto(udpSocket, reinterpret_cast<char*>(&joinReq), sizeof(joinReq), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    std::thread recvThread(ReceiveThread, udpSocket);

    AEInputShowCursor(1);

    // Pre-load mesh and textures.
    AEGfxMeshStart();
    AEGfxTriAdd(-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 1.0f,
        0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
        -0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
    AEGfxTriAdd(0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
        0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,
        -0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
    pMesh = AEGfxMeshEnd();

    AsteroidTexture = AEGfxTextureLoad("Assets/PlanetTexture.png");
    PlayerTexture = AEGfxTextureLoad("Assets/Player.png");
    BulletTexture = AEGfxTextureLoad("Assets/Fire.png");

    bool gGameRunning = true;
    while (gGameRunning)
    {
        AESysFrameStart();
        AEGfxSetBackgroundColor(0.f, 0.f, 0.f);
        AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToMultiply(1, 1, 1, 1);
        AEGfxSetColorToAdd(0, 0, 0, 0);

        float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());

        // Update local simulation and handle bullet spawn.
        UpdateLocalSimulation(dt);
        SendPlayerUpdate(clientId);
        // Update remote objects interpolation.
        UpdateLerpingAndPredict(dt);
        // Render all entities.
        Render();

        AESysFrameEnd();
        if (AEInputCheckTriggered(AEVK_ESCAPE) || !AESysDoesWindowExist())
            gGameRunning = false;
    }

    AEGfxMeshFree(pMesh);
    AEGfxTextureUnload(AsteroidTexture);
    AEGfxTextureUnload(PlayerTexture);
    AEGfxTextureUnload(BulletTexture);
    AESysExit();

    running = false;
    if (recvThread.joinable())
        recvThread.join();

    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
