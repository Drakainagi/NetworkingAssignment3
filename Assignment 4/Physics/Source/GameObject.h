#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "Vector3.h"
#include <MatrixStack.h>

enum GAMEOBJECT_TYPE
{
	GO_NONE = 0,
	GO_BALL,
	GO_CUBE,
	GO_MISSILE, //missile
	GO_PULSEBULLET, //Repulsor
	GO_ASTEROID, //asteroid
	GO_SHIP, //player ship
	GO_BULLET, //player bullet
	GO_ENEMYSHIP, //enemy ship
	GO_ENEMYSHIP_BULLET, //enemy bullet
	GO_BOSS,
	GO_PLANET,
	GO_BLACKHOLE,
};

class GameObject {
public:
    // Constructor & virtual destructor
    GameObject();
    virtual ~GameObject();

    // Common properties shared among all game objects.
    Vector3 pos;
    Vector3 vel;
    Vector3 scale;
    float angle;    // Orientation (radians)
    float mass;
    float health;
    bool active;
    GAMEOBJECT_TYPE type;

    // Update the object logic. dt is the time delta.
    virtual void update(float dt);

    // Sync object data (for multiplayer or networked play).
    virtual void syncData();  // Pure virtual to enforce implementation
};

#endif // GAMEOBJECT_H
