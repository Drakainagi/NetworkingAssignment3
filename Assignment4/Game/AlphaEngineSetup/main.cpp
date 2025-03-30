//
// Rewrote asteroid with AE-Engine rendering pipeline 
// 
// Written by: Joshua Sim Yue Chen
// 
// Structure written to be as simple as possible to accomodate networking capabilities and pipeline. Need to merge out into server and client cpps with the respective functionalities
// Rendering functional via gameobject list (Left to client to read gameobjectlist from server and render)
// Input handling (Leave in client, instead of updating in client send packets to server)
// Collision should be handled in server 
// 
// TO DO:
// Client only reads and render, and send packet via input and ack.
// Server to receive player commands and update its master gameobject list.
//
// 
// AE-Engine Asteroid Simulation
// Replaces planets with general-purpose GameObjects (e.g., asteroids)
//

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

#define MAX_GAME_OBJECTS 4000

// Constants for player movement
const float MOVE_SPEED = 200.0f;     // pixels per second
const float ROTATE_SPEED = 2.5f;     // radians per second
const float BULLET_SPEED = 400.f;
const float ClientID = 1; //temp


enum ObjectType {
	Player,
	Asteroid,
	Bullet
};

struct GameObject
{
	AEMtx33 transform;
	float pos_x, pos_y;
	float scale;
	float rotation = 0;
	float velocity = 0;
	ObjectType objectType;
	bool isActive = true; //disable on dead
	int bulletOwner = 0; // Client who owns this bullet
};

struct AABB {
	float min_x, min_y;
	float max_x, max_y;
};

//--------------------------- HELPER FUNCTIONS HERE ---------------------
AABB GetAABB(const GameObject& obj)
{
	float halfSize = obj.scale / 2.0f;
	return {
		obj.pos_x - halfSize,
		obj.pos_y - halfSize,
		obj.pos_x + halfSize,
		obj.pos_y + halfSize
	};
}

bool CheckAABBCollision(const GameObject& a, const GameObject& b, float offset)
{
	AABB aBox = GetAABB(a);
	AABB bBox = GetAABB(b);

	// Apply offset: shrink bounding box inward or expand it outward
	aBox.min_x += offset;
	aBox.min_y += offset;
	aBox.max_x -= offset;
	aBox.max_y -= offset;

	bBox.min_x += offset;
	bBox.min_y += offset;
	bBox.max_x -= offset;
	bBox.max_y -= offset;

	// Standard AABB overlap check
	return !(aBox.max_x < bBox.min_x ||
		aBox.min_x > bBox.max_x ||
		aBox.max_y < bBox.min_y ||
		aBox.min_y > bBox.max_y);
}


GameObject gGameObjects[MAX_GAME_OBJECTS];
unsigned int gObjectCount = 0;


// Helper to add a game object to the scene
void AddObject(float x, float y, float scale = 100.0f, ObjectType object = Asteroid , float rotation = 0)
{
	if (gObjectCount >= MAX_GAME_OBJECTS) return;

	GameObject& obj = gGameObjects[gObjectCount];
	obj.pos_x = x;
	obj.pos_y = y;
	obj.scale = scale;
	obj.objectType = object;

	if (object == Asteroid)
	{
		obj.rotation = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;         // random angle
		obj.velocity = 50.0f + static_cast<float>(rand()) / RAND_MAX * 100.0f;   // between 50 and 150
	}
	
	if (object == Bullet) {
		obj.rotation = rotation;
		obj.velocity = BULLET_SPEED;
		obj.bulletOwner = ClientID;
	}
	++gObjectCount;
}

void SpawnRandomAsteroid()
{
	float x = static_cast<float>(rand() % 600 - 300);
	float y = static_cast<float>(rand() % 400 - 200);
	float scale = static_cast<float>(rand() % 40 + 30); // Scale between 30 and 70
	float rotation = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;

	AddObject(x, y, scale, Asteroid, rotation);
}
// COLLISION CHECKS

void HandleAsteroidPlayerCollision(GameObject& asteroid, GameObject& player)
{
	printf("Asteroid collided with Player!\n");

	// Reset player position (could be life decrement instead)
	//player.pos_x = 0;
	//player.pos_y = 0;

	// Deactivate old asteroid and spawn a new one
	asteroid.isActive = false;
	SpawnRandomAsteroid();
}


void HandleAsteroidBulletCollision(GameObject& asteroid, GameObject& bullet)
{
	printf("Bullet hit Asteroid!\n");

	asteroid.isActive = false;
	bullet.isActive = false;

	// Spawn a replacement asteroid
	SpawnRandomAsteroid();
}


// ------------------ HELPER FUNCTION END ----------------------

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, true, NULL);
	AESysSetWindowTitle("Simple Asteroid Renderer");

	// Load quad mesh
	AEGfxVertexList* pMesh = nullptr;
	AEGfxMeshStart();
	AEGfxTriAdd(-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 1.0f,
		0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	pMesh = AEGfxMeshEnd();

	AEGfxTexture* AsteroidTexture = AEGfxTextureLoad("Assets/PlanetTexture.png");
	AEGfxTexture* PlayerTexture = AEGfxTextureLoad("Assets/Player.png");
	AEGfxTexture* BulletTexture = AEGfxTextureLoad("Assets/Fire.png");

	// Add a few asteroids
	AddObject(-200.f, 0.f, 150.f,Asteroid);
	AddObject(100.f, -100.f, 80.f,Asteroid);
	AddObject(300.f, 200.f, 120.f,Player);

	bool gGameRunning = true;
	AEInputShowCursor(1);


	GameObject* player = nullptr;
	for (unsigned int i = 0; i < gObjectCount; ++i)
	{
		if (gGameObjects[i].objectType == Player)
		{
			player = &gGameObjects[i];
			break;
		}
	}

	while (gGameRunning)
	{
		AESysFrameStart();

		// Setup rendering init for client
		AEGfxSetBackgroundColor(0.f, 0.f, 0.f);
		AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxSetTransparency(1.f);
		AEGfxSetColorToMultiply(1, 1, 1, 1);
		AEGfxSetColorToAdd(0, 0, 0, 0);
		
		

		float dt = (float)AEFrameRateControllerGetFrameTime();

		if (player) {

			float angle = player->rotation + (PI / 2.0f); // match render direction
		
		//Input handling // Send packet to server here 

		if (AEInputCheckCurr(AEVK_A))
			player->rotation += ROTATE_SPEED * dt;

		if (AEInputCheckCurr(AEVK_D))
			player->rotation -= ROTATE_SPEED * dt;


		if (AEInputCheckCurr(AEVK_W))
		{
			player->pos_x += cosf(angle) * MOVE_SPEED * dt;
			player->pos_y += sinf(angle) * MOVE_SPEED * dt;
		}

		if (AEInputCheckCurr(AEVK_S))
		{
			player->pos_x -= cosf(angle) * MOVE_SPEED * dt;
			player->pos_y -= sinf(angle) * MOVE_SPEED * dt;
		}


		if (AEInputCheckTriggered(AEVK_SPACE))
		{
			float correctedAngle = player->rotation + (PI / 2.0f); // match visual rotation
			float bulletOffset = player->scale / 2.0f + 10.0f;

			float bulletX = player->pos_x + cosf(correctedAngle) * bulletOffset;
			float bulletY = player->pos_y + sinf(correctedAngle) * bulletOffset;

			AddObject(bulletX, bulletY, 20.0f, Bullet, player->rotation); // Keep original rotation for rendering
		}
	}
		

		//Update asteroids to move
		
		for (unsigned int i = 0; i < gObjectCount; ++i) {
			GameObject& obj = gGameObjects[i];


			if (!obj.isActive)
				continue;

			// -- ASTEROID MOVEMENT ONLY --
			if (obj.objectType == Asteroid || obj.objectType == Bullet) {
				float angle = obj.rotation + (PI / 2.0f); // Correct for sprite facing up
				obj.pos_x += cosf(angle) * obj.velocity * dt;
				obj.pos_y += sinf(angle) * obj.velocity * dt;

			}

			// -- DESTROY BULLETS IF OUT OF BOUNDS --
			if (obj.objectType == Bullet) {
				if (obj.pos_x < AEGfxGetWinMinX() || obj.pos_x > AEGfxGetWinMaxX() ||
					obj.pos_y < AEGfxGetWinMinY() || obj.pos_y > AEGfxGetWinMaxY()) {
					obj.isActive = false;
				}
			}


			// -- WRAPPING LOGIC FOR ALL OBJECTS --
			float halfSize = obj.scale / 2.0f;
			obj.pos_x = AEWrap(obj.pos_x, AEGfxGetWinMinX() - halfSize, AEGfxGetWinMaxX() + halfSize);
			obj.pos_y = AEWrap(obj.pos_y, AEGfxGetWinMinY() - halfSize, AEGfxGetWinMaxY() + halfSize);
		}

		//collision check
		for (unsigned int i = 0; i < gObjectCount; ++i)
		{
			GameObject& obj = gGameObjects[i];


			if (!obj.isActive)
				continue;

			if (obj.objectType != Asteroid)
				continue;

			// ASTEROID COLLISIONS TO OTHER OBJECTS 
			for (unsigned int i = 0; i < gObjectCount; ++i)
			{
				GameObject& obj = gGameObjects[i];

				if (!obj.isActive || obj.objectType != Asteroid)
					continue;

				for (unsigned int j = 0; j < gObjectCount; ++j)
				{
					GameObject& other = gGameObjects[j];
					if (!other.isActive || i == j) continue;

					// Asteroid hits Player
					if (other.objectType == Player && CheckAABBCollision(obj, other, 20.f))
					{
						HandleAsteroidPlayerCollision(obj, other);
					}

					// Asteroid hits Bullet
					else if (other.objectType == Bullet && CheckAABBCollision(obj, other, 0.f))
					{
						HandleAsteroidBulletCollision(obj, other);
					}
				}
			}

			// BULLET COLLISION TO OTHER OBJECTS
			for (unsigned int i = 0; i < gObjectCount; ++i)
			{
				GameObject& obj = gGameObjects[i];
				if (!obj.isActive || obj.objectType != Bullet)
					continue;

				for (unsigned int j = 0; j < gObjectCount; ++j)
				{
					GameObject& other = gGameObjects[j];
					if (!other.isActive || i == j) continue;

					// Bullet hits Player, but not their own bullet
					if (other.objectType == Player && obj.bulletOwner != ClientID && CheckAABBCollision(obj, other, 0.f))
					{
						printf("Enemy bullet hit Player!\n");
						// e.g. reduce health or reset
						other.pos_x = 0;
						other.pos_y = 0;
						obj.isActive = false;
					}
				}
			}

		}
		


		// Render each game object
		for (unsigned int i = 0; i < gObjectCount; ++i)
		{
			GameObject& obj = gGameObjects[i];

			if (!obj.isActive)
				continue;

			AEMtx33 scaleMtx, rotMtx, transMtx;

			AEMtx33Scale(&scaleMtx, obj.scale, obj.scale);
			AEMtx33Rot(&rotMtx, obj.rotation + (PI / 1.0f)); // rotate +90 degrees
			AEMtx33Trans(&transMtx, obj.pos_x, obj.pos_y);

			AEMtx33Concat(&obj.transform, &rotMtx, &scaleMtx);
			AEMtx33Concat(&obj.transform, &transMtx, &obj.transform);


			// Texture set
			switch (obj.objectType)
			{
			case Player:
				AEGfxTextureSet(PlayerTexture, 0, 0);
				break;
			case Bullet:
				AEGfxTextureSet(BulletTexture, 0, 0);
				break;
			case Asteroid:
			default:
				AEGfxTextureSet(AsteroidTexture, 0, 0);
				break;
			}

			AEGfxSetTransform(obj.transform.m);
			AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);
		}


		AESysFrameEnd();

		// Exit conditions
		if (AEInputCheckTriggered(AEVK_ESCAPE) || !AESysDoesWindowExist())
			gGameRunning = false;

		if (AEInputCheckTriggered(AEVK_1)) AESysSetFullScreen(1);
		if (AEInputCheckTriggered(AEVK_2)) AESysSetFullScreen(0);
	}

	AEGfxMeshFree(pMesh);
	AEGfxTextureUnload(AsteroidTexture);
	AEGfxTextureUnload(PlayerTexture);
	AESysExit();
}



