#include "Bullet.h"
#include "Application.h"
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

    // Deactivate if too far. - Can delete on own computer naturally
    if ((pos - Vector3(Application::GetWindowWidth() / 2, Application::GetWindowHeight() / 2, 0)).Length() > 200.0f)
    {
        active = false;
        return;
    }

    // TODO Collision: Collide with either other player/asteroid/enemy
}

void Bullet::syncData() {
    // Implement network sync for bullet properties (e.g., pos, enemyBullet flag).
}
