/******************************************************************************/
/*!
\file		GameState_ActualAsteroids.cpp
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		This file contains the functions and variables for the 
            'Actual Asteroids' scene state.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include <iostream>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM		= 300;			// initial number of ship lives
const float			SHIP_SCALE_X			= 50.0f;		// ship scale x
const float			SHIP_SCALE_Y			= 50.0f;		// ship scale y
const float			BULLET_SCALE_X			= 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y			= 20.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE   	= 30.0f;		// asteroid minimum scale
const float			ASTEROID_MAX_SCALE   	= 100.0f;		// asteroid maximum scale
const float			ASTEROID_MIN_VEL        = 50.0f;		// asteroid minimum velocity
const float			ASTEROID_MAX_VEL        = 300.0f;		// asteroid maximum velocity

const float			WALL_SCALE_X			= 64.0f;		// wall scale x
const float			WALL_SCALE_Y			= 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD		= 50.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= 50.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_MAX_ACCEL_FORWARD  = 200.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_MAX_ACCEL_BACKWARD = 200.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED			= 1500.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE      = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

const int           NUM_OF_BACKGROUNDS      = 5;            // Num of parallax background 
const float			BACKGROUND_SCALE_X      = 1600;	     	// background scale x
const float			BACKGROUND_SCALE_Y      = 900;		    // background scale y
// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0, 
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_BACKGROUND1,
	TYPE_BACKGROUND2,
	TYPE_BACKGROUND3,
	TYPE_BACKGROUND4,
	TYPE_BACKGROUND5,

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
	AEGfxTexture *      pTex;       // object texture
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
static GameObjInst*         spBackground[NUM_OF_BACKGROUNDS];			// Pointer to the "Background" game object instance

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

static AEVec2               WORLD_CENTER;                               // Determines the start point of the world
// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst *		GOInstCreate (unsigned long type, AEVec2* scale,
											   AEVec2 * pPos, AEVec2 * pVel, float dir);
void				GOInstDestroy(GameObjInst * pInst);

void				SpawnActualAsteroid(int Num);

/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateActualAsteroidsLoad(void)
{
	//SET WORLD BASE COORDINATE
	WORLD_CENTER = { 0,0 };

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
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/GameObject_PlayerMain.png");
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/GameObject_PlayerBulletHolo.png");
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the asteroid shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/GameObject_Asteroid.png");
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// =========================
    // create the Background
    // =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BACKGROUND1;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/Background1.png");
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BACKGROUND2;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/Background2.png");
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BACKGROUND3;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/Background3.png");
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BACKGROUND4;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/Background4.png");
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BACKGROUND5;
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue
	pObj->pMesh = AEGfxMeshEnd();
	pObj->pTex = AEGfxTextureLoad("../Resources/Textures/Background5.png");
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateActualAsteroidsInit(void)
{
	// create the background
	AEVec2 scale;
	AEVec2Set(&scale, BACKGROUND_SCALE_X, BACKGROUND_SCALE_Y);
	for (int i = 0; i < NUM_OF_BACKGROUNDS; i++)
	{
		switch (i)
		{
		case 0:
			spBackground[i] = GOInstCreate(TYPE_BACKGROUND1, &scale, nullptr, nullptr, 0.0f);
			break;
		case 1:
			spBackground[i] = GOInstCreate(TYPE_BACKGROUND2, &scale, nullptr, nullptr, 0.0f);
			break;
		case 2:
			spBackground[i] = GOInstCreate(TYPE_BACKGROUND3, &scale, nullptr, nullptr, 0.0f);
			break;
		case 3:
			spBackground[i] = GOInstCreate(TYPE_BACKGROUND4, &scale, nullptr, nullptr, 0.0f);
			break;
		case 4:
			spBackground[i] = GOInstCreate(TYPE_BACKGROUND5, &scale, nullptr, nullptr, 0.0f);
			break;
		default:
			break;
		}
	}
		

	// create the main ship
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = GOInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);

	//Spawn Asteroids
	SpawnActualAsteroid(8);

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
void GameStateActualAsteroidsUpdate(void)
{
	//////////////////////////////////////////////////////////////
	//AUTO SPAWNING OF ASTEROIDS
	static float timer = 5.0f;
	if (timer < 0.0f)
	{
		SpawnActualAsteroid(1); 
		timer = 5.0f;
	}
	timer -= (float)AEFrameRateControllerGetFrameTime();
	//////////////////////////////////////////////////////////////
	
	// =========================================================
	// update according to input
	// =========================================================
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
		if (AEInputCheckCurr(AEVK_SPACE))
		{
			AEVec2 m_vel{ 0 }, m_dir {0}, m_scale{ 0 };
			// Get the bullet's direction according to the ship's direction
			AEVec2Set(&m_dir, cosf(spShip->dirCurr), sinf(spShip->dirCurr));

			// Set the velocity
			m_vel.x = spShip->velCurr.x + m_dir.x * BULLET_SPEED;
			m_vel.y = spShip->velCurr.y + m_dir.y * BULLET_SPEED;

			AEVec2Set(&m_scale, BULLET_SCALE_X, BULLET_SCALE_Y);

			// Create an instance, based on BULLET_SCALE_X and BULLET_SCALE_Y
			GOInstCreate(TYPE_BULLET, &m_scale, &spShip->posCurr, &m_vel, spShip->dirCurr);
		}
	}

	// ======================================================================
	// Update parallax background
	// ======================================================================
	AEVec2 m_BackgroundPos{ 0 };
	AEVec2Scale(&m_BackgroundPos, &WORLD_CENTER, 3.0f);
	m_BackgroundPos = { -fmodf(m_BackgroundPos.x, BACKGROUND_SCALE_X),
		-fmodf(m_BackgroundPos.y, BACKGROUND_SCALE_Y) };
	spBackground[0]->posCurr = m_BackgroundPos;

	for (int i = 1; i < NUM_OF_BACKGROUNDS; i++)
	{
		AEVec2Scale(&m_BackgroundPos, &WORLD_CENTER, 4.0f + i * 1.0f);
		m_BackgroundPos = { -fmodf(m_BackgroundPos.x, BACKGROUND_SCALE_X),
			-fmodf(m_BackgroundPos.y, BACKGROUND_SCALE_Y) };
		spBackground[i]->posCurr = m_BackgroundPos;
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
	// check for dynamic-dynamic collisions
	// ======================================================================
	if (!sStateGameOver)
	{
		for (unsigned long i = 0; i < sGameObjInstNum; i++)
		{
			GameObjInst* pInst = sGameObjInstList + i;

			// skip non-active object
			if ((pInst->flag & FLAG_ACTIVE) == 0
				|| pInst->pObject->type == TYPE_BACKGROUND1
				|| pInst->pObject->type == TYPE_BACKGROUND2
				|| pInst->pObject->type == TYPE_BACKGROUND3
				|| pInst->pObject->type == TYPE_BACKGROUND4
				|| pInst->pObject->type == TYPE_BACKGROUND5)
				continue;

			
			if (pInst->pObject->type == TYPE_ASTEROID)
			{
				for (unsigned long j = 0; j < sGameObjInstNum; j++)
				{
					GameObjInst* pInst2 = sGameObjInstList + j;

					if ((pInst2->flag & FLAG_ACTIVE) == 0
						|| (pInst2->pObject->type == TYPE_ASTEROID && i>=j)
						|| pInst2->pObject->type == TYPE_BACKGROUND1
						|| pInst2->pObject->type == TYPE_BACKGROUND2
						|| pInst2->pObject->type == TYPE_BACKGROUND3
						|| pInst2->pObject->type == TYPE_BACKGROUND4
						|| pInst2->pObject->type == TYPE_BACKGROUND5)
						continue;

					float temp = 0;

					// collides with ship
					if (pInst2->pObject->type == TYPE_SHIP)
					{
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
							pInst2->boundingBox, pInst2->velCurr, temp) && temp < AEFrameRateControllerGetFrameTime())
						{
							sShipLives--;
							onValueChange = true;
							spShip->posCurr = { 0,0 };
							GOInstDestroy(pInst);
							SpawnActualAsteroid(1);
							break;
						}
					}

					// collides with another asteroid
					if (pInst2->pObject->type == TYPE_ASTEROID)
					{
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
							pInst2->boundingBox, pInst2->velCurr, temp) && temp < AEFrameRateControllerGetFrameTime())
						{
							HandlePhysics(pInst2->posCurr, pInst2->velCurr, pInst2->scale.x, pInst->posCurr, pInst->velCurr, pInst->scale.x);
						}
					}

					// collides with bullet
					if (pInst2->pObject->type == TYPE_BULLET)
					{
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
							pInst2->boundingBox, pInst2->velCurr, temp) && temp < AEFrameRateControllerGetFrameTime())
						{
							GOInstDestroy(pInst);
							GOInstDestroy(pInst2);

							sScore += 100;
							onValueChange = true;
							SpawnActualAsteroid(1);
							break;
						}
					}
				}
			}
		}
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
		if ((pInst->flag & FLAG_ACTIVE) == 0
			|| pInst->pObject->type == TYPE_BACKGROUND1
			|| pInst->pObject->type == TYPE_BACKGROUND2
			|| pInst->pObject->type == TYPE_BACKGROUND3
			|| pInst->pObject->type == TYPE_BACKGROUND4
			|| pInst->pObject->type == TYPE_BACKGROUND5)
			continue;

		if (pInst->pObject->type == TYPE_SHIP)
		{
			//Adding velocity of ship to WORLD_CENTER
			AEVec2 SpeedDT;
			AEVec2Scale(&SpeedDT, &pInst->velCurr, (float)(AEFrameRateControllerGetFrameTime()));
			AEVec2Add(&WORLD_CENTER, &WORLD_CENTER, &SpeedDT);

			pInst->boundingBox.min.x = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
			pInst->boundingBox.min.y = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
			pInst->boundingBox.max.x = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
			pInst->boundingBox.max.y = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;

			continue;
		}
		else
		{
			//Adding velocity of GameObjects to position based on WORLD_CENTER
			AEVec2 SpeedDT;
			AEVec2Scale(&SpeedDT, &pInst->velCurr, (float)(AEFrameRateControllerGetFrameTime()));
			pInst->posPrev = pInst->posCurr;
			AEVec2 PlayerSpeedDT;
			AEVec2Scale(&PlayerSpeedDT, &spShip->velCurr, 0.3f); //Reduce the amount of movement made
			AEVec2Add(&pInst->posCurr, &pInst->posCurr, &SpeedDT);
			AEVec2Sub(&pInst->posCurr, &pInst->posCurr, &PlayerSpeedDT);

			pInst->boundingBox.min.x = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
			pInst->boundingBox.min.y = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
			pInst->boundingBox.max.x = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
			pInst->boundingBox.max.y = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
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
			pInst->posCurr.x < -AEGfxGetWindowWidth() / 2.0f ? GOInstDestroy(pInst) : pInst->posCurr.x > AEGfxGetWindowWidth() / 2.0f ? GOInstDestroy(pInst):0;
			pInst->posCurr.y < -AEGfxGetWindowHeight() / 2.0f ? GOInstDestroy(pInst) : pInst->posCurr.y > AEGfxGetWindowHeight() / 2.0f ? GOInstDestroy(pInst) : 0;
		
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
	if (sScore >= 999999 || sShipLives < 0)
		sStateGameOver = true;
	
}

/******************************************************************************/
/*!
	Render all GameObjects active in scene.
*/
/******************************************************************************/
void GameStateActualAsteroidsDraw(void)
{
	char strBuffer[1024];

	// Set the background to black.
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);


	AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
	// Set the the color to multiply to white, so that the sprite can 
	// display the full range of colors (default is black).
	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 0.75f);

	// Set the color to add to nothing, so that we don't alter the sprite's color
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.01f);

	// Set blend mode to AE_GFX_BM_BLEND
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);

	// Set blend mode to AE_GFX_BM_BLEND
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(0.5f);

	// draw all Backgrounds
	for (int i = 0; i < NUM_OF_BACKGROUNDS; i++)
	{
		for (int x = -1; x < 2; x++)
		{
			for (int y = -1; y < 2; y++)
			{
				AEGfxTextureSet(spBackground[i]->pObject->pTex, 0, 0);

				//RECONFIG FOR ENDLESS LOOPING BACKGROUND (3x3 Grid)
				AEMtx33	trans, rot, scale;
				// Compute the scaling matrix
				AEMtx33Scale(&scale, spBackground[i]->scale.x, spBackground[i]->scale.y);
				// Compute the rotation matrix
				AEMtx33Rot(&rot, spBackground[i]->dirCurr);
				// Compute the translation matrix
				AEMtx33Trans(&trans, spBackground[i]->posCurr.x + x * BACKGROUND_SCALE_X, spBackground[i]->posCurr.y + y * BACKGROUND_SCALE_Y);
				// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
				AEMtx33Concat(&spBackground[i]->transform, &rot, &scale);
				AEMtx33Concat(&spBackground[i]->transform, &trans, &spBackground[i]->transform);

				// Set the current object instance's transform matrix using "AEGfxSetTransform"
				AEGfxSetTransform(spBackground[i]->transform.m);

				// Draw the shape used by the current object instance using "AEGfxMeshDraw"
				AEGfxMeshDraw(spBackground[i]->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
			}
		}
	}

	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 1.0f);
	AEGfxSetTransparency(1.0f);
	// draw all object instances in the list
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0 ||
			pInst->pObject->type == TYPE_BACKGROUND1 ||
			pInst->pObject->type == TYPE_BACKGROUND2 || 
			pInst->pObject->type == TYPE_BACKGROUND3 || 
			pInst->pObject->type == TYPE_BACKGROUND4 || 
			pInst->pObject->type == TYPE_BACKGROUND5)
			continue;

		// Set the Texture
		AEGfxTextureSet(pInst->pObject->pTex, 0, 0);

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

		if (sScore >= 999999)
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
void GameStateActualAsteroidsFree(void)
{
	// kill all object instances in the array using "GOInstDestroy"
	for (unsigned long i = 0; i < sGameObjInstNum; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		GOInstDestroy(pInst);
	}
	
}

/******************************************************************************/
/*!
	Free all meshes and textures of GameObject data.
*/
/******************************************************************************/
void GameStateActualAsteroidsUnload(void)
{

	for (unsigned long i = 0; i < sGameObjNum; i++)
	{
		GameObj* pInst = sGameObjList + i;
		// free all mesh data (shapes) of each object using "AEGfxMeshFree"
		AEGfxMeshFree(pInst->pMesh);
		// free all textures of each object
		AEGfxTextureUnload(pInst->pTex);

		pInst->pMesh = NULL;
		pInst->pTex = NULL;
	}
	return ;
}

/******************************************************************************/
/*!
	Create GameObject from GameObject Pooling
*/
/******************************************************************************/
GameObjInst * GOInstCreate(unsigned long type,
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
void GOInstDestroy(GameObjInst * pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}

/******************************************************************************/
/*!
	Spawn number of asteroids based on parameter on the outskirts of the screen.
*/
/******************************************************************************/
void SpawnActualAsteroid(int Num)
{
	for (int i = 0; i < Num; i++)
	{
		AEVec2 pos{ 0 }, vel{ 0 }, scale{ 0 };
		(AERandFloat() > 0.5f ? -1 : 1);

		float Value = AERandFloat() * (ASTEROID_MAX_SCALE - ASTEROID_MIN_SCALE) + ASTEROID_MIN_SCALE;
        AEVec2Set(&scale, Value, Value);

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
		GOInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);
	}
}