#include "GameObject.h"

GameObject::GameObject()
    : pos(0, 0, 0),
    vel(0, 0, 0),
    scale(1, 1, 1),
    angle(0.0f),
    mass(1.0f),
    active(false), 
    health(100),
    type(GAMEOBJECT_TYPE::GO_NONE)
{
}

GameObject::~GameObject()
{
}

void GameObject::update(float dt) {
    // A simple base update: integrate velocity.
    pos = pos + (vel * dt);
}

void GameObject::syncData() {
    //TODO
}