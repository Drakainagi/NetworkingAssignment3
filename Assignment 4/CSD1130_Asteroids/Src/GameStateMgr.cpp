/******************************************************************************/
/*!
\file		GameStateMgr.cpp
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		This file contains the functions and variables for the GameState Manager.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

// ---------------------------------------------------------------------------
// globals

// variables to keep track the current, previous and next game state
unsigned int	gGameStateInit;
unsigned int	gGameStateCurr;
unsigned int	gGameStatePrev;
unsigned int	gGameStateNext;

// pointer to functions for game state life cycles functions
void (*GameStateLoad)()		= 0;
void (*GameStateInit)()		= 0;
void (*GameStateUpdate)()	= 0;
void (*GameStateDraw)()		= 0;
void (*GameStateFree)()		= 0;
void (*GameStateUnload)()	= 0;

/******************************************************************************/
/*!
	"Initialize" game state
*/
/******************************************************************************/
void GameStateMgrInit(unsigned int gameStateInit)
{
	// set the initial game state
	gGameStateInit = gameStateInit;

	// reset the current, previous and next game
	gGameStateCurr = 
	gGameStatePrev = 
	gGameStateNext = gGameStateInit;

	// call the update to set the function pointers
	GameStateMgrUpdate();
}

/******************************************************************************/
/*!
	"Initialize" game state
*/
/******************************************************************************/
void GameStateMgrUpdate()
{
	if ((gGameStateCurr == GS_RESTART) || (gGameStateCurr == GS_QUIT))
		return;

	switch (gGameStateCurr)
	{
	case GS_ASTEROIDS:
		GameStateLoad = GameStateAsteroidsLoad;
		GameStateInit = GameStateAsteroidsInit;
		GameStateUpdate = GameStateAsteroidsUpdate;
		GameStateDraw = GameStateAsteroidsDraw;
		GameStateFree = GameStateAsteroidsFree;
		GameStateUnload = GameStateAsteroidsUnload;
		break;
	case GS_ACTUALASTEROIDS:
		GameStateLoad = GameStateActualAsteroidsLoad;
		GameStateInit = GameStateActualAsteroidsInit;
		GameStateUpdate = GameStateActualAsteroidsUpdate;
		GameStateDraw = GameStateActualAsteroidsDraw;
		GameStateFree = GameStateActualAsteroidsFree;
		GameStateUnload = GameStateActualAsteroidsUnload;
		break;
	default:
		AE_FATAL_ERROR("invalid state!!");
	}
}
