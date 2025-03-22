#include "CelestialBodies.h"
#include "PlayerShip.h"
#include "Application.h"

CelestialBodies::CelestialBodies()
    : GameObject(),
    gravitationalMass(1000.0f),
    radius(50.0f)
{
    type = GAMEOBJECT_TYPE::GO_ASTEROID;
    active = true;
}

CelestialBodies::~CelestialBodies()
{
}

void CelestialBodies::update(float dt) 
{
    // Celestial bodies may have unique update logic such as slow rotations.
    GameObject::update(dt);
    // Additional celestial-specific behavior can be added here.
    
    // TODO SERVER CONTROL
    // Deactivate if too far.
    if ((pos - Vector3(Application::GetWindowWidth()/2, Application::GetWindowHeight() / 2, 0)).Length() > 200.0f)
    {
        active = false;
        return;
    }

    // TODO Collision: Apply gravitational pull on the obj list. 
    //float combinedRadii = scale.x + player->scale.x + 10.0f;
    //if ((pos - entitiy->pos).LengthSquared() > combinedRadii * combinedRadii)
    //{
    //    entitiy->vel += (pos - player->pos).Normalized() * (mass / (pos - player->pos).LengthSquared());
    //}
}

void CelestialBodies::syncData() {
    // Implement network sync for celestial body properties (e.g., pos, gravitationalMass, radius).
}