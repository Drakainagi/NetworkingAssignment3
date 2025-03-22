#ifndef BULLET_H
#define BULLET_H

#include "GameObject.h"

class Bullet : public GameObject
{
public:
    // Constructor & Destructor
    Bullet();
    virtual ~Bullet();

    // Bullet-specific properties.
    bool enemyBullet; // True if fired by an enemy, false if by an ally/player.
    float speed;      // Speed multiplier for bullet movement.

    // Override update to move bullet along its trajectory.
    virtual void update(float dt) override;

    // Synchronize bullet state over the network.
    virtual void syncData() override;
};

#endif // BULLET_H
