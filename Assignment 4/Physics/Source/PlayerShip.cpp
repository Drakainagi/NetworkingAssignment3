#include "PlayerShip.h"
#include "Application.h"
#include "Math.h"
#include <cmath>
#include <iostream>

// Assumed external constants. Ensure these are defined in your project.
extern const float BULLET_SPEED;
//extern const float MISSILE_SPEED;

PlayerShip::PlayerShip()
    : GameObject(),
    playerID(0),
    shield(50.0f),
    currentWeapon(WeaponType::NORMAL),
    bulletCooldown(0),
    angularVelocity(0.0f),
    momentOfInertia(1.0f),
    speed(1.0f),
    m_firedbulletspeed(50)
{
    active = true;
    // Initialize facing direction (upwards)
    dir = Vector3(0, 1, 0);
    m_torque.SetZero();
    // Initially lock all weapons then unlock the default (NORMAL)
    unlockedWeapons.reset();
    unlockedWeapons.set(static_cast<size_t>(WeaponType::NORMAL), true);
}

PlayerShip::~PlayerShip()
{
}

void PlayerShip::update(float dt)
{
    // --- Movement & Physics Controls ---
    Vector3 appliedForce(0, 0, 0);

    // Process movement input
    if (Application::IsKeyPressed('W'))
    {
        appliedForce = dir * 150.0f;
    }
    if (Application::IsKeyPressed('S'))
    {
        appliedForce = dir * -150.0f;
    }

    // Reset torque before applying new input
    m_torque.SetZero();

    if (Application::IsKeyPressed('A'))
    {
        m_torque = Vector3(1, -1, 0).Cross(Vector3(0.0f, 500.0f, 0.0f));
    }
    if (Application::IsKeyPressed('D'))
    {
        m_torque = Vector3(-1, -1, 0).Cross(Vector3(0.0f, 500.0f, 0.0f));
    }

    // Dampen angular velocity over time
    angularVelocity *= (1.0f - 0.9f * dt);
    if (fabs(angularVelocity) <= Math::EPSILON)
    {
        angularVelocity = 0.0f;
    }

    // Update rotational physics:
    momentOfInertia = mass * scale.x * scale.x;
    angularVelocity += (m_torque.z / momentOfInertia) * dt;
    angle += angularVelocity * dt;
    dir = Vector3(cosf(angle), sinf(angle), 0.0f);
    m_torque.SetZero();

    // Update linear physics:
    Vector3 acceleration = force * (1.0f / mass);
    vel += acceleration * (speed * dt);

    // Call the base update to handle position updates (if implemented in GameObject::update)
    GameObject::update(dt);

    // --- Weapon Selection Controls (Keyboard) ---
    // Example: Use keys '1' through '4' to select different weapons.
    if (Application::IsKeyPressed('1') && unlockedWeapons.test(static_cast<size_t>(WeaponType::NORMAL)))
        currentWeapon = WeaponType::NORMAL;
    if (Application::IsKeyPressed('2') && unlockedWeapons.test(static_cast<size_t>(WeaponType::MACHINE_GUN)))
        currentWeapon = WeaponType::MACHINE_GUN;
    if (Application::IsKeyPressed('3') && unlockedWeapons.test(static_cast<size_t>(WeaponType::SHOTGUN)))
        currentWeapon = WeaponType::SHOTGUN;
    if (Application::IsKeyPressed('4') && unlockedWeapons.test(static_cast<size_t>(WeaponType::PULSE_GUN)))
        currentWeapon = WeaponType::PULSE_GUN;

    // For testing: Press '0' to unlock all weapons.
    if (Application::IsKeyPressed('0'))
    {
        for (size_t i = 0; i < static_cast<size_t>(WeaponType::COUNT); ++i)
            unlockedWeapons.set(i, true);
    }

    // SHOOTING CODE IS IN SCENE DUE TO LIMITATIONS WITH SPAWNING
}

void PlayerShip::syncData()
{
    // Implement network sync for ship properties:
    // e.g., playerID, pos, health, shield, currentWeapon, etc.
}