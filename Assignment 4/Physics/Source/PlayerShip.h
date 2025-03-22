#ifndef PLAYERSHIP_H
#define PLAYERSHIP_H

#include "GameObject.h"
#include <bitset>

// Define weapon types. The ordering should match your intended mapping:
// 0: Normal Pellets, 1: Machine Gun, 2: Shotgun, 3: Pulse Gun.
enum class WeaponType {
    NORMAL = 0,
    MACHINE_GUN,
    SHOTGUN,
    PULSE_GUN,
    COUNT  // Utility to get number of weapon types
};

class PlayerShip : public GameObject {
public:
    // Constructor & Destructor
    PlayerShip();
    virtual ~PlayerShip();

    // Ship-specific properties for multiplayer control.
    int playerID;      // Identifier for the player controlling this ship
    float shield;

    // Weapon system: current weapon selection and unlocked status.
    WeaponType currentWeapon;
    std::bitset<static_cast<size_t>(WeaponType::COUNT)> unlockedWeapons;

    // Shooting & control timers.
    float bulletCooldown;

    // Physics & movement variables.
    Vector3 dir;         // Current facing direction.
    Vector3 force;       // Force applied for movement.
    float speed;         // Movement speed multiplier.
    Vector3 m_torque;    // Current torque applied.
    float angularVelocity; // Current angular velocity.
    float momentOfInertia; // Computed moment of inertia for rotation.
    float m_firedbulletspeed; // Bullet fire at what speed

    // Override update to include ship-specific behavior (input handling, shooting, physics).
    virtual void update(float dt) override;

    // Implement syncData to synchronize ship state over the network.
    virtual void syncData() override;

private:
    // Helper function to spawn a projectile. In a complete system, this would interface with an object pool.
    GameObject* spawnProjectile(GAMEOBJECT_TYPE projType);
};

#endif // PLAYERSHIP_H
