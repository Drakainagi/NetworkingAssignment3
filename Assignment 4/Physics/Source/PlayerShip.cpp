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
    speed(10.0f),
    m_firedbulletspeed(600)
{
    active = true;
    type = GO_SHIP;
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
    // Compute applied force based on movement input.
    Vector3 appliedForce(0, 0, 0);
    if (Application::IsKeyPressed('W'))
        appliedForce += dir * 150.0f;
    if (Application::IsKeyPressed('S'))
        appliedForce += dir * -150.0f;
    force = appliedForce;  // Set the force for linear physics

    // Process rotational input.
    m_torque.SetZero();
    if (Application::IsKeyPressed('A'))
        m_torque = Vector3(1, -1, 0).Cross(Vector3(0.0f, 2000.0f, 0.0f));
    else if (Application::IsKeyPressed('D'))
        m_torque = Vector3(-1, -1, 0).Cross(Vector3(0.0f, 2000.0f, 0.0f));

    // Dampen angular velocity.
    angularVelocity *= (1.0f - 0.9f * dt);
    if (fabs(angularVelocity) <= Math::EPSILON)
        angularVelocity = 0.0f;

    // Update rotational physics.
    momentOfInertia = mass * scale.x * scale.x;
    angularVelocity += (m_torque.z / momentOfInertia) * (speed * dt);
    angle += angularVelocity * dt;
    dir = Vector3(cosf(angle), sinf(angle), 0.0f);
    m_torque.SetZero();

    // Update linear physics.
    Vector3 acceleration = force * (1.0f / mass);
    vel += acceleration * (speed * dt);

    // Call base update (e.g., to update position using vel).
    GameObject::update(dt);

    // --- Weapon Selection Controls (Keyboard) ---
    // Use an array of key/weapon pairs to reduce redundancy.
    const std::pair<char, WeaponType> weaponMapping[] = {
        { '1', WeaponType::NORMAL },
        { '2', WeaponType::MACHINE_GUN },
        { '3', WeaponType::SHOTGUN },
        { '4', WeaponType::PULSE_GUN }
    };
    for (const auto& keyWeapon : weaponMapping)
    {
        if (Application::IsKeyPressed(keyWeapon.first) &&
            unlockedWeapons.test(static_cast<size_t>(keyWeapon.second)))
        {
            currentWeapon = keyWeapon.second;
        }
    }

    // Unlock all weapons when '0' is pressed.
    if (Application::IsKeyPressed('0'))
    {
        for (size_t i = 0; i < static_cast<size_t>(WeaponType::COUNT); ++i)
            unlockedWeapons.set(i, true);
    }
    
    // Wrap-around logic: assume pos is a Vector3 and using Application::GetWindowWidth()/GetWindowHeight()
    float windowWidth = static_cast<float>(Application::GetWindowWidth());
    float windowHeight = static_cast<float>(Application::GetWindowHeight());
    if (pos.x < 0)
        pos.x += windowWidth;
    else if (pos.x > windowWidth)
        pos.x -= windowWidth;
    if (pos.y < 100)
        pos.y += windowHeight;
    else if (pos.y > windowHeight+100)
        pos.y -= windowHeight;

    // SHOOTING CODE IS HANDLED IN THE SCENE
}

void PlayerShip::syncData()
{
    // Implement network sync for ship properties:
    // e.g., playerID, pos, health, shield, currentWeapon, etc.
}