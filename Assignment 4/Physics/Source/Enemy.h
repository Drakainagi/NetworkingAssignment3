#ifndef ENEMY_H
#define ENEMY_H

#include "GameObject.h"
class Enemy : public GameObject {
public:
    // Constructor & Destructor
    Enemy();
    virtual ~Enemy();

    // Enemy-specific properties.
    // Use class variables instead of enums to determine behavior.
    bool isBoss;    // true if this enemy is a boss
    float damage;   // Damage dealt by the enemy

    // Override update to implement enemy AI behavior.
    virtual void update(float dt) override;

    // Implement syncData to synchronize enemy state over the network.
    virtual void syncData() override;
};

#endif // ENEMY_H
