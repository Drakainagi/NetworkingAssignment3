#ifndef SCENE_ASTEROID_H
#define SCENE_ASTEROID_H

#include "GameObject.h"
#include <vector>
#include "SceneBase.h"
#include <cmath>

class SceneAsteroid : public SceneBase
{
	static const int MAX_SPEED = 1000; // 100
	static const int BULLET_SPEED = 150; // 50
	static const int MISSILE_SPEED = 60; // 20
	static const int MISSILE_POWER = 2; // 1

	std::vector<GameObject*> m_goAsteroid;
	std::vector<GameObject*> m_goBullet;
	std::vector<GameObject*> m_goShip;
	std::vector<GameObject*> m_goPowerUp;
	std::vector<GameObject*> m_goCelestialBodies;
	std::vector<GameObject*> m_goBossShips;

	float m_worldWidth;
	float m_worldHeight;
	float m_CameraLowestX;
	float m_CameraLowestY;
	float m_CameraHighestX;
	float m_CameraHighestY;
	int WorldBackGroundNumber;
	Vector3 BackgroundPos;
	Vector3 ParallaxLayer1;
	Vector3 ParallaxLayer2;
	Vector3 ParallaxLayer3;

	GameObject* m_ship;
	Vector3 m_force;
	Vector3 m_ShipStartPos;
	float m_speed;
	int m_objectCount;
	int m_lives;
	int m_score;
	int m_index;
	float m_timer;
	int m_WeaponChoice;
	bool m_WeaponUnlock1;
	bool m_WeaponUnlock2;
	bool m_WeaponUnlock3;
	bool m_WeaponUnlock4;

	float EnemySpawnRate;
	float AsteroidSpawnRate;
	float PowerUpSpawnRate;
	float CelestialBodySpawnRate;

	bool BossSpawned;

	bool StartGame;
	bool EndGame;
public:
	SceneAsteroid();
	~SceneAsteroid();

	virtual void Init();
	virtual void Update(double dt);
	virtual void Render();
	virtual void Exit();

	void RenderGO(GameObject *go);

	GameObject* FetchGO(std::vector<GameObject*> m_goList);
	GameObject* FetchNearestOBJ(const Vector3& position);
};

#endif