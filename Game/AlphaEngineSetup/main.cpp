/*
 *****************************************************************
 *  File:      client.cpp
 *  Author:    Joshua Sim Yue Chen (modified by ChatGPT)
 *  Brief:     Implements the client for the asteroid shooter.
 *             The client computes its own ship movement using
 *             simplified physics (client-side prediction) and
 *             sends PLAYER_UPDATE packets (including velocity).
 *             It also receives GAME_UPDATE packets containing
 *             all game objects. We now separate local objects
 *             (local player, bullets, etc.) from remote objects.
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
#include <string>

#pragma comment(lib, "ws2_32.lib")

 // Constants
constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;
constexpr int BUFFER_SIZE = 4096;
constexpr int MAX_REMOTE_OBJECTS = 256;  // Objects coming from the server.
constexpr int MAX_LOCAL_ENTITIES = 4000;     // Local pool for bullets, power-ups, etc.
constexpr int MAX_LOCAL_ENTITIES_SPAWN_RATE = 10; 
bool gGameRunning{};

#pragma region Helper Func
 // ----------------------------------------------------------------------
 // Helper: Linear Interpolation Function
 // ----------------------------------------------------------------------
inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}
#pragma endregion

#pragma region Packets Declaration
// Packet types (must match server definitions)
enum PacketType : uint8_t
{
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_UPDATE = 0x04,
    ACK = 0x05,
    BULLET_SPAWN = 0x06,  // New packet type for bullet spawning.
    DISCONNECT = 0x07,
    NAME_REJECTED = 0x08   // ✅ new packet type
};

#pragma pack(push, 1)

// Packet for join request.
struct JoinRequestPacket {
    uint8_t type = JOIN_REQUEST;
    char name[32];  // Max 31 characters + null terminator
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

struct BulletSpawnMultiPacket {
    uint8_t type;         // This will be BULLET_SPAWN or a new type if you prefer.
    uint32_t count;       // Number of bullets being sent.
    // Then an array of bullet data. For simplicity, we assume a fixed maximum.
    BulletSpawnPacket bullets[MAX_LOCAL_ENTITIES_SPAWN_RATE];  // Adjust size as necessary.
};

// Game object data received from the server (for remote objects).
struct GameObjectData {
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet, etc.
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
    GameObjectData objects[MAX_REMOTE_OBJECTS];
};

#pragma pack(pop)
#pragma endregion

#pragma region VARIABLES DECLARATION

// ----------------------------------------------------------------------
// Global Networking Variables
// ----------------------------------------------------------------------
std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;
SOCKET udpSocket = INVALID_SOCKET;
sockaddr_in serverAddr{};

// ----------------------------------------------------------------------
// Entity Structures & Pools
// ----------------------------------------------------------------------
struct GameObject 
{
    AEMtx33 transform;  // For rendering.
    float pos_x = 0.0f, pos_y = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f;
    float scale = 1.0f;
    float rotation = 0.0f;
    uint8_t objectType = 0; // 0=Player, 1=Asteroid, 2=Bullet, etc.
    uint32_t playerId = 0;  // Valid for player objects.

    bool isSent = false;
};

// Remote pool: all entities updated from the server.
GameObject gRemoteEntities[MAX_REMOTE_OBJECTS];
GameObject gServerEntityPool[MAX_REMOTE_OBJECTS]; // Temporary buffer from server.
std::atomic<uint32_t> gRemoteCount{ 0 };

// Local pool: stores local player and other local spawned objects.
GameObject gLocalPlayer; // Local player object.
GameObject gLocalEntities[MAX_LOCAL_ENTITIES];
uint32_t gLocalEntityCount = 0;

// Mutex for synchronizing access to both pools.
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

#pragma endregion

#pragma region Local Spawning
// ----------------------------------------------------------------------
// SpawnLocalEntity: Spawns a new local entity (e.g. bullet, power-up).
// ----------------------------------------------------------------------
void SpawnLocalEntity(uint8_t objectType, float pos_x, float pos_y, float vel_x, float vel_y, float rotation, float scale)
{
    std::lock_guard<std::mutex> lock(gPoolMutex);
    if (gLocalEntityCount >= MAX_LOCAL_ENTITIES)
        return; // Pool full.

    GameObject& obj = gLocalEntities[gLocalEntityCount++];
    obj.objectType = objectType;
    obj.playerId = myPlayerId;  // For example, associate with the local player.
    obj.pos_x = pos_x;
    obj.pos_y = pos_y;
    obj.vel_x = vel_x;
    obj.vel_y = vel_y;
    obj.rotation = rotation;
    obj.scale = scale;
}

// ----------------------------------------------------------------------
// SpawnBullet: Sends a bullet spawn packet and spawns the bullet locally.
// ----------------------------------------------------------------------
void SpawnBullet()
{
    // Prepare packet data.
    BulletSpawnPacket pkt{};
    pkt.playerId = myPlayerId;
    pkt.bulletType = 0;   // Standard bullet.
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

    // Spawn bullet locally in the local pool.
    SpawnLocalEntity(2, pkt.pos_x, pkt.pos_y, pkt.vel_x, pkt.vel_y, playerAngle, 100.0f);
}

#pragma endregion

#pragma region Update Logic
// ----------------------------------------------------------------------
// UpdateLocalSimulation: Updates local simulation (player and local entities).
// ----------------------------------------------------------------------
void UpdateLocalSimulation(float dt)
{
    // Process input for player movement.
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

    // Update local player.
    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        gLocalPlayer.pos_x = playerPosX;
        gLocalPlayer.pos_y = playerPosY;
        gLocalPlayer.vel_x = playerVelX;
        gLocalPlayer.vel_y = playerVelY;
        gLocalPlayer.rotation = playerAngle;
        gLocalPlayer.scale = playerRenderScale;
        gLocalPlayer.objectType = 0; // Local player.
        gLocalPlayer.playerId = myPlayerId;
    }

    // Update local entities (e.g., bullets) in the local pool.
    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        for (uint32_t i = 0; i < gLocalEntityCount; i++)
        {
            gLocalEntities[i].pos_x += gLocalEntities[i].vel_x * dt;
            gLocalEntities[i].pos_y += gLocalEntities[i].vel_y * dt;
        }
    }

    // Check for bullet spawn input (fire key, e.g., SPACE).
    if (AEInputCheckTriggered(AEVK_SPACE))
    {
        SpawnBullet();
    }

    // Here you could add more input-handling for other local-spawnable entities.
}
#pragma endregion

#pragma region Client Interaction
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
            if (count > MAX_REMOTE_OBJECTS)
                count = MAX_REMOTE_OBJECTS;

            std::lock_guard<std::mutex> lock(gPoolMutex);
            uint32_t remoteIndex = 0;
            for (uint32_t i = 0; i < count; i++)
            {
                const GameObjectData& src = update->objects[i];
                // Skip local player's update.
                if (src.objectType == 0 && src.playerId == myPlayerId)
                    continue;
                // Skip bullet updates from local player; local bullets are managed locally.
                if (src.objectType == 2 && src.playerId == myPlayerId)
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
        if (packetType == NAME_REJECTED) 
        {
            std::cout << "[SERVER] Name already taken. Please restart and try a different name.\n";
            running = false; // Optional: stop game loop
            gGameRunning = false;
            continue;
        }
    }
}

void SendLocalUpdate(int clientId)
{
#pragma region Sending Player Packet
    // First, send the player's update (as before).
    PlayerUpdatePacket playerPkt{};
    playerPkt.type = PLAYER_UPDATE;
    playerPkt.playerId = (myPlayerId != 0) ? myPlayerId : static_cast<uint32_t>(clientId);
    playerPkt.pos_x = playerPosX;
    playerPkt.pos_y = playerPosY;
    playerPkt.angle = playerAngle;
    playerPkt.vel_x = playerVelX;
    playerPkt.vel_y = playerVelY;

    int sentBytes = sendto(udpSocket, reinterpret_cast<char*>(&playerPkt), sizeof(playerPkt), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (sentBytes == SOCKET_ERROR)
    {
        std::cerr << "[ERROR] sendto PLAYER_UPDATE failed: " << WSAGetLastError() << std::endl;
    }
#pragma endregion

#pragma region Sending Bullet Packet
    //////////////////////////////////////////////////////////////////////////////////////
    // Sending Bullets
    //////////////////////////////////////////////////////////////////////////////////////
    BulletSpawnMultiPacket multiPkt{};
    multiPkt.type = BULLET_SPAWN; // Using the same type, or define a new one.
    multiPkt.count = 0;
    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        for (uint32_t i = 0; i < gLocalEntityCount && multiPkt.count < 10; i++)
        {
            if (gLocalEntities[i].objectType == 2 && !gLocalEntities[i].isSent)
            {
                // Prepare the bullet spawn data.
                multiPkt.bullets[multiPkt.count].playerId = myPlayerId;
                multiPkt.bullets[multiPkt.count].bulletType = 0; // Standard bullet.
                multiPkt.bullets[multiPkt.count].pos_x = gLocalEntities[i].pos_x;
                multiPkt.bullets[multiPkt.count].pos_y = gLocalEntities[i].pos_y;
                // Assuming vel_x and vel_y are already set in the local entity.
                multiPkt.bullets[multiPkt.count].vel_x = gLocalEntities[i].vel_x;
                multiPkt.bullets[multiPkt.count].vel_y = gLocalEntities[i].vel_y;
                multiPkt.bullets[multiPkt.count].damage = 10.0f;
                multiPkt.count++;

                // Optionally, mark the bullet as already sent.
                gLocalEntities[i].isSent = true;
            }
        }
    }

    // Only send the bullet packet if there is at least one bullet.
    if (multiPkt.count > 0)
    {
        sentBytes = sendto(udpSocket, reinterpret_cast<char*>(&multiPkt),
            sizeof(multiPkt.type) + sizeof(multiPkt.count) + multiPkt.count * sizeof(BulletSpawnPacket),
            0, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
        if (sentBytes == SOCKET_ERROR)
        {
            std::cerr << "[ERROR] sendto BULLET_SPAWN multi packet failed: " << WSAGetLastError() << std::endl;
        }
    }
#pragma endregion
}

#pragma endregion

#pragma region Render
// ----------------------------------------------------------------------
// UpdateRemoteInterpolation: Interpolates remote objects based on server data.
// ----------------------------------------------------------------------
void UpdateRemoteInterpolation(float dt)
{
    const float lerpFactor = 0.1f;
    const float posThreshold = 50.0f;
    const float extrapolationFactor = 1.0f; // Predict 1 second ahead.

    std::lock_guard<std::mutex> lock(gPoolMutex);
    uint32_t remoteCount = gRemoteCount.load();
    for (uint32_t i = 0; i < remoteCount; i++)
    {
        float targetPosX = gServerEntityPool[i].pos_x + gServerEntityPool[i].vel_x * extrapolationFactor;
        float targetPosY = gServerEntityPool[i].pos_y + gServerEntityPool[i].vel_y * extrapolationFactor;
        float targetRot = gServerEntityPool[i].rotation;
        float targetScale = gServerEntityPool[i].scale;

        float diffX = fabs(targetPosX - gRemoteEntities[i].pos_x);
        if (diffX > posThreshold)
            gRemoteEntities[i].pos_x = targetPosX;
        else
            gRemoteEntities[i].pos_x = Lerp(gRemoteEntities[i].pos_x, targetPosX, lerpFactor);

        float diffY = fabs(targetPosY - gRemoteEntities[i].pos_y);
        if (diffY > posThreshold)
            gRemoteEntities[i].pos_y = targetPosY;
        else
            gRemoteEntities[i].pos_y = Lerp(gRemoteEntities[i].pos_y, targetPosY, lerpFactor);

        gRemoteEntities[i].rotation = targetRot;
        gRemoteEntities[i].scale = targetScale;
        gRemoteEntities[i].objectType = gServerEntityPool[i].objectType;
        gRemoteEntities[i].playerId = gServerEntityPool[i].playerId;
        gRemoteEntities[i].vel_x = gServerEntityPool[i].vel_x;
        gRemoteEntities[i].vel_y = gServerEntityPool[i].vel_y;
    }
}

// ----------------------------------------------------------------------
// Render: Renders local player, local entities, and remote entities.
// ----------------------------------------------------------------------
void Render()
{
    std::lock_guard<std::mutex> lock(gPoolMutex);

    // Render local player.
    {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gLocalPlayer.scale, gLocalPlayer.scale);
        AEMtx33Rot(&rotMtx, gLocalPlayer.rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gLocalPlayer.pos_x, gLocalPlayer.pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);
        AEGfxTextureSet(PlayerTexture, 0, 0);
        AEGfxSetTransform(finalMtx.m);
        AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);
    }

    // Render local entities (bullets, etc.).
    for (uint32_t i = 0; i < gLocalEntityCount; i++)
    {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gLocalEntities[i].scale, gLocalEntities[i].scale);
        AEMtx33Rot(&rotMtx, gLocalEntities[i].rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gLocalEntities[i].pos_x, gLocalEntities[i].pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

        // Choose texture based on object type.
        if (gLocalEntities[i].objectType == 2) // Bullet.
            AEGfxTextureSet(BulletTexture, 0, 0);
        else
            AEGfxTextureSet(AsteroidTexture, 0, 0); // Or any other texture for other entity types.
        AEGfxSetTransform(finalMtx.m);
        AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);
    }

    // Render remote entities.
    uint32_t remoteCount = gRemoteCount.load();
    for (uint32_t i = 0; i < remoteCount; i++)
    {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gRemoteEntities[i].scale, gRemoteEntities[i].scale);
        AEMtx33Rot(&rotMtx, gRemoteEntities[i].rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gRemoteEntities[i].pos_x, gRemoteEntities[i].pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

        switch (gRemoteEntities[i].objectType)
        {
        case 1:
            AEGfxTextureSet(AsteroidTexture, 0, 0);
            break;
        case 2:
            AEGfxTextureSet(BulletTexture, 0, 0);
            break;
        default:
            AEGfxTextureSet(PlayerTexture, 0, 0);
            break;
        }
        AEGfxSetTransform(finalMtx.m);
        AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);
    }
}
#pragma endregion

#pragma region DO NOT TOUCH
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

    std::string userName;
    std::cout << "Enter your name: ";
    std::getline(std::cin, userName);


    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = 0; // ✅ No more clientId usage

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
    strcpy_s(joinReq.name, sizeof(joinReq.name), userName.c_str());
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

    gGameRunning = true;
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

        // Update local simulation (local player and local entities).
        UpdateLocalSimulation(dt);
        SendLocalUpdate(myPlayerId);
        // Interpolate remote entities.
        UpdateRemoteInterpolation(dt);
        // Render local and remote entities.
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

    if (myPlayerId != 0) {
        uint8_t disconnectMsg[5];
        disconnectMsg[0] = DISCONNECT;
        uint32_t netId = htonl(myPlayerId);
        memcpy(&disconnectMsg[1], &netId, sizeof(uint32_t));
        sendto(udpSocket, reinterpret_cast<char*>(disconnectMsg), 5, 0,
            (sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
#pragma endregion