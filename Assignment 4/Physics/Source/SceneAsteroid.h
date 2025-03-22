#ifndef SCENE_ASTEROID_H
#define SCENE_ASTEROID_H

#include "SceneBase.h"
#include "GameObject.h"
#include "PlayerShip.h"
#include "Enemy.h"
#include "Bullet.h"
#include "CelestialBodies.h"
#include <vector>
#include <memory>

class SceneAsteroid : public SceneBase
{
public:
    SceneAsteroid();
    ~SceneAsteroid();

    // Core scene functions
    virtual void Init() override;  // Initialize scene objects
    virtual void Update(double dt) override;  // Update all game objects
    virtual void Render() override;  // Render all game objects
    virtual void Exit() override;  // Cleanup memory and reset state

    // Rendering function
    void RenderGO(GameObject* go);

    // Fetch an available GameObject from the pool
    std::shared_ptr<GameObject> FetchGO();

    // Get the nearest object to a given position
    std::shared_ptr<GameObject> FetchNearestOBJ(const Vector3& position);

    // Process controls input for scene
    void ProcessInput();

    // Multiplayer synchronization functions
    void SyncStateToClients();  //TODO: Sync game state to connected clients -> IF THIS IS ACTING AS SERVER TODO
    void SyncStateFromServer();  //TODO: Receive and apply updates from the server -> IF THIS IS ACTING AS CLIENT

private:
    // Object pooling: single vector for efficient memory reuse
    std::vector<std::shared_ptr<GameObject>> m_gameObjects;

    // Player management
    std::shared_ptr<PlayerShip> m_playerShip; //THIS WILL BE YOU
    Vector3 m_force;
    Vector3 m_playerShipStartPos;

    // World properties
    float m_worldWidth;
    float m_worldHeight;

    // Game state tracking
    int m_lives;
    int m_score;
    float m_timer;
    bool m_bossSpawned;
    bool m_gameStarted;
    bool m_gameEnded;

    // Spawn rates for various entities
    float m_enemySpawnRate;
    float m_asteroidSpawnRate;
    float m_celestialBodySpawnRate;

    // Object spawning functions
    void SpawnEnemy();
    void SpawnAsteroid();
    void SpawnCelestialBody();
    //void SpawnBoss();
};

#endif // SCENE_ASTEROID_H
