#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include "Vector3.h"

struct GameObject
{
	enum GAMEOBJECT_TYPE
	{
		GO_NONE = 0,
		GO_BALL,
		GO_CUBE,
		GO_MISSILE, //missile
		GO_PULSEBULLET, //Repulsor
		GO_ASTEROID, //asteroid
		GO_SHIP, //player ship
		GO_GUARDIAN, //Ally
		GO_BULLET, //player bullet
		GO_TOTAL, //must be last
		GO_ENEMYSHIP, //enemy ship
		GO_ENEMYSHIP_BULLET, //enemy bullet
		GO_BOSS,
		GO_PLANET1,
		GO_PLANET2,
		GO_PLANET3,
		GO_PLANET4,
		GO_BLACKHOLE,
		GO_PORTAL,
		GO_POWERUP, //powerup item
	};
	GAMEOBJECT_TYPE type;
	Vector3 pos;
	Vector3 vel;
	Vector3 ShootingVel;
	Vector3 scale;
	float OriginalScale; //For Asteroid
	Vector3 dir;//direction.orientation
	Vector3 m_torque;
	float momentOfInertia;
	float angularVelocity; //in radians
	bool active;
	float mass;
	float angle;
	float bulletCooldown;
	float health;

	int PowerUpNum;//For PowerUps

	float BossDamage;//For boss
	float PhaseCooldown;

	GameObject* target;
	GameObject(GAMEOBJECT_TYPE typeValue = GO_BALL);
	~GameObject();
};

#endif