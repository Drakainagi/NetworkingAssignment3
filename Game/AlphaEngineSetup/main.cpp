/* Start Header
*****************************************************************/
/*!
  \file   client.cpp
  \author Joshua Sim Yue Chen
  \brief  This file implements the client for the asteroid shooter.
          The client computes its own ship movement (position and rotation)
          using simplified physics (client-side prediction), sends a PLAYER_UPDATE
          packet to the server with the new state, receives game state updates from
          the server, and renders the game objects using the AE-Engine rendering pipeline.
          Received game object values are interpolated (lerped) for smooth transitions.
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
#include <map>

#pragma comment(lib, "ws2_32.lib")

constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_GAMEOBJECTS = 4000; // Maximum number of game objects expected
constexpr int MAX_PLAYERS = 4000; // Maximum number of Players expected

// Packet types (must match server definitions)
enum PacketType : uint8_t {
    JOIN_REQUEST = 0x01,
    JOIN_ACCEPT = 0x02,
    GAME_UPDATE = 0x03,
    PLAYER_UPDATE = 0x04,
    ACK = 0x05,
    // Score packets
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

// PLAYER_UPDATE packet: client sends its current state (position and angle).
struct PlayerUpdatePacket
{
    uint8_t type = PLAYER_UPDATE;
    uint32_t playerId;
    float pos_x;
    float pos_y;
    float angle; // Orientation (in radians)
};

struct GameObjectData
{
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
    float pos_x;
    float pos_y;
    float rotation;
    float scale;
};

struct GameUpdatePacket
{
    uint8_t type = GAME_UPDATE;
    uint32_t objectCount;
    GameObjectData objects[4000];
};

struct ScoreIncrementPacket {
    uint8_t type = SCORE_INCREMENT;
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

// Global networking variables
std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;
SOCKET udpSocket = INVALID_SOCKET;
sockaddr_in serverAddr{};

// Score variables
std::map<uint32_t, uint32_t> gScoreBoard;
std::mutex gScoreMutex;

                  
// ----------------------------------------------------------------------
// Object Pooling Setup (Double-buffered)
// ----------------------------------------------------------------------
struct GameObject
{
    AEMtx33 transform;
    float pos_x, pos_y;
    float scale;
    float rotation;
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
};
// Render pool holds currently displayed values.
GameObject gRenderPool[MAX_GAMEOBJECTS];
// Back pool holds the target values received from the server.
GameObject gBackPool[MAX_GAMEOBJECTS];
std::atomic<uint32_t> gGameObjectCount{ 0 };
std::mutex gPoolMutex;

// AE-Engine mesh and textures.
AEGfxVertexList* pMesh = nullptr;
AEGfxTexture* AsteroidTexture = nullptr;
AEGfxTexture* PlayerTexture = nullptr;
AEGfxTexture* BulletTexture = nullptr;
s8	pFont;

// ----------------------------------------------------------------------
// Player Physics Variables (Local Simulation)
// ----------------------------------------------------------------------
float playerPosX = 400.0f;
float playerPosY = 300.0f;
float playerAngle = 0.0f;           // Orientation in radians.
float playerVelX = 0.0f;
float playerVelY = 0.0f;
float playerAngularVelocity = 0.0f;
const float playerRenderScale = 100.0f;  // For rendering scale

// ----------------------------------------------------------------------
// Helper: Linear Interpolation Function
// ----------------------------------------------------------------------
inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// ----------------------------------------------------------------------
// Helper: Score Increment Function
// ----------------------------------------------------------------------
void ReportScoreUpdate(uint32_t playerId, uint32_t points)
{
    ScoreIncrementPacket pkt;
    pkt.type = SCORE_INCREMENT;
    pkt.playerId = playerId;
    pkt.increment = points;

    int sent = sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
        reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (sent == SOCKET_ERROR) {
        std::cerr << "[ERROR] sendto SCORE_INCREMENT failed: " << WSAGetLastError() << std::endl;
    }
}


// ----------------------------------------------------------------------
// Receive Thread
// Handles JOIN_ACCEPT and GAME_UPDATE packets.
void receiveThread(SOCKET udpSocket)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    while (running)
    {
        int bytes = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0,
            reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (bytes <= 0)
            continue;
        uint8_t packetType = buffer[0];
        if (packetType == JOIN_ACCEPT) {
            JoinAcceptPacket* pkt = reinterpret_cast<JoinAcceptPacket*>(buffer);
            myPlayerId = pkt->playerId;
            std::cout << "[JOINED] Assigned Player ID: " << myPlayerId << std::endl;
        }
        else if (packetType == GAME_UPDATE)
        {
            GameUpdatePacket* update = reinterpret_cast<GameUpdatePacket*>(buffer);
            uint32_t count = update->objectCount;
            if (count > MAX_GAMEOBJECTS)
                count = MAX_GAMEOBJECTS;
            // Copy received values into the back pool.
            for (uint32_t i = 0; i < count; ++i)
            {
                gBackPool[i].pos_x = update->objects[i].pos_x;
                gBackPool[i].pos_y = update->objects[i].pos_y;
                gBackPool[i].rotation = update->objects[i].rotation;
                gBackPool[i].scale = update->objects[i].scale;
                gBackPool[i].objectType = update->objects[i].objectType;
            }
            {
                std::lock_guard<std::mutex> lock(gPoolMutex);
                // Instead of immediately swapping, we now update via lerping.
                // (The actual lerp update is done in UpdateLerping().)
                gGameObjectCount.store(count);
            }
        }
        else if (packetType == SCORE_UPDATE)
        {
            ScoreUpdatePacket* pkt = reinterpret_cast<ScoreUpdatePacket*>(buffer);
            std::lock_guard<std::mutex> lock(gScoreMutex);
            gScoreBoard.clear();
            for (uint32_t i = 0; i < pkt->scoreCount; ++i) {
                gScoreBoard[pkt->scores[i].playerId] = pkt->scores[i].score;
            }

            // Optional: Console debug display
            std::cout << "\n== LIVE SCOREBOARD ==\n";
            for (const auto& pair : gScoreBoard) {
                std::cout << "Player " << pair.first << ": " << pair.second << "\n";
            }

            std::cout << "=====================\n";
        }
    }
}


// ----------------------------------------------------------------------
// UpdateLerping()
// This function interpolates the render pool values toward the target back pool values.
// ----------------------------------------------------------------------
void UpdateLerping(float dt)
{
    const float lerpFactor = 0.1f; // Adjust for smoother or snappier interpolation.
    std::lock_guard<std::mutex> lock(gPoolMutex);
    uint32_t count = gGameObjectCount.load();
    for (uint32_t i = 0; i < count; i++) {
        gRenderPool[i].pos_x = Lerp(gRenderPool[i].pos_x, gBackPool[i].pos_x, lerpFactor);
        gRenderPool[i].pos_y = Lerp(gRenderPool[i].pos_y, gBackPool[i].pos_y, lerpFactor);
        gRenderPool[i].rotation = Lerp(gRenderPool[i].rotation, gBackPool[i].rotation, lerpFactor);
        gRenderPool[i].scale = Lerp(gRenderPool[i].scale, gBackPool[i].scale, lerpFactor);
        // Object type can be directly assigned (it rarely changes).
        gRenderPool[i].objectType = gBackPool[i].objectType;
    }
}

// ----------------------------------------------------------------------
// UpdateLocalSimulation()
// Computes your own ship’s state using local physics.
void UpdateLocalSimulation(float dt)
{
    float thrustInput = 0.0f;
    float rotationInput = 0.0f;
    if (AEInputCheckCurr(AEVK_W)) thrustInput = 1.0f;
    if (AEInputCheckCurr(AEVK_S)) thrustInput = -1.0f;
    if (AEInputCheckCurr(AEVK_D)) rotationInput = -1.0f;
    if (AEInputCheckCurr(AEVK_A)) rotationInput = 1.0f;

    //Score increment
    if (AEInputCheckTriggered(AEVK_1)) {
        ReportScoreUpdate(myPlayerId, 10);  // +10 points test
    }

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

    //Text rendering
            // Text display for lose
    std::string Str = "Your Score: " + std::to_string(10);

    const char* WinTxt = Str.c_str();
    AEGfxGetPrintSize(pFont, WinTxt, 1.f, &w, &h);
    AEGfxPrint(pFont, WinTxt, -w / 2, -h / 2 + 0.2f, 1, 1, 1, 1, 1);

    Str = "Press R to try again!";
    WinTxt = Str.c_str();
    AEGfxGetPrintSize(pFont, WinTxt, 1.f, &w, &h);
    AEGfxPrint(pFont, WinTxt, -w / 2, (-h / 2) - 0.3f, 1, 1, 1, 1, 1);
}

// ----------------------------------------------------------------------
// SendPlayerUpdate()
// Sends your current ship state to the server.
void SendPlayerUpdate(int clientId)
{
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

// ----------------------------------------------------------------------
// Render()
// Draws all game objects from the render pool using AE-Engine.
void Render()
{
    uint32_t count;
    {
        std::lock_guard<std::mutex> lock(gPoolMutex);
        count = gGameObjectCount.load();
    }
    for (uint32_t i = 0; i < count; i++)
    {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, gRenderPool[i].scale, gRenderPool[i].scale);
        AEMtx33Rot(&rotMtx, gRenderPool[i].rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, gRenderPool[i].pos_x, gRenderPool[i].pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

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

    std::thread recvThread(receiveThread, udpSocket);

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
    while (gGameRunning) {
        AESysFrameStart();
        AEGfxSetBackgroundColor(0.f, 0.f, 0.f);
        AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(1.f);
        AEGfxSetColorToMultiply(1, 1, 1, 1);
        AEGfxSetColorToAdd(0, 0, 0, 0);

        float dt = static_cast<float>(AEFrameRateControllerGetFrameTime());
        // Update our own ship simulation and send update.
        UpdateLocalSimulation(dt);
        SendPlayerUpdate(clientId);
        // Lerp render pool toward target (back pool) updates.
        UpdateLerping(dt);
        // Render all game objects.
        Render();

        AESysFrameEnd();
        if (AEInputCheckTriggered(AEVK_ESCAPE) || !AESysDoesWindowExist())
            gGameRunning = false;
    }

    AEGfxMeshFree(pMesh);
    AEGfxTextureUnload(AsteroidTexture);
    AEGfxTextureUnload(PlayerTexture);
    AEGfxTextureUnload(BulletTexture);
    AEGfxDestroyFont(pFont); //Unload font
    AESysExit();

    running = false;
    recvThread.join();
    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
