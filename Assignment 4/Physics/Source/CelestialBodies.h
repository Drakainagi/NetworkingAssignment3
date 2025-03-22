#ifndef CELESTIALBODIES_H
#define CELESTIALBODIES_H

#include "GameObject.h"
class CelestialBodies : public GameObject {
public:
    // Constructor & Destructor
    CelestialBodies();
    virtual ~CelestialBodies();

    // Celestial body-specific properties.
    float gravitationalMass;  // Used for gravitational calculations.
    float radius;             // Size of the celestial body.

    // Override update to include any rotation or orbital motion if necessary.
    virtual void update(float dt) override;

    // Implement syncData to synchronize celestial body state over the network.
    virtual void syncData() override;
};

#endif // CELESTIALBODIES_H
