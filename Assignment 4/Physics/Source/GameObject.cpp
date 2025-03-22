
#include "GameObject.h"

GameObject::GameObject(GAMEOBJECT_TYPE typeValue) 
	: type(typeValue),
	scale(1, 1, 1),
	active(false),
	mass(1.f),
	angularVelocity(0.0f),
	angle(0.0f),
	momentOfInertia(0.0f),
	BossDamage(0.0f)
{
}

GameObject::~GameObject()
{
}