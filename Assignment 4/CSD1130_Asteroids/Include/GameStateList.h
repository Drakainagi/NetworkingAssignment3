/******************************************************************************/
/*!
\file		GameStateList.h
\author 	Soh Wei Jie, weijie.soh, 2301289
\par    	email: weijie.soh\@digipen.edu
\date   	February 08, 2024
\brief		This file handles the game states' names.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_GAME_STATE_LIST_H_
#define CSD1130_GAME_STATE_LIST_H_

// ---------------------------------------------------------------------------
// game state list

enum
{
	// list of all game states 
	GS_ASTEROIDS = 0, 
	GS_ACTUALASTEROIDS = 1,

	// special game state. Do not change
	GS_RESTART,
	GS_QUIT, 
	GS_NONE
};

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_LIST_H_