#ifndef GAME_H
#define GAME_H

#include "AEEngine.h"
#include <cstdlib>  // For rand()

// Maximum number of game objects
#define MAX_GAME_OBJECTS 4000

// Constants for player movement and bullet speed
const float MOVE_SPEED = 200.0f;     // pixels per second
const float ROTATE_SPEED = 2.5f;     // radians per second
const float BULLET_SPEED = 400.f;
const float ClientID = 1; // Temporary client id

// Object types used in the game
enum ObjectType {
    Player,
    Asteroid,
    Bullet
};

// Structure to represent a game object
struct GameObject
{
    AEMtx33 transform;
    float pos_x, pos_y;
    float scale;
    float rotation;    // in radians
    float velocity;
    ObjectType objectType;
    bool isActive;     // false if object is inactive (e.g., destroyed)
    int bulletOwner;   // Client who owns this bullet
};

// Axis-Aligned Bounding Box structure
struct AABB {
    float min_x, min_y;
    float max_x, max_y;
};

// Function declarations
void StartGame(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow);

// Returns the AABB for a given GameObject based on its position and scale.
AABB GetAABB(const GameObject& obj);

// Checks if two GameObjects are colliding (with an offset applied).
bool CheckAABBCollision(const GameObject& a, const GameObject& b, float offset);

// Adds a game object to the scene with the given parameters.
void AddObject(float x, float y, float scale = 100.0f, ObjectType object = Asteroid, float rotation = 0);

// Spawns a new asteroid at a random location.
void SpawnRandomAsteroid();

// Handles collision between an asteroid and the player.
void HandleAsteroidPlayerCollision(GameObject& asteroid, GameObject& player);

// Handles collision between an asteroid and a bullet.
void HandleAsteroidBulletCollision(GameObject& asteroid, GameObject& bullet);

// Global game object list and count.
extern GameObject gGameObjects[MAX_GAME_OBJECTS];
extern unsigned int gObjectCount;

#endif  // GAME_H
