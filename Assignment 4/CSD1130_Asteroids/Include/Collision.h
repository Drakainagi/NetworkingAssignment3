/******************************************************************************/
/*!
\file		Collision.h
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		Handles the collision response and collision checks via AABB.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_

#include "AEEngine.h"

/**************************************************************************/
/*!
Struct for the min and max positions of the boundary box
*/
/**************************************************************************/
struct AABB
{
	AEVec2	min;
	AEVec2	max;
};

bool CollisionIntersection_RectRect(const AABB& aabb1,            //Input
									const AEVec2& vel1,           //Input 
									const AABB& aabb2,            //Input 
									const AEVec2& vel2,           //Input
									float& firstTimeOfCollision); //Output: the calculated value of tFirst, must be returned here


bool AABBCollision(const AABB& aabb1, //Input
	               const AABB& aabb2);//Input


void HandlePhysics(AEVec2& m_FirstPos, AEVec2& m_FirstVel, const float& m_FirstMass,
	AEVec2& m_SecondPos, AEVec2& m_SecondVel, const float& m_SecondMass);
#endif // CSD1130_COLLISION_H_