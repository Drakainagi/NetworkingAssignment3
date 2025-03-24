/******************************************************************************/
/*!
\file		Collision.cpp
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		Handles the collision response and collision checks via AABB.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!
Static AABB checking & dynamic AABB checking
*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	/*
	Implement the collision intersection over here.

	The steps are:	
	Step 1: Check for static collision detection between rectangles (static: before moving). 
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.

	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/

	bool CheckCollision = AABBCollision(aabb1, aabb2);
	if (CheckCollision)return 1;
	else
	{
		float tFirst = 0.0f;
		float tLast = (float)AEFrameRateControllerGetFrameTime();
		
		AEVec2 velRelative = { vel2.x - vel1.x ,  vel2.y - vel1.y };
		
		//X-axis
		if (velRelative.x < 0)
		{
			if (aabb1.min.x > aabb2.max.x) return 0;
			if (aabb1.max.x < aabb2.min.x)
			{
				float temp = (aabb1.max.x - aabb2.min.x) / (velRelative.x);
				tFirst = temp > tFirst ? temp : tFirst;
			}
			if (aabb1.min.x < aabb2.max.x)
			{
				float temp = (aabb1.min.x - aabb2.max.x) / (velRelative.x);
				tLast = temp < tLast ? temp : tLast;
			}
		}
		else if (velRelative.x > 0)
		{
			if (aabb1.min.x > aabb2.max.x)
			{
				float temp = (aabb1.min.x - aabb2.max.x) / (velRelative.x);
				tFirst = temp > tFirst ? temp : tFirst;
			}
			if (aabb1.max.x > aabb2.min.x)
			{
				float temp = (aabb1.max.x - aabb2.min.x) / (velRelative.x);
				tLast = temp < tLast ? temp : tLast;
			}
			if (aabb1.max.x < aabb2.min.x) return 0;
		}

		//Y-axis
		if (velRelative.y < 0)
		{
			if (aabb1.min.y > aabb2.max.y) return 0;
			if (aabb1.max.y < aabb2.min.y)
			{
				float temp = (aabb1.max.y - aabb2.min.y) / (velRelative.y);
				tFirst = temp > tFirst ? temp : tFirst;
			}
			if (aabb1.min.y < aabb2.max.y)
			{
				float temp = (aabb1.min.y - aabb2.max.y) / (velRelative.y);
				tLast = temp < tLast ? temp : tLast;
			}
		}
		else if (velRelative.y > 0)
		{
			if (aabb1.min.y > aabb2.max.y)
			{
				float temp = (aabb1.min.y - aabb2.max.y) / (velRelative.y);
				tFirst = temp > tFirst ? temp : tFirst;
			}
			if (aabb1.max.y > aabb2.min.y)
			{
				float temp = (aabb1.max.y - aabb2.min.y) / (velRelative.y);
				tLast = temp < tLast ? temp : tLast;
			}
			if (aabb1.max.y < aabb2.min.y) return 0;
		}

		if (tFirst > tLast) return 0;
		else
		{
			firstTimeOfCollision = tFirst;
			return 1;
		}
	}

}

/**************************************************************************/
/*!
Static AABB checking function
*/
/**************************************************************************/
bool AABBCollision(const AABB& aabb1, const AABB& aabb2)
{
	return (aabb1.max.x > aabb2.min.x && aabb1.min.x < aabb2.max.x) &&
		(aabb1.max.y > aabb2.min.y && aabb1.min.y < aabb2.max.y);
}

/**************************************************************************/
/*!
Handling Physics when GameObjects collided
- Physics Collision (Change of vel & momentum) between Asteroids with mass based on asteroid scaling
- Asteroid Cluster due to mass of Asteroid (Smaller Asteroids stick to Bigger Asteroids) when contact
*/
/**************************************************************************/
void HandlePhysics(AEVec2& m_FirstPos, AEVec2& m_FirstVel, const float& m_FirstMass, 
	AEVec2& m_SecondPos, AEVec2& m_SecondVel, const float& m_SecondMass)
{
	AEVec2 temp{ 0 };
	AEVec2 DirofOBJS{ 0 };
	AEVec2Sub(&DirofOBJS, &m_FirstPos, &m_SecondPos);
	AEVec2Normalize(&DirofOBJS, &DirofOBJS); // Calculate the direction of the objects involved

	// Calculate the unit vector of the two objects to represent the direction of object 1 and 2 with the influence of its' velocity
	// Calculate the scalar using dot product of the object respective velocity vector and the direction vector and scale the direction vector by this scalar
	// The result is the unit vector of object 1 and 2 pointing in the direction vector adjusted by the respective velocity vector 
	AEVec2 u1N{ 0 };
	AEVec2Scale(&u1N, &DirofOBJS, AEVec2DotProduct(&m_FirstVel, &DirofOBJS));
	AEVec2 u2N{ 0 };
	AEVec2Scale(&u2N, &DirofOBJS, AEVec2DotProduct(&m_SecondVel, &DirofOBJS));

	// go->vel = u1 + (2 * m2) / (m1 + m2) * (u2N - u1N); KINEMATIC FORMULA + MOMENTUM
	// Calculate the relative velocity by subtracting u1N from u2N (for object 1)
	// Scale up the relative velocity by 2.0f/3.0f
	// Scale up the new vector by (2 * mass2) / (total mass of the two objects collided) - Momentum after collision
	// Add the initial velocity of the object by the new vectors and assign it as the object new velocity
	AEVec2Sub(&temp, &u2N, &u1N);
	AEVec2Scale(&temp, &temp, 2.f / 3.f);
	AEVec2Scale(&temp, &temp, (2 * m_SecondMass) / (m_FirstMass + m_SecondMass));
	AEVec2Add(&m_FirstVel, &temp, &m_FirstVel);
	temp = { 0 };
	AEVec2Sub(&temp, &u1N, &u2N);
	AEVec2Scale(&temp, &temp, 2.f / 3.f);
	AEVec2Scale(&temp, &temp, (2 * m_FirstMass) / (m_FirstMass + m_SecondMass));
	AEVec2Add(&m_SecondVel, &temp, &m_SecondVel);
}