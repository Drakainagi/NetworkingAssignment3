#include "Bullet.h"

Bullet::Bullet()
    : GameObject(),
    enemyBullet(false),
    speed(10.0f)
{
    // Bullets start inactive until fired.
    active = false;
}

Bullet::~Bullet()
{
}

void Bullet::update(float dt) {
    // For a bullet, assume that vel encodes its forward direction.
    // Move the bullet by its speed along the velocity direction.
    vel = vel * speed;
    GameObject::update(dt);
}

void Bullet::syncData() {
    // Implement network sync for bullet properties (e.g., pos, enemyBullet flag).
}
