#include "Enemy.h"

Enemy::Enemy()
    : GameObject(),
    isBoss(false),
    damage(10.0f)
{
    active = true;
}

Enemy::~Enemy()
{
}

void Enemy::update(float dt) {
    // Insert enemy AI logic here, such as movement, target tracking, or state changes.
    GameObject::update(dt);
    // Additional enemy-specific update code can go here.
}

void Enemy::syncData() {
    // Implement network sync for enemy properties (e.g., pos, health, isBoss).
}
