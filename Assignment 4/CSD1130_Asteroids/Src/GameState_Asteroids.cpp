/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		This file contains the functions and variables for the
			'Asteroids' scene state.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include "GameStateMgr.h"
#include <iostream>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM		= 3;			// initial number of ship lives
const float			SHIP_SCALE_X			= 16.0f;		// ship scale x
const float			SHIP_SCALE_Y			= 16.0f;		// ship scale y
const float			BULLET_SCALE_X			= 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y			= 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X	= 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X	= 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y	= 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y	= 60.0f;		// asteroid maximum scale y
const float			ASTEROID_MIN_VEL        = 50.0f;		// asteroid minimum velocity
const float			ASTEROID_MAX_VEL        = 300.0f;		// asteroid maximum velocity

const float			WALL_SCALE_X			= 64.0f;		// wall scale x
const float			WALL_SCALE_Y			= 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD		= 200.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= 200.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_MAX_ACCEL_FORWARD  = 400.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_MAX_ACCEL_BACKWARD = 400.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED			= 600.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE      = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0, 
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE				= 0x00000001;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList *	pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj *			pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
									// calculate the object instance's transformation matrix and save it here

};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst *		spShip;										// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst *		spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// Set to true when any value that is printed out is changed
static bool                 onValueChange;                              // On Value Changed

// State of the Game
static bool                 sStateGameOver;                             // Current mode of game
// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst *		gameObjInstCreate (unsigned long type, AEVec2* scale,
											   AEVec2 * pPos, AEVec2 * pVel, float dir);
void				gameObjInstDestroy(GameObjInst * pInst);

void				Helper_Wall_Collision();

void				SpawnAsteroid(int Num);

/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj * pObj;

	// =====================
	// create the ship shape
	// =====================

	pObj		= sGameObjList + sGameObjNum++;
	pObj->type	= TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f,  0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f,
		 0.5f,  0.0f, 0xFFFFFFFF, 0.0f, 0.0f );  

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		 0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		 0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		 0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the asteroid shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the wall shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_WALL;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");	
}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// OLD
	// create the initial 4 asteroids instances using the "gameObjInstCreate" function
	//AEVec2 pos, vel;

	////Asteroid 1
	//pos.x = 90.0f;		pos.y = -220.0f;
	//vel.x = -60.0f;		vel.y = -30.0f;
	//AEVec2Set(&scale, AERandFloat() * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X, 
	//	AERandFloat() * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y);
	//gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	////Asteroid 2
	//pos.x = -260.0f;	pos.y = -250.0f;
	//vel.x = 39.0f;		vel.y = -130.0f;
	//AEVec2Set(&scale, AERandFloat() * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X,
	//	AERandFloat() * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y);
	//gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	////Asteroid 3
	//pos.x = -20.0f;		pos.y = -100.0f;
	//vel.x = -60.0f;		vel.y = -30.0f;
	//AEVec2Set(&scale, AERandFloat() * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X,
	//	AERandFloat() * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y);
	//gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	////Asteroid 4
	//pos.x = 190.0f;		pos.y = -20.0f;
	//vel.x = -60.0f;		vel.y = -30.0f;
	//AEVec2Set(&scale, AERandFloat() * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X,
	//	AERandFloat() * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y);
	//gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// NEW
	// create the initial 4 asteroids instances using the "gameObjInstCreate" function
	SpawnAsteroid(4);

	// create the static wall
	AEVec2Set(&scale, WALL_SCALE_X, WALL_SCALE_Y);
	AEVec2 position;
	AEVec2Set(&position, 300.0f, 150.0f);
	spWall = gameObjInstCreate(TYPE_WALL, &scale, &position, nullptr, 0.0f);
	AE_ASSERT(spWall);
	

	// reset the score and the number of ships
	sScore      = 0;
	sShipLives  = SHIP_INITIAL_NUM;
	onValueChange = true;
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{

	// =========================================================
	// update according to input
	// =========================================================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0
	
	if (!sStateGameOver)
	{
		if (AEInputCheckCurr(AEVK_UP))
		{
			AEVec2 added;
			AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));

			// Find the velocity according to the acceleration
			spShip->velCurr.x += added.x * SHIP_ACCEL_FORWARD * (float)(AEFrameRateControllerGetFrameTime());
			spShip->velCurr.y += added.y * SHIP_ACCEL_FORWARD * (float)(AEFrameRateControllerGetFrameTime());

			// Limit your speed over here
			spShip->velCurr.x > SHIP_MAX_ACCEL_FORWARD ? spShip->velCurr.x = SHIP_MAX_ACCEL_FORWARD : 0;
			spShip->velCurr.x < -SHIP_MAX_ACCEL_FORWARD ? spShip->velCurr.x = -SHIP_MAX_ACCEL_FORWARD : 0;
			spShip->velCurr.y > SHIP_MAX_ACCEL_FORWARD ? spShip->velCurr.y = SHIP_MAX_ACCEL_FORWARD : 0;
			spShip->velCurr.y < -SHIP_MAX_ACCEL_FORWARD ? spShip->velCurr.y = -SHIP_MAX_ACCEL_FORWARD : 0;
		}

		if (AEInputCheckCurr(AEVK_DOWN))
		{
			AEVec2 added;
			AEVec2Set(&added, -cosf(spShip->dirCurr), -sinf(spShip->dirCurr));

			// Find the velocity according to the acceleration
			spShip->velCurr.x += added.x * SHIP_ACCEL_BACKWARD * (float)(AEFrameRateControllerGetFrameTime());
			spShip->velCurr.y += added.y * SHIP_ACCEL_BACKWARD * (float)(AEFrameRateControllerGetFrameTime());

			// Limit your speed over here
			spShip->velCurr.x > SHIP_MAX_ACCEL_BACKWARD ? spShip->velCurr.x = SHIP_MAX_ACCEL_BACKWARD : 0;
			spShip->velCurr.x < -SHIP_MAX_ACCEL_BACKWARD ? spShip->velCurr.x = -SHIP_MAX_ACCEL_BACKWARD : 0;
			spShip->velCurr.y > SHIP_MAX_ACCEL_BACKWARD ? spShip->velCurr.y = SHIP_MAX_ACCEL_BACKWARD : 0;
			spShip->velCurr.y < -SHIP_MAX_ACCEL_BACKWARD ? spShip->velCurr.y = -SHIP_MAX_ACCEL_BACKWARD : 0;
		}

		if (AEInputCheckCurr(AEVK_LEFT))
		{
			spShip->dirCurr += SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
			spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
		}

		if (AEInputCheckCurr(AEVK_RIGHT))
		{
			spShip->dirCurr -= SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
			spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
		}

		// Shoot a bullet if space is triggered (Create a new object instance)
		if (AEInputCheckTriggered(AEVK_SPACE))
		{
			AEVec2 m_vel{ 0 }, m_dir {0}, m_scale{ 0 };
			// Get the bullet's direction according to the ship's direction
			AEVec2Set(&m_dir, cosf(spShip->dirCurr), sinf(spShip->dirCurr));

			// Set the velocity
			m_vel.x += m_dir.x * BULLET_SPEED;
			m_vel.y += m_dir.y * BULLET_SPEED;

			AEVec2Set(&m_scale, BULLET_SCALE_X, BULLET_SCALE_Y);

			// Create an instance, based on BULLET_SCALE_X and BULLET_SCALE_Y
			gameObjInstCreate(TYPE_BULLET, &m_scale, &spShip->posCurr, &m_vel, spShip->dirCurr);
		}
	}






	// ======================================================================
	// Save previous positions
	//  -- For all instances
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		pInst->posPrev.x = pInst->posCurr.x;
		pInst->posPrev.y = pInst->posCurr.y;
	}






	// ======================================================================
	// update physics of all active game object instances
	//  -- Calculate the AABB bounding rectangle of the active instance, using the starting position:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//
	//	-- New position of the active instance is updated here with the velocity calculated earlier
	// ======================================================================

	//Running through gameObject List
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		//Adding velocity of GameObjects to position
		AEVec2 SpeedDT;
		AEVec2Scale(&SpeedDT, &pInst->velCurr, (float)(AEFrameRateControllerGetFrameTime()));
		pInst->posPrev = pInst->posCurr;
		AEVec2Add(&pInst->posCurr, &pInst->posCurr, &SpeedDT);

		pInst->boundingBox.min.x = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.min.y = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
		pInst->boundingBox.max.x = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.max.y = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
	}




	// ======================================================================
	// check for dynamic-static collisions (one case only: Ship vs Wall)
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	Helper_Wall_Collision();





	// ======================================================================
	// check for dynamic-dynamic collisions
	// ======================================================================

	/*
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		if (pInst->pObject->type == TYPE_ASTEROID)
		{

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between ship and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between bullet and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
	*/

	if (!sStateGameOver)
	{
		for (unsigned long i = 0; i < sGameObjInstNum; i++)
		{
			GameObjInst* pInst = sGameObjInstList + i;

			// skip non-active object
			if ((pInst->flag & FLAG_ACTIVE) == 0)
				continue;

			if (pInst->pObject->type == TYPE_ASTEROID)
			{
				for (unsigned long j = 0; j < sGameObjInstNum; j++)
				{
					GameObjInst* pInst2 = sGameObjInstList + j;

					if ((pInst2->flag & FLAG_ACTIVE) == 0
						|| pInst2->pObject->type == TYPE_ASTEROID)
						continue;

					float temp = 0;

					if (pInst2->pObject->type == TYPE_SHIP)
					{
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
							pInst2->boundingBox, pInst2->velCurr, temp) && temp < AEFrameRateControllerGetFrameTime())
						{
							sShipLives--;
							onValueChange = true;
							spShip->posCurr = { 0,0 };
							gameObjInstDestroy(pInst);
							SpawnAsteroid(1);
							break;
						}
					}

					if (pInst2->pObject->type == TYPE_BULLET)
					{
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
							pInst2->boundingBox, pInst2->velCurr, temp) && temp < AEFrameRateControllerGetFrameTime())
						{
							gameObjInstDestroy(pInst);
							gameObjInstDestroy(pInst2);

							sScore += 100;
							onValueChange = true;
							SpawnAsteroid(2);
							break;
						}
					}
				}
			}
		}
	}




	// ===================================================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================================================
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;
		
		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP)
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X, 
														AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y,
														AEGfxGetWinMaxY() + SHIP_SCALE_Y);

			continue;
		}

		// Wrap asteroids here
		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			// Wrap the asteroids from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - pInst->scale.x,
				AEGfxGetWinMaxX() + pInst->scale.x);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - pInst->scale.y,
				AEGfxGetWinMaxY() + pInst->scale.y);

			continue;
		}

		
		if (pInst->pObject->type == TYPE_BULLET)
		{
			// Remove bullets that go out of bounds
			pInst->posCurr.x < -AEGfxGetWindowWidth() / 2.0f ? gameObjInstDestroy(pInst) : pInst->posCurr.x > AEGfxGetWindowWidth() / 2.0f ? gameObjInstDestroy(pInst):0;
			pInst->posCurr.y < -AEGfxGetWindowHeight() / 2.0f ? gameObjInstDestroy(pInst) : pInst->posCurr.y > AEGfxGetWindowHeight() / 2.0f ? gameObjInstDestroy(pInst) : 0;
		
			continue;
		}
	}



	

	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;
		AEMtx33		 trans, rot, scale;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Compute the scaling matrix
		AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y);

		// Compute the rotation matrix
		AEMtx33Rot(&rot, pInst->dirCurr);
		
		// Compute the translation matrix
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);

		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
		AEMtx33Concat(&pInst->transform, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);
	}

	// ======================================================================
	// Update State of Scene / Change GameState
	// ======================================================================
	if (sScore >= 5000 || sShipLives < 0)
		sStateGameOver = true;
	if (AEInputCheckCurr(AEVK_1))
		gGameStateNext = GS_ACTUALASTEROIDS;
	
}

/******************************************************************************/
/*!
		Render all GameObjects active in scene.
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	char strBuffer[1024];
	
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxTextureSet(NULL, 0, 0);


	// Set blend mode to AE_GFX_BM_BLEND
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);


	// draw all object instances in the list
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;
		
		// Set the current object instance's transform matrix using "AEGfxSetTransform"
		AEGfxSetTransform(pInst->transform.m);

		// Draw the shape used by the current object instance using "AEGfxMeshDraw"
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}

	//Display any of these variables/strings whenever a change in their value happens
	if(onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		//AEGfxPrint(10, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//AEGfxPrint(600, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		// display the game over message
		if (sShipLives < 0)
		{
			//AEGfxPrint(280, 260, 0xFFFFFFFF, "       GAME OVER       ");
			printf("       GAME OVER       \n");
		}

		if (sScore >= 5000)
		{
			printf("       YOU ROCK       \n");
		}
		onValueChange = false;
	}
}

/******************************************************************************/
/*!
	Destroy all GameObjects active in scene.
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy"
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		gameObjInstDestroy(pInst);
	}
	
}

/******************************************************************************/
/*!
	Free all meshes and textures of GameObject data.
*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{

	for (unsigned long i = 0; i < sGameObjNum; i++)
	{
		GameObj* pInst = sGameObjList + i;
		// free all mesh data (shapes) of each object using "AEGfxMeshFree"
		AEGfxMeshFree(pInst->pMesh);
		pInst->pMesh = NULL;
	}
}

/******************************************************************************/
/*!
	Create GameObject from GameObject Pooling
*/
/******************************************************************************/
GameObjInst * gameObjInstCreate(unsigned long type, 
							   AEVec2 * scale,
							   AEVec2 * pPos, 
							   AEVec2 * pVel, 
							   float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);
	
	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
 			if (pInst->pObject == nullptr)
				sGameObjInstNum++;
			pInst->pObject	= sGameObjList + type;
			pInst->flag		= FLAG_ACTIVE;
			pInst->scale	= *scale;
			pInst->posCurr	= pPos ? *pPos : zero;
			pInst->velCurr	= pVel ? *pVel : zero;
			pInst->dirCurr	= dir;

			// return the newly created instance
			return pInst;
		}
	}

	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!
	Set GameObject to inactive
*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst * pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}


/******************************************************************************/
/*!
    check for collision between Ship and Wall and apply physics response on the Ship
		-- Apply collision response only on the "Ship" as we consider the "Wall" object is always stationary
		-- We'll check collision only when the ship is moving towards the wall!
	[DO NOT UPDATE THIS PARAGRAPH'S CODE]
*/
/******************************************************************************/
void Helper_Wall_Collision()
{
	//calculate the vectors between the previous position of the ship and the boundary of wall
	AEVec2 vec1 = { 0 };
	vec1.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec1.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec2 = { 0 };
	vec2.x = 0.0f;
	vec2.y = -1.0f;
	AEVec2 vec3 = { 0 };
	vec3.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec3.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec4 = { 0 };
	vec4.x = 1.0f;
	vec4.y = 0.0f;
	AEVec2 vec5 = { 0 };
	vec5.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec5.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec6 = { 0 };
	vec6.x = 0.0f;
	vec6.y = 1.0f;
	AEVec2 vec7 = { 0 };
	vec7.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec7.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec8 = { 0 };
	vec8.x = -1.0f;
	vec8.y = 0.0f;
	if (
		(AEVec2DotProduct(&vec1, &vec2) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec2) <= 0.0f) ||
		(AEVec2DotProduct(&vec3, &vec4) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec4) <= 0.0f) ||
		(AEVec2DotProduct(&vec5, &vec6) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec6) <= 0.0f) ||
		(AEVec2DotProduct(&vec7, &vec8) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec8) <= 0.0f)
		)
	{
		float firstTimeOfCollision = 0.0f;
		if (CollisionIntersection_RectRect(spShip->boundingBox,
			spShip->velCurr,
			spWall->boundingBox,
			spWall->velCurr,
			firstTimeOfCollision))
		{
			//re-calculating the new position based on the collision's intersection time
			spShip->posCurr.x = spShip->velCurr.x * (float)firstTimeOfCollision + spShip->posPrev.x;
			spShip->posCurr.y = spShip->velCurr.y * (float)firstTimeOfCollision + spShip->posPrev.y;

			//reset ship velocity
			spShip->velCurr.x = 0.0f;
			spShip->velCurr.y = 0.0f;
		}
	}
}

/******************************************************************************/
/*!
	Spawn number of asteroids based on parameter on the outskirts of the screen.
*/
/******************************************************************************/
void SpawnAsteroid(int Num)
{
	for (int i = 0; i < Num; i++)
	{
		AEVec2 pos{ 0 }, vel{ 0 }, scale{ 0 };
		(AERandFloat() > 0.5f ? -1 : 1);

        AEVec2Set(&scale, AERandFloat() * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X, 
        	AERandFloat() * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y);

		//Reroll Position of asteroid until im happy =)
		while (true)
		{
			AEVec2 temp = { AERandFloat() * (AEGfxGetWinMaxX() + scale.x), AERandFloat() * (AEGfxGetWinMaxY() + scale.y) };

			if (temp.x < AEGfxGetWinMinX() && temp.x > AEGfxGetWinMinX() - scale.x && temp.y > AEGfxGetWinMinY() && temp.y < AEGfxGetWinMaxY() ||//Left Side
				temp.x > AEGfxGetWinMaxX() && temp.x < AEGfxGetWinMaxX() + scale.x && temp.y > AEGfxGetWinMinY() && temp.y < AEGfxGetWinMaxY() ||//Right Side
				temp.y < AEGfxGetWinMinY() && temp.y > AEGfxGetWinMinY() - scale.y && temp.x > AEGfxGetWinMinX() && temp.x < AEGfxGetWinMaxX() ||//top Side
				temp.y > AEGfxGetWinMaxY() && temp.y < AEGfxGetWinMaxY() + scale.y && temp.x > AEGfxGetWinMinX() && temp.x < AEGfxGetWinMaxX())  //bottom Side
			{
				pos = temp;
				break;
			}
		}

		AEVec2Set(&vel, ((AERandFloat() * (ASTEROID_MAX_VEL - ASTEROID_MIN_VEL)) + ASTEROID_MIN_VEL) * (AERandFloat() > 0.5f? -1:1),
			((AERandFloat() * (ASTEROID_MAX_VEL - ASTEROID_MIN_VEL)) + ASTEROID_MIN_VEL) * (AERandFloat() > 0.5f ? -1 : 1));
        gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);
	}
}