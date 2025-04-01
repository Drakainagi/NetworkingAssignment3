/* Start Header
*****************************************************************/
/*!
  \file   client.cpp
  \author Joshua Sim Yue Chen
  \brief  This file implements the client for the asteroid shooter.
          The client computes its own ship movement (position and rotation)
          using simplified physics (inspired by your playership movement code),
          sends a PLAYER_UPDATE packet to the server with the new state, receives
          game state updates from the server, and renders the game objects using the
          AE-Engine rendering pipeline.
*/
/* End Header
*******************************************************************/

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

constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_GAMEOBJECTS = 4000; // Maximum number of game objects expected

// Packet types (must match server definitions)
enum PacketType : uint8_t {
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    // Use PLAYER_UPDATE instead of PLAYER_INPUT.
    PLAYER_UPDATE = 0x04,
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

// New packet type: client sends its current state (position and angle).
struct PlayerUpdatePacket {
    uint8_t type = PLAYER_UPDATE;
    uint32_t playerId;
    float pos_x;
    float pos_y;
    float angle; // Orientation (in radians)
};

struct GameObjectData {
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
    float pos_x;
    float pos_y;
    float rotation;
    float scale;
};

struct GameUpdatePacket {
    uint8_t type = GAME_UPDATE;
    uint32_t objectCount;
    // This packet is built dynamically; the following fixed array is legacy.
    GameObjectData objects[4000];
};
#pragma pack(pop)

// Global networking variables
std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;
SOCKET udpSocket = INVALID_SOCKET;
sockaddr_in serverAddr{};

// ----------------------------------------------------------------------
// Object Pooling Setup
// Instead of a vector that we clear every update, we use two fixed arrays
// (double-buffered) to store the game objects.
struct GameObject {
    AEMtx33 transform;
    float pos_x, pos_y;
    float scale;
    float rotation;
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
};
// The buffers for game objects.
GameObject gRenderPool[MAX_GAMEOBJECTS];
GameObject gBackPool[MAX_GAMEOBJECTS];
// Count of valid objects in the pool.
std::atomic<uint32_t> gGameObjectCount{ 0 };
// Mutex to guard swapping buffers.
std::mutex gPoolMutex;

// AE-Engine mesh and textures.
AEGfxVertexList* pMesh = nullptr;
AEGfxTexture* AsteroidTexture = nullptr;
AEGfxTexture* PlayerTexture = nullptr;
AEGfxTexture* BulletTexture = nullptr;

// ----------------------------------------------------------------------
// ----- Player Physics Variables -----
// These simulate the ship movement locally.
float playerPosX = 400.0f;
float playerPosY = 300.0f;
float playerAngle = 0.0f;           // Orientation in radians.
float playerVelX = 0.0f;
float playerVelY = 0.0f;
float playerAngularVelocity = 0.0f;

// ----- Receive Thread -----
// Handles JOIN_ACCEPT and GAME_UPDATE packets.
void receiveThread(SOCKET udpSocket)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    while (running) {
        int bytes = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (bytes <= 0)
            continue;
        uint8_t packetType = buffer[0];
        if (packetType == JOIN_ACCEPT) {
            JoinAcceptPacket* pkt = reinterpret_cast<JoinAcceptPacket*>(buffer);
            myPlayerId = pkt->playerId;
            std::cout << "[JOINED] Assigned Player ID: " << myPlayerId << std::endl;
        }
        else if (packetType == GAME_UPDATE) {
            GameUpdatePacket* update = reinterpret_cast<GameUpdatePacket*>(buffer);
            uint32_t count = update->objectCount;
            if (count > MAX_GAMEOBJECTS)
                count = MAX_GAMEOBJECTS;
            // Write into the back pool.
            for (uint32_t i = 0; i < count; ++i) {
                gBackPool[i].pos_x = update->objects[i].pos_x;
                gBackPool[i].pos_y = update->objects[i].pos_y;
                gBackPool[i].rotation = update->objects[i].rotation;
                gBackPool[i].scale = update->objects[i].scale;
                gBackPool[i].objectType = update->objects[i].objectType;
            }
            // Atomically update the object count and swap the buffers.
            {
                std::lock_guard<std::mutex> lock(gPoolMutex);
                // Swap back pool into render pool.
                memcpy(gRenderPool, gBackPool, sizeof(GameObject) * count);
                gGameObjectCount.store(count);
            }
        }
    }
}

// ----- Update Function -----
// Computes the ship movement locally (physics) and sends the new state to the server.
void Update(float dt, int clientId)
{
    // --- Physics simulation based on key input ---
    // Thrust and rotation inputs.
    float thrustInput = 0.0f;
    float rotationInput = 0.0f;
    if (AEInputCheckCurr(AEVK_W)) thrustInput = 1.0f;    // Thrust forward.
    if (AEInputCheckCurr(AEVK_S)) thrustInput = -1.0f;   // Reverse thrust.
    if (AEInputCheckCurr(AEVK_D)) rotationInput = -1.0f; // Rotate left.
    if (AEInputCheckCurr(AEVK_A)) rotationInput = 1.0f;  // Rotate right.

    // Adjusted constants for tighter controls.
    const float thrustForce = 150.0f;      // Lowered acceleration for more precision.
    const float torqueForce = 7.0f;        // Slightly increased for snappier rotation.
    const float linearDamping = 0.7f;      // Damping applied to linear velocity.
    const float angularDamping = 0.95f;    // Damping for rotation.

    // Update linear velocity and position.
    float accel = thrustForce * thrustInput;
    // Compute acceleration in the direction of the ship’s angle.
    playerVelX += cosf(playerAngle) * accel * dt;
    playerVelY += sinf(playerAngle) * accel * dt;
    // Apply linear damping to reduce drift.
    playerVelX *= (1.0f - linearDamping * dt);
    playerVelY *= (1.0f - linearDamping * dt);
    playerPosX += playerVelX * dt;
    playerPosY += playerVelY * dt;

    // Update angular velocity and angle.
    playerAngularVelocity *= (1.0f - angularDamping * dt);
    playerAngularVelocity += torqueForce * rotationInput * dt;
    playerAngle += playerAngularVelocity * dt;

    // Wrap-around logic (assumes world size of 1600x900).
    // --- Wrap-around Logic using Dynamic Window Size ---
    float windowWidth = 1600.0f;
    float windowHeight = 900.0f;
    if (playerPosX < -windowWidth/2)
        playerPosX += windowWidth;
    else if (playerPosX > windowWidth / 2)
        playerPosX -= windowWidth;
    if (playerPosY < -windowHeight/2)
        playerPosY += windowHeight;
    else if (playerPosY > windowHeight / 2)
        playerPosY -= windowHeight;


    // ----- Send Updated State to the Server -----
    PlayerUpdatePacket pkt;
    pkt.type = PLAYER_UPDATE;
    pkt.playerId = (myPlayerId != 0) ? myPlayerId : clientId;
    pkt.pos_x = playerPosX;
    pkt.pos_y = playerPosY;
    pkt.angle = playerAngle;
    int sentBytes = sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (sentBytes == SOCKET_ERROR) {
        std::cerr << "[ERROR] sendto PLAYER_UPDATE failed: " << WSAGetLastError() << std::endl;
    }
}

// ----- Render Function -----
// Draws game objects using AE-Engine.
void Render()
{
    // Lock the pool for a consistent snapshot.
    uint32_t count;
    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        count = gGameObjectCount.load();
    }
    for (uint32_t i = 0; i < count; i++) {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gRenderPool[i].scale, gRenderPool[i].scale);
        // Adjust rotation so that sprite orientation is correct.
        AEMtx33Rot(&rotMtx, gRenderPool[i].rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gRenderPool[i].pos_x, gRenderPool[i].pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

        // Select texture based on object type.
        switch (gRenderPool[i].objectType) {
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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Allocate console for debugging.
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);

    // Initialize AE-Engine.
    AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, true, NULL);
    AESysSetWindowTitle("Asteroid Shooter - Client");

    // Initialize Winsock.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        WSACleanup();
        return 1;
    }
    // Bind to a client-specific port.
    int clientId = 0;
    std::cout << "Enter client ID (1-4): ";
    std::cin >> clientId;
    if (clientId < 1 || clientId > 4)
        clientId = 1;
    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(CLIENT_PORT_START + clientId - 1);
    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return 1;
    }
    // Set up server address.
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    char serverIpStr[INET_ADDRSTRLEN];
    std::cout << "Enter server IP address: ";
    std::cin >> serverIpStr;
    inet_pton(AF_INET, serverIpStr, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);

    // Send a join request.
    JoinRequestPacket join{};
    sendto(udpSocket, reinterpret_cast<char*>(&join), sizeof(join), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    // Start the receive thread.
    std::thread recvThread(receiveThread, udpSocket);

    // Hide the mouse cursor.
    AEInputShowCursor(1);

    // Pre-load mesh and textures for rendering.
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

    // Main update-render loop.
    bool gGameRunning = true;
    while (gGameRunning) {
        AESysFrameStart();
        AEGfxSetBackgroundColor(0.f, 0.f, 0.f);
        AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToMultiply(1, 1, 1, 1);
        AEGfxSetColorToAdd(0, 0, 0, 0);

        float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());
        Update(dt, clientId);
        Render();

        AESysFrameEnd();
        if (AEInputCheckTriggered(AEVK_ESCAPE) || !AESysDoesWindowExist())
            gGameRunning = false;
    }

    // Cleanup resources.
    AEGfxMeshFree(pMesh);
    AEGfxTextureUnload(AsteroidTexture);
    AEGfxTextureUnload(PlayerTexture);
    AEGfxTextureUnload(BulletTexture);
    AESysExit();

    running = false;
    recvThread.join();
    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
