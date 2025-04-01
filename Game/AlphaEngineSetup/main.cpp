/* Start Header
*****************************************************************/
/*!
  \file   client.cpp
  \author Joshua Sim Yue Chen
  \brief  This file implements the client for the asteroid shooter.
          The client sends player input to the server, receives game state updates,
          and renders the game objects using the AE-Engine rendering pipeline.
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

#pragma comment(lib, "ws2_32.lib")

constexpr uint16_t SERVER_PORT = 9000;
constexpr int CLIENT_PORT_START = 9001;
constexpr int BUFFER_SIZE = 1024;

// Packet types (must match server definitions)
enum PacketType : uint8_t {
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
    GameObjectData objects[4000];
};
#pragma pack(pop)

// Global networking variables
std::atomic<bool> running{ true };
uint32_t myPlayerId = 0;
SOCKET udpSocket = INVALID_SOCKET;
sockaddr_in serverAddr{};

// Local game object list for rendering
struct GameObject {
    AEMtx33 transform;
    float pos_x, pos_y;
    float scale;
    float rotation;
    uint8_t objectType; // 0=Player, 1=Asteroid, 2=Bullet
};

std::vector<GameObject> gGameObjects;

// Textures and mesh used for rendering.
AEGfxVertexList* pMesh = nullptr;
AEGfxTexture* AsteroidTexture = nullptr;
AEGfxTexture* PlayerTexture = nullptr;
AEGfxTexture* BulletTexture = nullptr;

// Receive thread: handles JOIN_ACCEPT and GAME_UPDATE packets.
void receiveThread(SOCKET udpSocket)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    while (running) {
        int bytes = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (bytes <= 0) continue;
        uint8_t packetType = buffer[0];
        if (packetType == JOIN_ACCEPT) {
            JoinAcceptPacket* pkt = reinterpret_cast<JoinAcceptPacket*>(buffer);
            myPlayerId = pkt->playerId;
            std::cout << "[JOINED] Assigned Player ID: " << myPlayerId << std::endl;
        }
        else if (packetType == GAME_UPDATE) {
            GameUpdatePacket* update = reinterpret_cast<GameUpdatePacket*>(buffer);
            // Update the local game object list.
            gGameObjects.clear();
            for (uint32_t i = 0; i < update->objectCount; ++i) {
                GameObjectData& data = update->objects[i];
                GameObject obj;
                obj.pos_x = data.pos_x;
                obj.pos_y = data.pos_y;
                obj.rotation = data.rotation;
                obj.scale = data.scale;
                obj.objectType = data.objectType;
                gGameObjects.push_back(obj);
            }
        }
    }
}

// Update function: handles input and sends player input to server.
void Update(float dt, int clientId)
{
    static float inputSendCooldown = 0.05f;
    static float inputTimer = 0.0f;
    inputTimer += dt;
    if (inputTimer >= inputSendCooldown) {
        float dx = 0.0f, dy = 0.0f;
        if (AEInputCheckCurr(AEVK_W)) dy = 1.0f;
        if (AEInputCheckCurr(AEVK_S)) dy = -1.0f;
        if (AEInputCheckCurr(AEVK_A)) dx = -1.0f;
        if (AEInputCheckCurr(AEVK_D)) dx = 1.0f;
        if (dx != 0.0f || dy != 0.0f) {
            PlayerInputPacket pkt;
            pkt.type = PLAYER_INPUT;
            // Use the JOIN_ACCEPT assigned id if available; else use local client id.
            pkt.playerId = (myPlayerId != 0) ? myPlayerId : clientId;
            pkt.moveX = dx;
            pkt.moveY = dy;
            sendto(udpSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
                reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
        }
        inputTimer = 0.0f;
    }
}

// Render function: draws game objects using AE-Engine.
void Render()
{
    // Render each game object in the list.
    for (const auto& obj : gGameObjects) {
        AEMtx33 scaleMtx, rotMtx, transMtx, finalMtx;
        AEMtx33Scale(&scaleMtx, obj.scale, obj.scale);
        // Adjust rotation so that sprite orientation is correct.
        AEMtx33Rot(&rotMtx, obj.rotation + (3.1415926f / 2.0f));
        AEMtx33Trans(&transMtx, obj.pos_x, obj.pos_y);
        AEMtx33Concat(&finalMtx, &rotMtx, &scaleMtx);
        AEMtx33Concat(&finalMtx, &transMtx, &finalMtx);

        // Select texture based on object type.
        switch (obj.objectType) {
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
    if (clientId < 1 || clientId > 4) clientId = 1;
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

    // Cleanup.
    AEGfxMeshFree(pMesh);
    AEGfxTextureUnload(AsteroidTexture);
    AEGfxTextureUnload(PlayerTexture);
    AESysExit();

    running = false;
    recvThread.join();
    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
