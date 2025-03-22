#include "SceneAsteroid.h"
#include "GL\glew.h"
#include "Application.h"
#include <sstream>

#pragma region Static Variables
int SceneAsteroid::m_lives = 3;
int SceneAsteroid::m_score = 0;
float SceneAsteroid::m_timer = 300.0f;
bool SceneAsteroid::m_bossSpawned = false;
bool SceneAsteroid::m_gameStarted = false;
bool SceneAsteroid::m_gameEnded = false;

float SceneAsteroid::m_enemySpawnRate = 1.0f;
float SceneAsteroid::m_asteroidSpawnRate = 1.0f;
float SceneAsteroid::m_celestialBodySpawnRate = 10.0f;

#pragma endregion

#pragma region Initialization

SceneAsteroid::SceneAsteroid()
{
}

SceneAsteroid::~SceneAsteroid()
{
	Exit();
}

void SceneAsteroid::Init()
{
	SceneBase::Init();

	m_asteroidSpawnRate = 1.0f;
	m_enemySpawnRate = 9999.0f;
	m_celestialBodySpawnRate = 10.0f;

	srand(static_cast<unsigned>(time(nullptr)));
	Math::InitRNG();

	m_worldHeight = Application::GetWindowHeight();
	m_worldWidth = Application::GetWindowWidth(); 

	// Initialize player ship
	m_playerShip = std::make_shared<PlayerShip>();
	m_playerShip->scale = { 40.0f, 40.0f, 40.0f };
	m_playerShip->pos = { m_worldWidth / 2.0f, m_worldHeight / 2.0f, 0 };
	m_playerShip->active = true;
	m_playerShip->mass = 1000000.0f; //Affects getting knocked around
	m_playerShip->health = 90000.0f;

	// Preallocate pooled objects
	for (int i = 0; i < 500; ++i) {
		m_gameObjects.push_back(std::make_shared<GameObject>());
	}
}

/****************************************************************************************************************
Exit
****************************************************************************************************************/
void SceneAsteroid::Exit()
{
	SceneBase::Exit();

	// Clear the game object pool (shared_ptr will handle memory cleanup)
	m_gameObjects.clear();

	// Reset player ship
	m_playerShip.reset();
}
#pragma endregion

#pragma region Helper Func
std::shared_ptr<GameObject> SceneAsteroid::FetchGO()
{
	for (auto& obj : m_gameObjects) 
	{
		if (!obj->active)
		{
			return obj;
		}
	}

	// Expand the pool if necessary
	for (int i = 0; i < 100; ++i) 
	{
		m_gameObjects.push_back(std::make_shared<GameObject>());
	}
	return m_gameObjects.back();
}

std::shared_ptr<GameObject> SceneAsteroid::FetchNearestOBJ(const Vector3& position)
{
	std::shared_ptr<GameObject> nearest = nullptr;
	float minDist = -1.0f;

	for (const auto& obj : m_gameObjects) 
	{
		if (!obj->active) continue;
		float dist = (obj->pos - position).LengthSquared();
		if (dist < minDist) 
		{
			minDist = dist;
			nearest = obj;
		}
	}
	return nearest;
}

//For testing purposes
void SceneAsteroid::ProcessInput()
{
	static bool bLButtonState = false;
	if (!bLButtonState && Application::IsMousePressed(0))
	{
		bLButtonState = true;
	}
	else if (bLButtonState && !Application::IsMousePressed(0))
	{
		bLButtonState = false;
	}
	static bool bRButtonState = false;
	if (!bRButtonState && Application::IsMousePressed(1))
	{
		bRButtonState = true;
		std::cout << "RBUTTON DOWN" << std::endl;
	}
	else if (bRButtonState && !Application::IsMousePressed(1))
	{
		bRButtonState = false;
		std::cout << "RBUTTON UP" << std::endl;
	}
}
#pragma endregion

/****************************************************************************************************************
Update
****************************************************************************************************************/
void SceneAsteroid::Update(float dt)
{
	///////////////////////////////////////////////////////////////////////////////
	// Game not started
	if (!m_gameStarted)
	{
		if (Application::IsKeyPressed(VK_SPACE))
		{
			m_gameStarted = true;
			m_playerShip->health = 100.0f;
			m_playerShip->mass = 5.0f;
			//m_speed = 1.f;
			m_enemySpawnRate = 1.f;
			m_asteroidSpawnRate = 1.f;
			m_celestialBodySpawnRate = 1.f;
		}
		m_playerShip->vel = { 0, 0, 0 };
		return;
	}
	
	///////////////////////////////////////////////////////////////////////////////
	// Game started
	SceneBase::Update(dt);
	m_timer -= dt;
	// Lose Condition
    if (m_playerShip->health <= 0.0f || m_timer <= 0.0f)
	{
		m_gameEnded = (--m_lives == 0);
		m_playerShip->health = 100.f;
		// TODO: Spawn bullets in all directions.
	}

	///////////////////////////////////////////////////////////////////////////////
	// Update obj loop
	for (auto& obj : m_gameObjects)
	{
		if (obj && obj->active)
			obj->update(dt);
	}
	m_playerShip->update(dt);

	///////////////////////////////////////////////////////////////////////////////
	// Spawning OBJ
	if(0)
	{
		m_asteroidSpawnRate -= 0.01f / dt;
		m_enemySpawnRate -= 0.0003f / dt;
		m_celestialBodySpawnRate -= 0.00006f / dt;
		m_playerShip->bulletCooldown--;

		if (m_asteroidSpawnRate <= 0)
		{
			m_asteroidSpawnRate = 1.0f;
			SpawnAsteroid();
		}
		if (m_enemySpawnRate <= 0)
		{
			if (m_score <= 500)
			{
				m_enemySpawnRate = 1.f;
				SpawnEnemy();
			}
			else if (m_score <= 9000)
			{
				m_enemySpawnRate = 99999.f;
#if 0 // TODO
				for (std::vector<GameObject*>::iterator it = m_goBossShips.begin(); it != m_goBossShips.end(); ++it)
				{
					GameObject* OBJ = (GameObject*)*it;
					if (!OBJ->active)
					{
						GameObject* enemy = FetchGO(m_goBossShips);
						if (rand() % 2 == 1)
						{
							RandX = Math::RandFloatMinMax(m_CameraLowestX - enemy->scale.x, (-m_CameraLowestX * 2) - enemy->scale.x);
						}
						else
						{
							RandX = Math::RandFloatMinMax(m_CameraHighestX + enemy->scale.x, m_CameraHighestX * 3 + enemy->scale.x);
						}
						if (rand() % 2 == 1)
						{
							RandY = Math::RandFloatMinMax(m_CameraLowestY - enemy->scale.y, (-m_CameraLowestY * 2) - enemy->scale.y);
						}
						else
						{
							RandY = Math::RandFloatMinMax(m_CameraHighestY + enemy->scale.y, m_CameraHighestY * 3 + enemy->scale.y);
						}
						if (!BossSpawned)
						{
							enemy->active = true;
							enemy->type = GAMEOBJECT_TYPE::GO_BOSS;
							enemy->pos = Vector3{ RandX, RandY,0.0f };
							enemy->health = 3000.0f;
							enemy->angle = 0.0f;
							enemy->scale.Set(15.0f, 30.0f, 5.0f);
							enemy->mass = 20;
							enemy->bulletCooldown = 0.0f;
							enemy->PhaseCooldown = 20.0f;
							BossSpawned = true;
						}
						else
						{
							enemy->active = true;
							enemy->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP;
							enemy->pos = Vector3{ RandX, RandY,0.0f };
							enemy->health = 0.0f;
							enemy->angle = 0.0f;
							enemy->scale.Set(7.5f, 7.5f, 0.0f);
							enemy->target = (GameObject*)*--it;
							enemy->mass = 7.5;
							it++;
						}
					}
				}
#endif
			}
		}
		if (m_celestialBodySpawnRate <= 0)
		{
			m_celestialBodySpawnRate = 1.f;
			SpawnCelestialBody();
		}


		if (m_gameStarted)
		{
#pragma region Player Shooting Controls
			/****************************************************************************************************************
			Mouse Section(Shooting)
			****************************************************************************************************************/
			// Update player ship's internal physics and cooldown timer.
			m_playerShip->bulletCooldown -= 2.f * dt;

			// Lambda to help set common properties for a spawned projectile.
			auto spawnProj = [this](GAMEOBJECT_TYPE type) -> std::shared_ptr<GameObject> {
				auto proj = FetchGO(); // Fetch an inactive game object from the pool.
				proj->active = true;
				proj->pos = m_playerShip->pos;
				proj->type = type;
				return proj;
				};

			// --- Mouse Shooting Section ---
			if (Application::IsMousePressed(0) && m_playerShip->bulletCooldown <= 0)
			{
				// Weapon selection based on m_WeaponChoice:
				switch (m_playerShip->currentWeapon)
				{
				case WeaponType::NORMAL: // Normal Pellets
				{
					auto bullet = spawnProj(GO_BULLET);
					bullet->scale.Set(0.5f, 0.5f, 0.5f);
					bullet->vel = m_playerShip->dir * m_playerShip->m_firedbulletspeed + m_playerShip->vel;
					m_playerShip->bulletCooldown = 10.0f; // Normal fire rate.
					m_playerShip->vel -= bullet->vel / 1000;
					break;
				}
				case WeaponType::MACHINE_GUN: // Machine Gun
				{
					for (int i = 0; i < 2; i++)
					{
						auto bullet = spawnProj(GO_BULLET);
						bullet->scale.Set(0.5f, 0.5f, 0.5f);
						bullet->vel = m_playerShip->dir * m_playerShip->m_firedbulletspeed + m_playerShip->vel +
							Math::RandFloatMinMax(-45.0f, 45.0f);
						m_playerShip->bulletCooldown = 0.001f; // Fast fire rate.
						m_playerShip->vel -= bullet->vel / 1000;
					}
					break;
				}
				case WeaponType::SHOTGUN: // ShotGun
				{
					for (int i = 0; i < 50; i++)
					{
						auto bullet = spawnProj(GO_BULLET);
						bullet->scale.Set(0.5f, 0.5f, 0.5f);
						float sprayAngle = Math::DegreeToRadian((i - 25) * 1.0f);
						bullet->vel.Set(
							m_playerShip->m_firedbulletspeed * cos(m_playerShip->angle + sprayAngle) + m_playerShip->vel.x,
							m_playerShip->m_firedbulletspeed * sin(m_playerShip->angle + sprayAngle) + m_playerShip->vel.y,
							0);
						m_playerShip->bulletCooldown = 50.0f;
						m_playerShip->vel -= bullet->vel / 400;
					}
					break;
				}
				case WeaponType::PULSE_GUN: // Pulse Gun
				{
					for (int i = 0; i < 100; i++)
					{
						auto pulse = spawnProj(GO_PULSEBULLET);
						pulse->scale.Set(2.5f, 2.5f, 2.5f);
						float sprayAngle = Math::DegreeToRadian((i - 50) * 1.0f);
						pulse->vel.Set(
							50 * cos(m_playerShip->angle + sprayAngle) + m_playerShip->vel.x,
							50 * sin(m_playerShip->angle + sprayAngle) + m_playerShip->vel.y,
							0);
						pulse->angle = m_playerShip->angle + sprayAngle;
						m_playerShip->bulletCooldown = 100.0f;
						m_playerShip->vel -= pulse->vel / 150;
					}
					break;
				}
				default:
					break;
				}
			}
#pragma endregion

#if 0 // SHOULD BE MOVED TO Player
			/****************************************************************************************************************
			Game Physics
			****************************************************************************************************************/
			m_playerShip->momentOfInertia = m_playerShip->mass * m_playerShip->scale.x * m_playerShip->scale.x;
			m_playerShip->angularVelocity += (m_playerShip->m_torque.z / m_playerShip->momentOfInertia) * dt;
			m_playerShip->angle += static_cast<float>(m_playerShip->angularVelocity * dt);
			m_playerShip->dir = Vector3(cosf(m_playerShip->angle), sinf(m_playerShip->angle), 0.0f);
			m_playerShip->m_torque.SetZero();

			Vector3 acceleration = m_force * (1 / m_playerShip->mass);
			m_playerShip->vel += acceleration * (m_speed * dt);
			//m_playerShip->pos += m_playerShip->vel*(m_speed * dt);
#endif
		}
#if 0 //TODO move AI over to individual classes
		/***********************************************************************************************************************************
		Asteroids AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goAsteroid.begin(); it != m_goAsteroid.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)*it;
			if (OBJ->active)
			{
				OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
				OBJ->angle += static_cast<float>(OBJ->angularVelocity * dt);
				if (sqrt(pow(OBJ->pos.x - m_playerShip->pos.x, 2) + pow(OBJ->pos.y - m_playerShip->pos.y, 2)) > 200.0f)
				{
					OBJ->active = false;
				}

				//Handle collision between GO_SHIP and GO_ASTEROID
				float combinedRadii = OBJ->scale.x + m_playerShip->scale.x;
				if ((OBJ->pos - m_playerShip->pos).LengthSquared() < combinedRadii)
				{
					float scaleReduc = (30.0f / OBJ->health) * 100;
					OBJ->health -= 30.0f;
					OBJ->scale.x = (OBJ->scale.x / 100) * (100 - scaleReduc);
					OBJ->scale.y = (OBJ->scale.y / 100) * (100 - scaleReduc);
					if (OBJ->health <= 0.0f)
						OBJ->active = false;
					m_playerShip->health -= 5.0f * (OBJ->scale.x / OBJ->OriginalScale);
				}
				for (std::vector<GameObject*>::iterator it2 = m_goShip.begin(); it2 != m_goShip.end(); ++it2)
				{
					GameObject* OBJ2 = (GameObject*)*it2;
					if (OBJ2->active)
					{
						//Handle collision between GO_ENEMY and GO_ASTEROID
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < combinedRadii)
						{
							float scaleReduc = (25.0f / OBJ->health) * 100;
							OBJ->health -= 25.0f;
							OBJ->scale.x = (OBJ->scale.x / 100) * (100 - scaleReduc);
							OBJ->scale.y = (OBJ->scale.y / 100) * (100 - scaleReduc);
							if (OBJ->health <= 0.0f)
								OBJ->active = false;
							if (!BossSpawned && OBJ2->type == GAMEOBJECT_TYPE::GO_ENEMYSHIP)
							{
								OBJ2->health -= 20.0f * (OBJ->scale.x / OBJ->OriginalScale);
								if (OBJ2->health <= 0.0f)
								{
									OBJ2->active = false;
								}
							}
						}
					}
				}
			}
		}
		/***********************************************************************************************************************************
		Planets & Black Holes AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goCelestialBodies.begin(); it != m_goCelestialBodies.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)*it;
			if (OBJ->active)
			{
				OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
				if (sqrt(pow(OBJ->pos.x - m_playerShip->pos.x, 2) + pow(OBJ->pos.y - m_playerShip->pos.y, 2)) > 200.0f)
				{
					OBJ->active = false;
				}

				float combinedRadii = OBJ->scale.x + m_playerShip->scale.x + 10.0f;
				if ((OBJ->pos - m_playerShip->pos).LengthSquared() > combinedRadii)
					m_playerShip->vel += (OBJ->pos - m_playerShip->pos).Normalized() * (OBJ->mass / (OBJ->pos - m_playerShip->pos).LengthSquared());

				if (OBJ->type == GAMEOBJECT_TYPE::GO_PLANET1
					|| OBJ->type == GAMEOBJECT_TYPE::GO_PLANET2
					|| OBJ->type == GAMEOBJECT_TYPE::GO_PLANET3
					|| OBJ->type == GAMEOBJECT_TYPE::GO_PLANET4)
				{
					//Asteroid
					for (int i = 0; i < m_goAsteroid.size(); ++i)
					{
						GameObject* OBJ2 = m_goAsteroid[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
						else
						{
							float scaleReduc = (1.0f / OBJ2->health) * 100;
							OBJ2->health -= 1.0f;
							OBJ2->scale.x = (OBJ2->scale.x / 100) * (100 - scaleReduc);
							OBJ2->scale.y = (OBJ2->scale.y / 100) * (100 - scaleReduc);
							if (OBJ2->health <= 0.0f)
								OBJ2->active = false;
						}
					}
					//Enemies
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
						else
						{
							OBJ2->health -= 5.0f;
							if (OBJ2->health <= 0.0f)
								OBJ2->active = false;
						}
					}
					//Bullets
					for (int i = 0; i < m_goBullet.size(); ++i)
					{
						GameObject* OBJ2 = m_goBullet[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
					}
				}

				if (OBJ->type == GAMEOBJECT_TYPE::GO_BLACKHOLE)
				{
					if ((OBJ->pos - m_playerShip->pos).LengthSquared() < combinedRadii)
						m_playerShip->health -= 10.0f * dt;
					//Asteroid
					for (int i = 0; i < m_goAsteroid.size(); ++i)
					{
						GameObject* OBJ2 = m_goAsteroid[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
						else
						{
							float scaleReduc = (20.0f / OBJ2->health) * 100;
							OBJ2->health -= 20.0f;
							OBJ2->scale.x = (OBJ2->scale.x / 100) * (100 - scaleReduc);
							OBJ2->scale.y = (OBJ2->scale.y / 100) * (100 - scaleReduc);
							if (OBJ2->health <= 0.0f)
								OBJ2->active = false;
						}
					}
					//Enemies
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
						else
						{
							OBJ2->health -= 5.0f * dt;
							OBJ2->vel = OBJ2->vel / 2;
							if (OBJ2->health <= 0.0f)
							{
								OBJ2->active = false;
							}
						}
					}
					//Bullets
					for (int i = 0; i < m_goBullet.size(); ++i)
					{
						GameObject* OBJ2 = m_goBullet[i];
						float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
						if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
							OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
						else
						{
							OBJ2->active = false;
						}

					}
					for (int i = 0; i < m_goCelestialBodies.size(); ++i)
					{
						GameObject* OBJ2 = m_goCelestialBodies[i];
						if (OBJ2->type == GAMEOBJECT_TYPE::GO_PLANET1
							|| OBJ2->type == GAMEOBJECT_TYPE::GO_PLANET2
							|| OBJ2->type == GAMEOBJECT_TYPE::GO_PLANET3
							|| OBJ2->type == GAMEOBJECT_TYPE::GO_PLANET4)
						{
							float combinedRadii = OBJ->scale.x + OBJ2->scale.x;
							if ((OBJ->pos - OBJ2->pos).LengthSquared() > combinedRadii)
								OBJ2->vel += (OBJ->pos - OBJ2->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ2->pos).LengthSquared());
							else
							{
								float scaleReduc = (20.0f / OBJ2->health) * 100;
								OBJ2->health -= 20.0f;
								OBJ2->scale.x = (OBJ2->scale.x / 100) * (100 - scaleReduc);
								OBJ2->scale.y = (OBJ2->scale.y / 100) * (100 - scaleReduc);
								if (OBJ2->health <= 0.0f)
									OBJ2->active = false;
							}
						}
					}
				}

			}
		}

		/***********************************************************************************************************************************
		Enemies AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goShip.begin(); it != m_goShip.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)*it;
			if (OBJ->active)
			{
				if (OBJ->type == GAMEOBJECT_TYPE::GO_ENEMYSHIP)
				{
					OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
					//Enemy AI
					OBJ->bulletCooldown -= 1.f * dt;
					OBJ->dir = (m_playerShip->pos - OBJ->pos).Normalized();
					OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					OBJ->vel += Vector3{ (m_playerShip->pos.x - OBJ->pos.x) / 200, (m_playerShip->pos.y - OBJ->pos.y) / 200, 0.0f };
					if (OBJ->vel.x > 25.0f)
					{
						OBJ->vel.x = 25.0f;
					}
					else if (OBJ->vel.x < -25.0f)
					{
						OBJ->vel.x = -25.0f;
					}
					if (OBJ->vel.y > 25.0f)
					{
						OBJ->vel.y = 25.0f;
					}
					else if (OBJ->vel.y < -25.0f)
					{
						OBJ->vel.y = -25.0f;
					}
					if (OBJ->bulletCooldown <= 0)
					{
						GameObject* EnemyBullet = FetchGO(m_goBullet);
						EnemyBullet->active = true;
						EnemyBullet->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET;
						EnemyBullet->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet->pos = OBJ->pos;
						EnemyBullet->vel = OBJ->dir * BULLET_SPEED + OBJ->vel;
						OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;
					}
				}
				else if (OBJ->type == GAMEOBJECT_TYPE::GO_GUARDIAN)
				{
					if (OBJ->target != NULL)
					{
						OBJ->dir = (OBJ->target->pos - OBJ->pos).Normalized();
						OBJ->vel += Vector3{ (m_playerShip->pos.x - OBJ->pos.x) / 30, (m_playerShip->pos.y - OBJ->pos.y) / 30, 0.0f };
					}
					else if (OBJ->target == NULL)
					{
						OBJ->target = FetchNearestOBJ(OBJ->pos);
					}
					OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
					//Enemy AI
					OBJ->bulletCooldown -= 1.f * dt;
					OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					if (OBJ->vel.x > 80.0f)
					{
						OBJ->vel.x = 80.0f;
					}
					else if (OBJ->vel.x < -80.0f)
					{
						OBJ->vel.x = -80.0f;
					}
					if (OBJ->vel.y > 80.0f)
					{
						OBJ->vel.y = 80.0f;
					}
					else if (OBJ->vel.y < -80.0f)
					{
						OBJ->vel.y = -80.0f;
					}
					if (OBJ->bulletCooldown <= 0)
					{
						for (int i = 0; i < 20; i++)
						{
							GameObject* AllyBullet = FetchGO(m_goBullet);
							AllyBullet->active = true;
							AllyBullet->type = GAMEOBJECT_TYPE::GO_BULLET;
							AllyBullet->scale.Set(0.5f, 0.5f, 0.5f);
							AllyBullet->pos = OBJ->pos;
							float sprayangle = (i - (20 / 2)) * 1;
							sprayangle = Math::DegreeToRadian(sprayangle);
							AllyBullet->vel.Set((BULLET_SPEED * cos(OBJ->angle + sprayangle)) + OBJ->vel.x, (BULLET_SPEED * sin(OBJ->angle + sprayangle)) + OBJ->vel.y, 0);
							OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;
							OBJ->vel -= AllyBullet->vel / 400;
						}
					}
				}
			}
		}
#if 0  //TODO REINTRODUCE this once everything is fixed
		/***********************************************************************************************************************************
		BOSS AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goBossShips.begin(); it != m_goBossShips.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)*it;
			if (OBJ->active)
			{
				static bool PhaseShift = false;
				if (OBJ->type == GAMEOBJECT_TYPE::GO_BOSS)
				{
					if (!PhaseShift)
					{
						//Enemy AI
						OBJ->vel += Vector3{ (m_playerShip->pos.x - OBJ->pos.x) / 50, (m_playerShip->pos.y - OBJ->pos.y) / 50, 0.0f };
						Vector3 OldPos = OBJ->pos;
						OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
						OBJ->dir = ((OBJ->pos - OldPos).Normalized());
						OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					}
					else if (PhaseShift)
					{
						//Enemy AI
						OBJ->vel += Vector3{ (m_playerShip->pos.x - OBJ->pos.x) / 80, (m_playerShip->pos.y - OBJ->pos.y) / 80, 0.0f };
						OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
						OBJ->bulletCooldown -= 1.f * dt;
						OBJ->dir = (m_playerShip->pos - OBJ->pos).Normalized();
						OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					}

					OBJ->PhaseCooldown -= 1.f * dt;
					//OBJ->bulletCooldown -= 1.f * dt;
					if (OBJ->PhaseCooldown <= 0.0f && !PhaseShift)
					{
						PhaseShift = true;
						for (unsigned int it = 1; it < m_goBossShips.size(); it++)
						{
							if (m_goBossShips[it]->active == true)
							{
								m_goBossShips[it]->vel += { Math::RandFloatMinMax(-200.0f, 200.0f), Math::RandFloatMinMax(-200.0f, 200.0f), 0.0f };
							}
						}
						OBJ->PhaseCooldown = 20.0f;
					}
					else if (OBJ->PhaseCooldown <= 0.0f && PhaseShift)
					{
						PhaseShift = false;
						OBJ->PhaseCooldown = 20.0f;
					}

					//OBJ->pos = { m_worldWidth/2,m_worldHeight / 2,0 };Testing
					if (OBJ->vel.x > 100.0f)
					{
						OBJ->vel.x = 100.0f;
					}
					else if (OBJ->vel.x < -100.0f)
					{
						OBJ->vel.x = -100.0f;
					}
					if (OBJ->vel.y > 100.0f)
					{
						OBJ->vel.y = 100.0f;
					}
					else if (OBJ->vel.y < -100.0f)
					{
						OBJ->vel.y = -100.0f;
					}
					if (PhaseShift)
					{
						for (int i = 0; i < 5; i++)
						{
							GameObject* object = FetchGO(m_goBullet);
							object->active = true;
							object->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET;
							object->scale.Set(0.5f, 0.5f, 0.5f);
							object->pos = OBJ->pos;
							object->vel = OBJ->dir * BULLET_SPEED + OBJ->vel + Math::RandFloatMinMax(-60.0f, 60.0f);
						}
						//OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;
					}
				}
				else
				{
					if (PhaseShift)
					{
						OBJ->vel += Vector3{ (m_playerShip->pos.x - OBJ->pos.x) / 30, (m_playerShip->pos.y - OBJ->pos.y) / 30, 0.0f };
						OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
						OBJ->bulletCooldown -= 1.f * dt;
						OBJ->dir = (m_playerShip->pos - OBJ->pos).Normalized();
						OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					}
					else
					{
						if (OBJ->target != NULL)
						{
							//const Vector3 newDir = (OBJ->target->pos - OBJ->pos).Normalized();
							//OBJ->dir = (0.975f * OBJ->dir + 0.025f * newDir).Normalized();
							//OBJ->vel += OBJ->dir * MISSILE_SPEED;

							OBJ->dir = (OBJ->target->pos - OBJ->pos).Normalized();
							OBJ->vel += Vector3{ (OBJ->target->pos.x - OBJ->pos.x) * 7, (OBJ->target->pos.y - OBJ->pos.y) * 7, 0.0f };
						}
						OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
						//Enemy AI
						OBJ->bulletCooldown -= 1.f * dt;
						OBJ->dir = (OBJ->target->pos - OBJ->pos).Normalized();
						OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					}

					if (OBJ->vel.x > 50.0f)
					{
						OBJ->vel.x = 50.0f;
					}
					else if (OBJ->vel.x < -50.0f)
					{
						OBJ->vel.x = -50.0f;
					}
					if (OBJ->vel.y > 50.0f)
					{
						OBJ->vel.y = 50.0f;
					}
					else if (OBJ->vel.y < -50.0f)
					{
						OBJ->vel.y = -50.0f;
					}
					if (OBJ->bulletCooldown <= 0)
					{
						GameObject* EnemyBullet1 = FetchGO(m_goBullet);
						EnemyBullet1->active = true;
						EnemyBullet1->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET;
						EnemyBullet1->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet1->pos = OBJ->pos;
						EnemyBullet1->vel = OBJ->dir * 1 / 2 * BULLET_SPEED + OBJ->vel;
						OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;

						GameObject* EnemyBullet2 = FetchGO(m_goBullet);
						EnemyBullet2->active = true;
						EnemyBullet2->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET;
						EnemyBullet2->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet2->pos = OBJ->pos;
						EnemyBullet2->vel = OBJ->dir * -1 / 2 * BULLET_SPEED + OBJ->vel;
					}

				}
			}
		}
#endif
		/***********************************************************************************************************************************
		Bullet Collision
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goBullet.begin(); it != m_goBullet.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)*it;
			if (OBJ->active)
			{
				//Collision check between GO_BULLET and GO_ASTEROID
				for (int i = 0; i < m_goAsteroid.size(); ++i)
				{
					GameObject* OBJ2 = m_goAsteroid[i];
					if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
					{
						OBJ->active = false;
						float scaleReduc = (40.0f / OBJ2->health) * 100;
						OBJ2->health -= 40.0f;
						OBJ2->scale.x = (OBJ2->scale.x / 100) * (100 - scaleReduc);
						OBJ2->scale.y = (OBJ2->scale.y / 100) * (100 - scaleReduc);
						if (OBJ2->health <= 0.0f)
							OBJ2->active = false;
						if (OBJ->type == GAMEOBJECT_TYPE::GO_BULLET)
						{
							m_score += 2;
						}
					}
				}

				//Handle collision between GO_BULLET and m_playerShip using simple distance-based check
				if (OBJ->type == GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * dt - m_playerShip->vel * (m_speed * dt);
					float combinedRadii = OBJ->scale.x + m_playerShip->scale.x;
					if ((OBJ->pos - m_playerShip->pos).LengthSquared() < combinedRadii)
					{
						OBJ->active = false;
						m_playerShip->health -= 5.0f;
					}
				}
				//Handle collision between GO_BULLET and enemy ships using simple distance-based check
				else if (OBJ->type == GAMEOBJECT_TYPE::GO_BULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * dt - m_playerShip->vel * (m_speed * dt);
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GAMEOBJECT_TYPE::GO_GUARDIAN)
						{
							OBJ->active = false;
							OBJ2->health -= 10.0f;
							if (OBJ2->health <= 0.0f)
							{
								m_score += 2;
								OBJ2->active = false;
							}
						}
						if (BossSpawned)
						{
							for (int i = 0; i < m_goBossShips.size(); ++i)
							{
								GameObject* OBJ2 = m_goBossShips[i];
								if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
								{
									OBJ->active = false;
									std::vector<GameObject*>::iterator it = m_goBossShips.begin();
									OBJ2 = static_cast<GameObject*>(*it);
									OBJ2->health -= 0.1f;
									OBJ2->BossDamage += 0.1f;
									if (OBJ2->health <= 0.0f)
									{
										m_score += 9000;
										OBJ2->active = false;
										EnemySpawnRate = 1.f;
										BossSpawned = false;
									}
									if (OBJ2->BossDamage >= 60.0f)
									{
										OBJ2->BossDamage = 0.0f;
										for (unsigned int it = m_goBossShips.size() - 1; it > 0; it--)
										{
											if (m_goBossShips[it]->active == true)
											{
												m_goBossShips[it]->active = false;
												OBJ2->mass -= 0.2;
												break;
											}
										}
									}
								}
							}
						}

					}
				}
				else if (OBJ->type == GAMEOBJECT_TYPE::GO_PULSEBULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * dt - m_playerShip->vel * (m_speed * dt);
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GAMEOBJECT_TYPE::GO_GUARDIAN)
						{
							OBJ2->vel = OBJ->vel * 2;
						}
					}
					for (int i = 0; i < m_goAsteroid.size(); ++i)
					{
						GameObject* OBJ2 = m_goAsteroid[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
						{
							OBJ2->vel = OBJ->vel * 2;
						}
					}
					if (BossSpawned)
					{
						for (int i = 0; i < m_goBossShips.size(); ++i)
						{
							GameObject* OBJ2 = m_goBossShips[i];
							if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
							{
								std::vector<GameObject*>::iterator it = m_goBossShips.begin();
								OBJ2 = static_cast<GameObject*>(*it);
								OBJ2->vel = OBJ->vel * 2;
							}
						}
					}
					for (int i = 0; i < m_goBullet.size(); ++i)
					{
						GameObject* OBJ2 = m_goBullet[i];
						if (OBJ2->type == GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET)
						{
							if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
							{
								OBJ2->vel = OBJ->vel * 2;
							}
						}
					}
				}
				else if (OBJ->type == GAMEOBJECT_TYPE::GO_MISSILE)
				{
					if (OBJ->target != NULL)
					{
						//const Vector3 newDir = (OBJ->target->pos - OBJ->pos).Normalized();
						//OBJ->dir = (0.975f * OBJ->dir + 0.025f * newDir).Normalized();
						//OBJ->vel += OBJ->dir * MISSILE_SPEED;

						OBJ->dir = (OBJ->target->pos - OBJ->pos).Normalized();
						OBJ->vel += Vector3{ (OBJ->target->pos.x - OBJ->pos.x) / 5, (OBJ->target->pos.y - OBJ->pos.y) / 5, 0.0f };
						//OBJ->vel += (OBJ->pos - OBJ->target->pos).Normalized() * (OBJ->mass / (OBJ->pos - OBJ->target->pos).LengthSquared());
					}
					else if (OBJ->target == NULL)
					{
						OBJ->target = FetchNearestOBJ(OBJ->pos);
					}
					OBJ->health -= 0.01f * dt;
					OBJ->pos += OBJ->vel * dt - m_playerShip->vel * (m_speed * dt);
					if (OBJ->health <= 0.0f)
					{
						OBJ = false;
					}
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GAMEOBJECT_TYPE::GO_GUARDIAN)
						{
							if (OBJ->target == OBJ2)
							{
								OBJ->target = NULL;
							}
							OBJ->active = false;
							OBJ2->health -= 10.0f * MISSILE_POWER;
							if (OBJ2->health <= 0.0f)
							{
								m_score += 2;
								OBJ2->active = false;
							}
						}
					}
					for (int i = 0; i < m_goAsteroid.size(); ++i)
					{
						GameObject* OBJ2 = m_goAsteroid[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
						{
							if (OBJ->target == OBJ2)
							{
								OBJ->target = NULL;
							}
							OBJ->active = false;
							float scaleReduc = (50.0f / OBJ2->health) * 100;
							OBJ2->health -= 50.0f;
							OBJ2->scale.x = (OBJ2->scale.x / 100) * (100 - scaleReduc);
							OBJ2->scale.y = (OBJ2->scale.y / 100) * (100 - scaleReduc);
							if (OBJ2->health <= 0.0f)
							{
								OBJ2->active = false;
								m_score += 2;
							}
						}
					}
					if (BossSpawned)
					{
						for (int i = 0; i < m_goBossShips.size(); ++i)
						{
							GameObject* OBJ2 = m_goBossShips[i];
							if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
							{
								OBJ->active = false;
								std::vector<GameObject*>::iterator it = m_goBossShips.begin();
								OBJ2 = static_cast<GameObject*>(*it);
								OBJ2->health -= 0.1f;
								OBJ2->BossDamage += 0.1f;
								if (OBJ2->health <= 0.0f)
								{
									m_score += 9000;
									OBJ2->active = false;
									EnemySpawnRate = 1.f;
									BossSpawned = false;
								}
								if (OBJ2->BossDamage >= 60.0f)
								{
									OBJ2->BossDamage = 0.0f;
									for (unsigned int it = m_goBossShips.size() - 1; it > 0; it--)
									{
										if (m_goBossShips[it]->active == true)
										{
											m_goBossShips[it]->active = false;
											OBJ2->mass -= 0.2;
											break;
										}
									}
								}
							}
						}
					}
				}
				//Unspawning Bullets out of screen
				if (OBJ->pos.x > m_CameraHighestX + OBJ->scale.x || OBJ->pos.y > m_CameraHighestY + OBJ->scale.y || OBJ->pos.x < m_CameraLowestX - OBJ->scale.x || OBJ->pos.y < m_CameraLowestY - OBJ->scale.y)
				{
					OBJ->active = false;
				}
			}
		}
#endif
	}

}

#pragma region TODO: Server Side
void SceneAsteroid::SpawnAsteroid()
{
	std::shared_ptr<GameObject> asteroid = FetchGO();
	asteroid->active = true;
	asteroid->type = GAMEOBJECT_TYPE::GO_ASTEROID;
	asteroid->pos = Vector3{ Math::RandFloatMinMax(-m_worldWidth, m_worldWidth), Math::RandFloatMinMax(-m_worldHeight, m_worldHeight), 0.0f };
	float scale = Math::RandFloatMinMax(5.0f, 15.0f);
	asteroid->scale.Set(scale, scale, 0.0f);
	asteroid->health = 10.0f * scale;
    asteroid->vel = Vector3{ Math::RandFloatMinMax(-10.0f, 10.0f), Math::RandFloatMinMax(-10.0f, 10.0f), 0.0f };
}

void SceneAsteroid::SpawnEnemy()
{
	std::shared_ptr<GameObject> enemy = FetchGO();
	enemy->active = true;
	enemy->type = GAMEOBJECT_TYPE::GO_ENEMYSHIP;
	enemy->pos = Vector3{ Math::RandFloatMinMax(-m_worldWidth, m_worldWidth), Math::RandFloatMinMax(-m_worldHeight, m_worldHeight), 0.0f };
	enemy->scale.Set(5.0f, 5.0f, 5.0f);
	enemy->health = 50.0f;
	enemy->angle = 0.0f;
}

void SceneAsteroid::SpawnCelestialBody()
{
	std::shared_ptr<GameObject> celestial = FetchGO();
	celestial->active = true;
	celestial->type = (rand() % 10 == 9) ? GAMEOBJECT_TYPE::GO_BLACKHOLE : GAMEOBJECT_TYPE::GO_PLANET;
	celestial->pos = Vector3{ Math::RandFloatMinMax(-m_worldWidth * 1.5, m_worldWidth * 1.5), Math::RandFloatMinMax(-m_worldHeight * 1.5, m_worldHeight * 1.5), 0.0f };
	float scale = (celestial->type == GAMEOBJECT_TYPE::GO_BLACKHOLE) ? Math::RandFloatMinMax(10.0f, 80.0f) : Math::RandFloatMinMax(30.0f, 60.0f);
	celestial->scale.Set(scale, scale, 0.0f);
	celestial->mass = (celestial->type == GAMEOBJECT_TYPE::GO_BLACKHOLE) ? 30 * scale : 10 * scale;
	celestial->health = (celestial->type == GAMEOBJECT_TYPE::GO_PLANET) ? 20 * scale : 0.0f;
	celestial->vel = { 0, 0, 0 };
}

void SceneAsteroid::SyncStateToClients()
{
	// Serialize and send game state to clients
}

#pragma endregion

#pragma region TODO: Client Side
void SceneAsteroid::SyncStateFromServer()
{
	// Receive and apply updates from the server
}
#pragma endregion

#pragma region Rendering (PRIMITIVE)
/****************************************************************************************************************
Rendering
****************************************************************************************************************/
void SceneAsteroid::RenderGO(GameObject *go)
{
	switch(go->type)
	{
	case GAMEOBJECT_TYPE::GO_SHIP:
		modelStack.PushMatrix();
		modelStack.Translate(m_playerShip->pos.x, m_playerShip->pos.y, m_playerShip->pos.z);
		modelStack.Scale(m_playerShip->scale.x, m_playerShip->scale.y, m_playerShip->scale.z);
		modelStack.Rotate(Math::RadianToDegree(m_playerShip->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_SHIP], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_ASTEROID:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_Asteroid], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_BULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_BALL], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_PULSEBULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_PULSEBULLET], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_ENEMYSHIP:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_ENEMY], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_BOSS:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_BOSS], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_ENEMYSHIP_BULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_ENEMYBALL], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_PLANET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_PLANET], false);
		modelStack.PopMatrix();
		break;
	case GAMEOBJECT_TYPE::GO_BLACKHOLE:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_BLACKHOLE], false);
		modelStack.PopMatrix();
		break;
	}
}

void SceneAsteroid::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// Projection matrix : Orthographic Projection
	Mtx44 projection;
	projection.SetToOrtho(0, m_worldWidth, 0, m_worldHeight, -10, 10);
	projectionStack.LoadMatrix(projection);
	
	// Camera matrix
	viewStack.LoadIdentity();
	viewStack.LookAt(
						camera.position.x, camera.position.y, camera.position.z,
						camera.target.x, camera.target.y, camera.target.z,
						camera.up.x, camera.up.y, camera.up.z
					);
	// Model matrix : an identity matrix (model will be at the origin)
	modelStack.LoadIdentity();
	
	RenderMesh(meshList[GEO_AXES], false);

	//Render list
	for (const auto& obj : m_gameObjects)
	{
		if (obj->active)
		{
			RenderGO(obj.get());
		}
	}

	//On screen text
	if (m_gameStarted && !m_gameEnded)
	{
		RenderGO(m_playerShip.get());
		modelStack.PushMatrix();
		modelStack.Translate(m_worldWidth / 2, m_worldHeight / 2, 0.0f);
		modelStack.Scale(m_worldWidth, m_worldHeight, 0.0f);
		RenderMesh(meshList[GEO_GAMESCRN], false);
		//Health
		modelStack.PushMatrix();
		modelStack.Translate(-0.34f, -0.45f, 0.0f);
		if (m_lives == 2 || m_lives == 1)
		{
			modelStack.PushMatrix();
			if (m_lives == 1)
			{
				modelStack.Translate(-0.085f / 100 * (100 - m_playerShip->health), 0.0f, 0.0f);
				modelStack.Scale(0.17f / 100 * m_playerShip->health, 0.05f, 0.0f);
			}
			else
				modelStack.Scale(0.17f, 0.05f, 0.0f);
			RenderMesh(meshList[GEO_HEALTHRED], false);
			modelStack.PopMatrix();
		}
		if (m_lives == 3 || m_lives == 2)
		{
			modelStack.PushMatrix();
			if (m_lives == 2)
			{
				modelStack.Translate(-0.085f / 100 * (100 - m_playerShip->health), 0.0f, 0.0f);
				modelStack.Scale(0.17f / 100 * m_playerShip->health, 0.05f, 0.0f);
			}
			else
				modelStack.Scale(0.17f, 0.05f, 0.0f);
			RenderMesh(meshList[GEO_HEALTHYELLOW], false);
			modelStack.PopMatrix();
		}
		if (m_lives == 3)
		{
			modelStack.PushMatrix();
			modelStack.Translate(-0.085f / 100 * (100 - m_playerShip->health), 0.0f, 0.0f);
			modelStack.Scale(0.17f / 100 * m_playerShip->health, 0.05f, 0.0f);
			RenderMesh(meshList[GEO_HEALTHGREEN], false);
			modelStack.PopMatrix();
		}
		modelStack.PopMatrix();
		//WeaponLocks
		//if (!m_WeaponUnlock1)
		//{
		//	modelStack.PushMatrix();
		//	modelStack.Translate(-0.108f, -0.438f, 0.0f);
		//	modelStack.Scale(0.03f, 0.07f, 0.0f);
		//	RenderMesh(meshList[GEO_LOCKED], false);
		//	modelStack.PopMatrix();
		//}
		//if (!m_WeaponUnlock2)
		//{
		//	modelStack.PushMatrix();
		//	modelStack.Translate(0.025f, -0.438f, 0.0f);
		//	modelStack.Scale(0.03f, 0.07f, 0.0f);
		//	RenderMesh(meshList[GEO_LOCKED], false);
		//	modelStack.PopMatrix();
		//}
		//if (!m_WeaponUnlock3)
		//{
		//	modelStack.PushMatrix();
		//	modelStack.Translate(0.157f, -0.438f, 0.0f);
		//	modelStack.Scale(0.03f, 0.07f, 0.0f);
		//	RenderMesh(meshList[GEO_LOCKED], false);
		//	modelStack.PopMatrix();
		//}
		//if (!m_WeaponUnlock4)
		//{
		//	modelStack.PushMatrix();
		//	modelStack.Translate(0.285f, -0.438f, 0.0f);
		//	modelStack.Scale(0.03f, 0.07f, 0.0f);
		//	RenderMesh(meshList[GEO_LOCKED], false);
		//	modelStack.PopMatrix();
		//}
		modelStack.PopMatrix();

		std::ostringstream ss;
		//ss.str("");
		ss.precision(1);
		//ss << "Lives: " << m_playerShip->health;//m_lives
		//RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0, 1, 0), 3, 0, 21);

		ss.str("");
		ss << "Score: " << m_score;
		RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0.2, 1, 0.7), 2.5, 2.5f, 57.5f);

		ss.str("");
		ss << static_cast<int>(m_timer);
		RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0.2, 1, 0.7), 2.5, 38.0f, 57.5f);

		ss.str("");
		ss.precision(3);
		ss << "FPS: " << fps;
		RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0.2, 1, 0.7), 2.5, 62.0f, 57.5f);

	}

	if (!m_gameStarted)
	{
		modelStack.PushMatrix();
		modelStack.Translate(m_worldWidth / 2, m_worldHeight / 2, 0.0f);
		modelStack.Scale(m_worldWidth, m_worldHeight, 0.0f);
		RenderMesh(meshList[GEO_MENU], false);
		modelStack.PopMatrix();
	}
	if (m_gameEnded)
	{
		modelStack.PushMatrix();
		modelStack.Translate(m_worldWidth / 2, m_worldHeight / 2, 0.0f);
		modelStack.Scale(m_worldWidth, m_worldHeight, 0.0f);
		RenderMesh(meshList[GEO_GAMEOVER], false);
		modelStack.PopMatrix();
		std::ostringstream ss;
		ss.precision(1);
		ss.str("");
		ss << "Score: " << m_score;
		RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0.2, 1, 0.7), 5.0f, 18.5f, 12.5f);
		ss.str("");
		ss << "Press ESC to exit";
		RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0.5, 1, 0.7), 2.5f, 17.5f, 6.5f);
	}
}

#pragma endregion