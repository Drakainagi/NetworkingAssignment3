#include "SceneAsteroid.h"
#include "GL\glew.h"
#include "Application.h"
#include <sstream>
#define PI 3.14159265

//Collision improvement
//Damage animation
//Life lost Send out a pulsar
//Graphic Rendering for gameplay
//Title Screen
//Game Over Screen
//Boss improvement

SceneAsteroid::SceneAsteroid()
{
}

SceneAsteroid::~SceneAsteroid()
{
}

void SceneAsteroid::Init()
{
	SceneBase::Init();

	AsteroidSpawnRate = 1.0f;
	EnemySpawnRate = 9999.0f;
	PowerUpSpawnRate = 9999.0f;
	CelestialBodySpawnRate = 10.0f;

	srand(time(NULL));
	WorldBackGroundNumber = Math::RandIntMinMax(1,3);

	Math::InitRNG();

	/****************************************************************************************************************
	Construct OBJ
	****************************************************************************************************************/
	for (int i = 0; i < 50; ++i)
	{
		m_goShip.push_back(new GameObject(GameObject::GO_ENEMYSHIP));
	}
	for (int i = 0; i < 50; ++i) // later change to 40 less;
	{
		m_goBossShips.push_back(new GameObject(GameObject::GO_ENEMYSHIP));
	}
	for (int i = 0; i < 200; ++i)
	{
		m_goAsteroid.push_back(new GameObject(GameObject::GO_ASTEROID));
	}
	for (int i = 0; i < 300; ++i)
	{
		m_goBullet.push_back(new GameObject(GameObject::GO_ASTEROID));
	}
	for (int i = 0; i < 30; ++i)
	{
		m_goPowerUp.push_back(new GameObject(GameObject::GO_ASTEROID));
	}
	for (int i = 0; i < 10; ++i)
	{
		m_goCelestialBodies.push_back(new GameObject(GameObject::GO_ASTEROID));
	}
	
	/****************************************************************************************************************
	Initialization of player ship stats/world
	****************************************************************************************************************/
	m_lives = 3;
	m_score = 0;
    m_worldHeight = 100.0f;
	m_worldWidth = m_worldHeight * static_cast<float>(Application::GetWindowWidth() / Application::GetWindowHeight());
	m_index = 0;
	m_timer = 300.0f;

	m_ship = new GameObject(GameObject::GO_SHIP);
	m_ship->scale = { 5.0f,5.0f,5.0f };
	m_ship->pos = { m_worldWidth / 2.0f + 45.0f,m_worldHeight / 2.0f + 5.0f, 0 };
	m_ShipStartPos = m_ship->pos;
	m_ship->active = true;
	m_ship->mass = 1000000.0f;
	m_ship->bulletCooldown = 0.0f;
	m_ship->angle = 0.0f;
	m_ship->angularVelocity = 0.0f;
	m_ship->momentOfInertia = 1.0f;
	m_ship->health = 90000.0f;
	m_WeaponChoice = 0;
	m_WeaponUnlock1 = m_WeaponUnlock2 = m_WeaponUnlock3 = m_WeaponUnlock4 = false;
	BackgroundPos = ParallaxLayer1 = ParallaxLayer2 = ParallaxLayer3 = { m_worldWidth / 2.0f + 17.5f,m_worldHeight / 2.0f, 0 };

	BossSpawned = false;

	StartGame = false;
	EndGame = false;
}

/****************************************************************************************************************
Get OBJ
****************************************************************************************************************/
GameObject* SceneAsteroid::FetchGO(std::vector<GameObject*> m_goList)
{

	for (unsigned int it = 0; it < m_goList.size(); ++it)
	{
		if (m_goList[it]->active == false)
		{
			return m_goList[it];
		}
	}
	for (int counter = 0; counter < 100; ++counter)
	{
		m_goList.push_back(new GameObject(GameObject::GO_NONE));
	}
	return m_goList[m_goList.size() - 1];
}
/****************************************************************************************************************
Change Target
****************************************************************************************************************/
GameObject* SceneAsteroid::FetchNearestOBJ(const Vector3& position)
{
	GameObject* result = nullptr;
	float dist = -1.f;

	for (std::vector<GameObject*>::iterator it = m_goShip.begin(); it != m_goShip.end(); ++it)
	{
		GameObject* OBJ = (GameObject*)* it;
		if (OBJ->type == GameObject::GO_ENEMYSHIP || OBJ->type == GameObject::GO_BOSS)
		{
			if (!OBJ->active)
				continue;
			const float d = (OBJ->pos - position).LengthSquared();
			if (d < dist || dist == -1)
			{
				result = OBJ;
				dist = d;
			}
		}
	}
	for (std::vector<GameObject*>::iterator it = m_goAsteroid.begin(); it != m_goAsteroid.end(); ++it)
	{
		GameObject* OBJ = (GameObject*)* it;
		if (!OBJ->active)
			continue;
		const float d = (OBJ->pos - position).LengthSquared();
		if (d < dist || dist == -1)
		{
			result = OBJ;
			dist = d;
		}
	}
	for (std::vector<GameObject*>::iterator it = m_goBossShips.begin(); it != m_goBossShips.end(); ++it)
	{
		GameObject* OBJ = (GameObject*)* it;
		if (!OBJ->active)
			continue;
		const float d = (OBJ->pos - position).LengthSquared();
		if (d < dist || dist == -1)
		{
			result = OBJ;
			dist = d;
		}
	}
	return result;
}
/****************************************************************************************************************
Update
****************************************************************************************************************/
void SceneAsteroid::Update(double dt)
{
	/***********************************************************************************************************
	Title Screen
	***********************************************************************************************************/
	if (Application::IsKeyPressed(VK_RETURN) && !StartGame)
	{
		StartGame = true;
		m_ship->health = 100.0f;
		m_ship->mass = 5.0f;
		m_speed = 1.f;
		EnemySpawnRate = 1.f;
		AsteroidSpawnRate = 1.f;
		PowerUpSpawnRate = 1.f;
		CelestialBodySpawnRate = 1.f;
	}
	else if (!StartGame)
	{
		m_ship->vel = { 0, 0, 0 };
	}
	/************************************************************************************************************
	GamePlay
	*************************************************************************************************************/
	
		m_CameraLowestX = m_ship->pos.x - m_worldWidth / 2.0f;
		m_CameraLowestY = m_ship->pos.y - m_worldHeight / 2.0f;
		m_CameraHighestX = m_ship->pos.x + m_worldWidth / 2.0f;
		m_CameraHighestY = m_ship->pos.y + m_worldHeight / 2.0f;
		float RandX, RandY;
		m_ship->bulletCooldown--;

		BackgroundPos -= ((m_ship->vel * (m_speed * static_cast<float>(dt))) / 1.3);
		//ParallaxLayer1 -= ((m_ship->vel * (m_speed * static_cast<float>(dt))) / 1.2);
		ParallaxLayer2 -= ((m_ship->vel * (m_speed * static_cast<float>(dt))) / 1.2);
		ParallaxLayer3 -= ((m_ship->vel * (m_speed * static_cast<float>(dt))) / 1.1);

		SceneBase::Update(dt);

		/*if (Application::IsKeyPressed('9'))
		{
			m_speed = Math::Max(0.f, m_speed - 0.1f);
		}*/
		//if (Application::IsKeyPressed('0'))
		//{
		//	m_speed += 0.1f;
		//}
		m_force.SetZero();

		if (StartGame)
		{
			m_timer -= static_cast<float>(dt);
			/****************************************************************************************************************
			Movement
			****************************************************************************************************************/
			if (Application::IsKeyPressed('W'))
			{
				//m_force.Set(0, 100.0f, 0);
				m_force = m_ship->dir * 150;
			}
			if (Application::IsKeyPressed('A'))
			{
				//m_force.Set(-100.0f, 0, 0);
				m_force = m_ship->dir * 25;
				m_ship->m_torque = Vector3(1, -1, 0).Cross(Vector3(0.0f, 500.f, 0.0f));
			}
			if (Application::IsKeyPressed('S'))
			{
				//m_force.Set(0, -100.0f, 0);
				m_force = m_ship->dir * -150;
			}
			if (Application::IsKeyPressed('D'))
			{
				//m_force.Set(100.0f, 0, 0);
				m_force = m_ship->dir * 25;
				m_ship->m_torque = Vector3(-1, -1, 0).Cross(Vector3(0.0f, 500.f, 0.0f));
			}
			m_ship->angularVelocity *= (1 - 0.9 * dt);
			if (abs(m_ship->angularVelocity) <= Math::EPSILON)
			{
				m_ship->angularVelocity = 0.f;
			}
			if (Application::IsKeyPressed('1') && m_WeaponUnlock1)
			{
				m_WeaponChoice = 1;
			}
			if (Application::IsKeyPressed('2') && m_WeaponUnlock2)
			{
				m_WeaponChoice = 2;
			}
			if (Application::IsKeyPressed('3') && m_WeaponUnlock3)
			{
				m_WeaponChoice = 3;
			}
			if (Application::IsKeyPressed('4') && m_WeaponUnlock4)
			{
				m_WeaponChoice = 4;
			}
			if (Application::IsKeyPressed('0'))
			{
				m_WeaponUnlock1 = true;
				m_WeaponUnlock2 = true;
				m_WeaponUnlock3 = true;
				m_WeaponUnlock4 = true;
			}
		}
		/****************************************************************************************************************
		Alteration to main ship
		****************************************************************************************************************/
		//Empty

		/****************************************************************************************************************
		Spawning OBJ
		****************************************************************************************************************/
		AsteroidSpawnRate -= 0.01f / dt;
		EnemySpawnRate -= 0.0003f / dt;
		PowerUpSpawnRate -= 0.0001f / dt;
		CelestialBodySpawnRate -= 0.00006f / dt;

		if (AsteroidSpawnRate <= 0)
		{
			AsteroidSpawnRate = 1.0f;
			GameObject* tmp = FetchGO(m_goAsteroid);
			if (rand() % 2 == 1)
			{
				RandX = Math::RandFloatMinMax(m_CameraLowestX - tmp->scale.x, -m_CameraLowestX - tmp->scale.x) + 1.0f;
			}
			else
			{
				RandX = Math::RandFloatMinMax(m_CameraHighestX + tmp->scale.x, m_CameraHighestX * 2 + tmp->scale.x) + 1.0f;
			}
			if (rand() % 2 == 1)
			{
				RandY = Math::RandFloatMinMax(m_CameraLowestY - tmp->scale.y, -m_CameraLowestY - tmp->scale.y) + 1.0f;
			}
			else
			{
				RandY = Math::RandFloatMinMax(m_CameraHighestY + tmp->scale.y, m_CameraHighestY * 2 + tmp->scale.y) + 1.0f;
			}
			tmp->active = true;
			tmp->type = GameObject::GO_ASTEROID;
			tmp->pos = Vector3{ RandX, RandY, 0.0f };
			tmp->vel = Vector3{ Math::RandFloatMinMax(-10.0f, 10.0f), Math::RandFloatMinMax(-10.0f, 10.0f), 0.0f };
			float ScaleSet = Math::RandFloatMinMax(0.f, 10.0f) + 5.0f;
			tmp->scale.Set(ScaleSet, ScaleSet, 0.0f);
			tmp->OriginalScale = ScaleSet;
			tmp->health = 10.0f * ScaleSet;
			tmp->angularVelocity = Math::RandFloatMinMax(-90.0f, 90.0f) * static_cast<float>(dt);
		}
		if (EnemySpawnRate <= 0)
		{
			if (m_score <= 500)
			{
				EnemySpawnRate = 1.f;
				GameObject* enemy = FetchGO(m_goShip);
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
				enemy->active = true;
				enemy->type = GameObject::GO_ENEMYSHIP;
				enemy->pos = Vector3{ RandX, RandY,0.0f };
				enemy->health = 50.0f;
				enemy->angle = 0.0f;
				enemy->scale.Set(5.0f, 5.0f, 5.0f);
			}
			else if(m_score<=9000)
			{
				EnemySpawnRate = 99999.f;
				for (std::vector<GameObject*>::iterator it = m_goBossShips.begin(); it != m_goBossShips.end(); ++it)
				{
					GameObject* OBJ = (GameObject*)* it;
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
							enemy->type = GameObject::GO_BOSS;
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
							enemy->type = GameObject::GO_ENEMYSHIP;
							enemy->pos = Vector3{ RandX, RandY,0.0f };
							enemy->health = 0.0f;
							enemy->angle = 0.0f;
							enemy->scale.Set(7.5f, 7.5f, 0.0f);
							enemy->target = (GameObject*) * --it;
							enemy->mass = 7.5;
							it++;
						}
					}
				}
			}
		}
		if (PowerUpSpawnRate <= 0)
		{
			PowerUpSpawnRate = 1.f;
			GameObject* PowerUp = FetchGO(m_goPowerUp);
			if (rand() % 2 == 1)
			{
				RandX = Math::RandFloatMinMax(m_CameraLowestX - PowerUp->scale.x, (-m_CameraLowestX * 2) - PowerUp->scale.x);
			}
			else
			{
				RandX = Math::RandFloatMinMax(m_CameraHighestX + PowerUp->scale.x, m_CameraHighestX * 3 + PowerUp->scale.x);
			}
			if (rand() % 2 == 1)
			{
				RandY = Math::RandFloatMinMax(m_CameraLowestY - PowerUp->scale.y, (-m_CameraLowestY * 2) - PowerUp->scale.y);
			}
			else
			{
				RandY = Math::RandFloatMinMax(m_CameraHighestY + PowerUp->scale.y, m_CameraHighestY * 3 + PowerUp->scale.y);
			}
			PowerUp->active = true;
			PowerUp->type = GameObject::GO_POWERUP;
			PowerUp->pos = Vector3{ RandX, RandY,0.0f };
			PowerUp->angle = 0.0f;
			PowerUp->scale.Set(10.0f, 10.0f, 0.0f);
			PowerUp->PowerUpNum = Math::RandIntMinMax(1, 4);
			PowerUp->angularVelocity = 90.0f * static_cast<float>(dt);
		}
		if (CelestialBodySpawnRate <= 0)
		{
			CelestialBodySpawnRate = 1.f;
			GameObject* CB = FetchGO(m_goCelestialBodies);
			if (rand() % 2 == 1)
			{
				RandX = Math::RandFloatMinMax(m_CameraLowestX * 1.5 - CB->scale.x, (-m_CameraLowestX * 2) - CB->scale.x);
			}
			else
			{
				RandX = Math::RandFloatMinMax(m_CameraHighestX * 1.5 + CB->scale.x, m_CameraHighestX * 3 + CB->scale.x);
			}
			if (rand() % 2 == 1)
			{
				RandY = Math::RandFloatMinMax(m_CameraLowestY * 1.5 - CB->scale.y, (-m_CameraLowestY * 2) - CB->scale.y);
			}
			else
			{
				RandY = Math::RandFloatMinMax(m_CameraHighestY * 1.5 + CB->scale.y, m_CameraHighestY * 3 + CB->scale.y);
			}
			CB->active = true;
			if (rand() % 10 == 9)
			{
				CB->type = GameObject::GO_BLACKHOLE;
				CB->pos = Vector3{ RandX, RandY,0.0f };
				float scale = Math::RandFloatMinMax(10.0f, 80.0f);
				CB->mass = 30 * scale;
				CB->scale.Set(scale * 2.5, scale, 0.0f);
				CB->vel = { 0,0,0 };
			}
			else
			{
				int randnum = rand() % 4;
				if (randnum == 1)
					CB->type = GameObject::GO_PLANET1;
				else if (randnum == 2)
					CB->type = GameObject::GO_PLANET2;
				else if (randnum == 3)
					CB->type = GameObject::GO_PLANET3;
				else
					CB->type = GameObject::GO_PLANET4;
				CB->pos = Vector3{ RandX, RandY,0.0f };
				float scale = Math::RandFloatMinMax(30.0f, 60.0f);
				CB->scale.Set(scale, scale, 0.0f);
				CB->mass = 10 * scale;
				CB->health = 20 * scale;
				CB->vel = { 0,0,0 };
			}
		}
		if (StartGame)
		{
			/****************************************************************************************************************
			Mouse Section(Shooting)
			****************************************************************************************************************/
			m_ship->bulletCooldown -= 2.f * dt;
			static bool bLButtonState = false;
			if (Application::IsMousePressed(0) && m_ship->bulletCooldown <= 0)
			{
				if (m_WeaponChoice == 0) // Normal Pellets
				{
						GameObject* object = FetchGO(m_goBullet);
						object->active = true;
						object->type = GameObject::GO_BULLET;
						object->scale.Set(0.5f, 0.5f, 0.5f);
						object->pos = m_ship->pos;
						object->vel = m_ship->dir * BULLET_SPEED + m_ship->vel;
						m_ship->bulletCooldown = 10.0f/* BULLET_RATE*/;
						m_ship->vel -= object->vel / 1000;
				}
				else if (m_WeaponChoice == 1) // Machine Gun
				{
					for (int i = 0; i < 2; i++)
					{
						GameObject* object = FetchGO(m_goBullet);
						object->active = true;
						object->type = GameObject::GO_BULLET;
						object->scale.Set(0.5f, 0.5f, 0.5f);
						object->pos = m_ship->pos;
						object->vel = m_ship->dir * BULLET_SPEED + m_ship->vel + Math::RandFloatMinMax(-45.0f, 45.0f);
						m_ship->bulletCooldown = 0.001f/* BULLET_RATE*/;
						m_ship->vel -= object->vel / 1000;
					}
				}
				else if (m_WeaponChoice == 2)//ShotGun
				{
					for (int i = 0; i < 50; i++)
					{
						GameObject* object = FetchGO(m_goBullet);
						object->active = true;
						object->type = GameObject::GO_BULLET;
						object->scale.Set(0.5f, 0.5f, 0.5f);
						object->pos = m_ship->pos;
						float sprayangle = (i - (50 / 2)) * 1;
						sprayangle = Math::DegreeToRadian(sprayangle);
						object->vel.Set((BULLET_SPEED * cos(m_ship->angle + sprayangle)) + m_ship->vel.x, (BULLET_SPEED * sin(m_ship->angle + sprayangle)) + m_ship->vel.y, 0);
						m_ship->bulletCooldown = 50.0f/* BULLET_RATE*/;
						m_ship->vel -= object->vel / 400;
					}
				}
				else if (m_WeaponChoice == 3)//Homing Rockets
				{
						GameObject* object = FetchGO(m_goBullet);
						object->active = true;
						object->type = GameObject::GO_MISSILE;
						object->scale.Set(1.0f, 3.0f, 0.0f);
						object->pos = m_ship->pos;
						object->vel = -m_ship->dir * MISSILE_SPEED + m_ship->vel - Math::RandFloatMinMax(-45.0f, 45.0f);
						object->target = FetchNearestOBJ(object->pos);
						object->health = 10.0f; //For time
						m_ship->bulletCooldown = 3.0f/* BULLET_RATE*/;
				}
				else if (m_WeaponChoice == 4)//Pulse Gun
				{
						for (int i = 0; i < 100; i++)
						{
							GameObject* object = FetchGO(m_goBullet);
							object->active = true;
							object->type = GameObject::GO_PULSEBULLET;
							object->scale.Set(2.5f, 2.5f, 2.5f);
							object->pos = m_ship->pos;
							float sprayangle = (i - (100 / 2)) * 1;
							sprayangle = Math::DegreeToRadian(sprayangle);
							object->vel.Set((50 * cos(m_ship->angle + sprayangle)) + m_ship->vel.x, (50 * sin(m_ship->angle + sprayangle)) + m_ship->vel.y, 0);
							object->angle = m_ship->angle + sprayangle;
							m_ship->bulletCooldown = 100.0f/* BULLET_RATE*/;
							m_ship->vel -= object->vel / 150;
						}
				}
			}

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

			/****************************************************************************************************************
			Game Physics
			****************************************************************************************************************/
			m_ship->momentOfInertia = m_ship->mass * m_ship->scale.x * m_ship->scale.x;
			m_ship->angularVelocity += (m_ship->m_torque.z / m_ship->momentOfInertia) * static_cast<float>(dt);
			m_ship->angle += static_cast<float>(m_ship->angularVelocity * dt);
			m_ship->dir = Vector3(cosf(m_ship->angle), sinf(m_ship->angle), 0.0f);
			m_ship->m_torque.SetZero();

			Vector3 acceleration = m_force * (1 / m_ship->mass);
			m_ship->vel += acceleration * (m_speed * static_cast<float>(dt));
			//m_ship->pos += m_ship->vel*(m_speed * static_cast<float>(dt));
		}
		/***********************************************************************************************************************************
		Asteroids AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goAsteroid.begin(); it != m_goAsteroid.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)* it;
			if (OBJ->active)
			{
				OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
				OBJ->angle += static_cast<float>(OBJ->angularVelocity * dt);
				if (sqrt(pow(OBJ->pos.x - m_ship->pos.x, 2) + pow(OBJ->pos.y - m_ship->pos.y, 2)) > 200.0f)
				{
					OBJ->active = false;
				}

				//Handle collision between GO_SHIP and GO_ASTEROID
				float combinedRadii = OBJ->scale.x + m_ship->scale.x;
				if ((OBJ->pos - m_ship->pos).LengthSquared() < combinedRadii)
				{
					float scaleReduc = (30.0f / OBJ->health) * 100;
					OBJ->health -= 30.0f;
					OBJ->scale.x = (OBJ->scale.x / 100) * (100 - scaleReduc);
					OBJ->scale.y = (OBJ->scale.y / 100) * (100 - scaleReduc);
					if (OBJ->health <= 0.0f)
						OBJ->active = false;
					m_ship->health -= 5.0f * (OBJ->scale.x/ OBJ->OriginalScale);
				}
				for (std::vector<GameObject*>::iterator it2 = m_goShip.begin(); it2 != m_goShip.end(); ++it2)
				{
					GameObject* OBJ2 = (GameObject*)* it2;
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
							if (!BossSpawned && OBJ2->type == GameObject::GO_ENEMYSHIP)
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
			GameObject* OBJ = (GameObject*)* it;
			if (OBJ->active)
			{
				OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
				if (sqrt(pow(OBJ->pos.x - m_ship->pos.x, 2) + pow(OBJ->pos.y - m_ship->pos.y, 2)) > 200.0f)
				{
					OBJ->active = false;
				}

				float combinedRadii = OBJ->scale.x + m_ship->scale.x + 10.0f;
				if ((OBJ->pos - m_ship->pos).LengthSquared() > combinedRadii)
					m_ship->vel += (OBJ->pos - m_ship->pos).Normalized() * (OBJ->mass / (OBJ->pos - m_ship->pos).LengthSquared());

				if (OBJ->type == GameObject::GO_PLANET1
					|| OBJ->type == GameObject::GO_PLANET2
					|| OBJ->type == GameObject::GO_PLANET3
					|| OBJ->type == GameObject::GO_PLANET4)
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

				if (OBJ->type == GameObject::GO_BLACKHOLE)
				{
					if ((OBJ->pos - m_ship->pos).LengthSquared() < combinedRadii)
						m_ship->health -= 10.0f * dt;
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
						if (OBJ2->type == GameObject::GO_PLANET1
							|| OBJ2->type == GameObject::GO_PLANET2
							|| OBJ2->type == GameObject::GO_PLANET3
							|| OBJ2->type == GameObject::GO_PLANET4)
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
		PowerUps AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goPowerUp.begin(); it != m_goPowerUp.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)* it;
			if (OBJ->active)
			{
				OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
				OBJ->angle += static_cast<float>(OBJ->angularVelocity * dt);
				if (sqrt(pow(OBJ->pos.x - m_ship->pos.x, 2) + pow(OBJ->pos.y - m_ship->pos.y, 2)) > 200.0f)
				{
					OBJ->active = false;
				}
				if (sqrt(pow(OBJ->pos.x - m_ship->pos.x, 2) + pow(OBJ->pos.y - m_ship->pos.y, 2)) < 40.0f)
				{
					float dist = sqrt(pow(OBJ->pos.x - m_ship->pos.x, 2) + pow(OBJ->pos.y - m_ship->pos.y, 2)) / 2;
					OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / dist, (m_ship->pos.y - OBJ->pos.y) / dist, 0.0f };
				}
				else
				{
					OBJ->vel += (OBJ->pos - m_ship->pos).Normalized() * (OBJ->mass / (OBJ->pos - m_ship->pos).LengthSquared());
					if (OBJ->vel.x <= 0.1f || OBJ->vel.x >= -0.1f && OBJ->vel.y <= 0.1f || OBJ->vel.y >= -0.1f)
					{
						OBJ->vel.SetZero();
					}
				}
				//Handle collision between m_ship and GO_POWERUP using simple distance-based check
				float combinedRadii = OBJ->scale.x + m_ship->scale.x;
				if ((OBJ->pos - m_ship->pos).LengthSquared() < combinedRadii)
				{
					OBJ->active = false;
					if (OBJ->PowerUpNum == 1)
					{
						if (m_WeaponUnlock1)
						{
							GameObject* Ally = FetchGO(m_goShip);
							Ally->active = true;
							Ally->type = GameObject::GO_GUARDIAN;
							Ally->scale.Set(3.0f, 3.0f, 3.0f);
							Ally->pos = OBJ->pos;
							Ally->vel = m_ship->vel;
							Ally->target = FetchNearestOBJ(Ally->pos);
							Ally->health = 900.0f;
							Ally->mass = 5.0f;
						}
						m_WeaponUnlock1 = true;
						m_ship->health = 100;
						
					}
					else if (OBJ->PowerUpNum == 2)
					{
						if (m_WeaponUnlock2)
						{
							GameObject* Ally = FetchGO(m_goShip);
							Ally->active = true;
							Ally->type = GameObject::GO_GUARDIAN;
							Ally->scale.Set(3.0f, 3.0f, 3.0f);
							Ally->pos = OBJ->pos;
							Ally->vel = m_ship->vel;
							Ally->target = FetchNearestOBJ(Ally->pos);
							Ally->health = 900.0f;
							Ally->mass = 5.0f;
						}
						m_WeaponUnlock2 = true;
						m_ship->health = 100;
						
					}
					else if (OBJ->PowerUpNum == 3)
					{
						if (m_WeaponUnlock3)
						{
							GameObject* Ally = FetchGO(m_goShip);
							Ally->active = true;
							Ally->type = GameObject::GO_GUARDIAN;
							Ally->scale.Set(3.0f, 3.0f, 3.0f);
							Ally->pos = OBJ->pos;
							Ally->vel = m_ship->vel;
							Ally->target = FetchNearestOBJ(Ally->pos);
							Ally->health = 900.0f;
							Ally->mass = 5.0f;
						}
						m_WeaponUnlock3 = true;
						m_ship->health = 100;
						
					}
					else if (OBJ->PowerUpNum == 4)
					{
						if (m_WeaponUnlock4)
						{
							GameObject* Ally = FetchGO(m_goShip);
							Ally->active = true;
							Ally->type = GameObject::GO_GUARDIAN;
							Ally->scale.Set(3.0f, 3.0f, 3.0f);
							Ally->pos = OBJ->pos;
							Ally->vel = m_ship->vel;
							Ally->target = FetchNearestOBJ(Ally->pos);
							Ally->health = 900.0f;
							Ally->mass = 5.0f;
						}
						m_WeaponUnlock4 = true;
						m_ship->health = 100;
						
					}
				}
			}
		}

		/***********************************************************************************************************************************
		Enemies AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goShip.begin(); it != m_goShip.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)* it;
			if (OBJ->active)
			{
				if (OBJ->type == GameObject::GO_ENEMYSHIP)
				{
					OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
					//Enemy AI
					OBJ->bulletCooldown -= 1.f * dt;
					OBJ->dir = (m_ship->pos - OBJ->pos).Normalized();
					OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / 200, (m_ship->pos.y - OBJ->pos.y) / 200, 0.0f };
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
						EnemyBullet->type = GameObject::GO_ENEMYSHIP_BULLET;
						EnemyBullet->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet->pos = OBJ->pos;
						EnemyBullet->vel = OBJ->dir * BULLET_SPEED + OBJ->vel;
						OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;
					}
				}
				else if (OBJ->type == GameObject::GO_GUARDIAN)
				{
					if (OBJ->target != NULL)
					{
						OBJ->dir = (OBJ->target->pos - OBJ->pos).Normalized();
						OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / 30, (m_ship->pos.y - OBJ->pos.y) / 30, 0.0f };
					}
					else if (OBJ->target == NULL)
					{
						OBJ->target = FetchNearestOBJ(OBJ->pos);
					}
					OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
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
							AllyBullet->type = GameObject::GO_BULLET;
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
		/***********************************************************************************************************************************
		BOSS AI
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goBossShips.begin(); it != m_goBossShips.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)* it;
			if (OBJ->active)
			{
				static bool PhaseShift = false;
				if (OBJ->type == GameObject::GO_BOSS)
				{
					if (!PhaseShift)
					{
						//Enemy AI
						OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / 50, (m_ship->pos.y - OBJ->pos.y) / 50, 0.0f };
						Vector3 OldPos = OBJ->pos;
						OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
						OBJ->dir = ((OBJ->pos - OldPos).Normalized());
						OBJ->angle = atan2(OBJ->dir.y, OBJ->dir.x);
					}
					else if (PhaseShift)
					{
						//Enemy AI
						OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / 80, (m_ship->pos.y - OBJ->pos.y) / 80, 0.0f };
						OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
						OBJ->bulletCooldown -= 1.f * dt;
						OBJ->dir = (m_ship->pos - OBJ->pos).Normalized();
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
							object->type = GameObject::GO_ENEMYSHIP_BULLET;
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
						OBJ->vel += Vector3{ (m_ship->pos.x - OBJ->pos.x) / 30, (m_ship->pos.y - OBJ->pos.y) / 30, 0.0f };
						OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
						OBJ->bulletCooldown -= 1.f * dt;
						OBJ->dir = (m_ship->pos - OBJ->pos).Normalized();
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
						OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
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
						EnemyBullet1->type = GameObject::GO_ENEMYSHIP_BULLET;
						EnemyBullet1->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet1->pos = OBJ->pos;
						EnemyBullet1->vel = OBJ->dir * 1 / 2 * BULLET_SPEED + OBJ->vel;
						OBJ->bulletCooldown = 1.0f/* BULLET_RATE*/;

						GameObject* EnemyBullet2 = FetchGO(m_goBullet);
						EnemyBullet2->active = true;
						EnemyBullet2->type = GameObject::GO_ENEMYSHIP_BULLET;
						EnemyBullet2->scale.Set(0.5f, 0.5f, 0.5f);
						EnemyBullet2->pos = OBJ->pos;
						EnemyBullet2->vel = OBJ->dir * -1 / 2 * BULLET_SPEED + OBJ->vel;
					}

				}
			}
		}
		/***********************************************************************************************************************************
		Bullet Collision
		***********************************************************************************************************************************/
		for (std::vector<GameObject*>::iterator it = m_goBullet.begin(); it != m_goBullet.end(); ++it)
		{
			GameObject* OBJ = (GameObject*)* it;
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
						if (OBJ->type == GameObject::GO_BULLET)
						{
							m_score += 2;
						}
					}
				}

				//Handle collision between GO_BULLET and m_ship using simple distance-based check
				if (OBJ->type == GameObject::GO_ENEMYSHIP_BULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
					float combinedRadii = OBJ->scale.x + m_ship->scale.x;
					if ((OBJ->pos - m_ship->pos).LengthSquared() < combinedRadii)
					{
						OBJ->active = false;
						m_ship->health -= 5.0f;
					}
				}
				//Handle collision between GO_BULLET and enemy ships using simple distance-based check
				else if (OBJ->type == GameObject::GO_BULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GameObject::GO_GUARDIAN)
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
				else if (OBJ->type == GameObject::GO_PULSEBULLET)
				{
					OBJ->pos += OBJ->vel * m_speed * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GameObject::GO_GUARDIAN)
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
						if (OBJ2->type == GameObject::GO_ENEMYSHIP_BULLET)
						{
							if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active)
							{
								OBJ2->vel = OBJ->vel * 2;
							}
						}
					}
				}
				else if (OBJ->type == GameObject::GO_MISSILE)
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
					OBJ->pos += OBJ->vel * static_cast<float>(dt) - m_ship->vel * (m_speed * static_cast<float>(dt));
					if (OBJ->health <= 0.0f)
					{
						OBJ = false;
					}
					for (int i = 0; i < m_goShip.size(); ++i)
					{
						GameObject* OBJ2 = m_goShip[i];
						if ((OBJ->pos - OBJ2->pos).LengthSquared() < (OBJ->scale.x + OBJ2->scale.x) && OBJ2->active && OBJ2->type != GameObject::GO_GUARDIAN)
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

		/******************************************************************************************************
		Lives
		******************************************************************************************************/
		if (m_ship->health <= 0.0f || m_timer <= 0.0f)
		{
			--m_lives;

			if(m_lives == 0)
			EndGame = true;
			
			m_ship->health = 100.0f;

			//Spawn Bullets in all directions
		}
		
		ParallaxLayer3.x = Math::Wrap(ParallaxLayer3.x, static_cast<float>(-m_worldWidth * 1.25), static_cast<float>(m_worldWidth * 1.25));
		ParallaxLayer3.y = Math::Wrap(ParallaxLayer3.y, static_cast<float>(-m_worldHeight * 1.25), static_cast<float>(m_worldHeight * 1.25));
		ParallaxLayer2.x = Math::Wrap(ParallaxLayer2.x, static_cast<float>(-m_worldWidth * 1.25), static_cast<float>(m_worldWidth * 1.25));
		ParallaxLayer2.y = Math::Wrap(ParallaxLayer2.y, static_cast<float>(-m_worldHeight * 1.25), static_cast<float>(m_worldHeight * 1.25));
		BackgroundPos.x = Math::Wrap(BackgroundPos.x , static_cast<float>(-m_worldWidth * 5), static_cast<float>(m_worldWidth * 5));
		BackgroundPos.y = Math::Wrap(BackgroundPos.y, static_cast<float>(-m_worldHeight * 5), static_cast<float>(m_worldHeight * 5));
		
	/***********************************************************************************************************************
	EndGame (Love you 3000)
	************************************************************************************************************************/
	//Empty
}

/****************************************************************************************************************
Rendering
****************************************************************************************************************/
void SceneAsteroid::RenderGO(GameObject *go)
{
	switch(go->type)
	{
	case GameObject::GO_SHIP:
		modelStack.PushMatrix();
		modelStack.Translate(m_ship->pos.x, m_ship->pos.y, m_ship->pos.z);
		modelStack.Scale(m_ship->scale.x, m_ship->scale.y, m_ship->scale.z);
		modelStack.Rotate(Math::RadianToDegree(m_ship->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_SHIP], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_GUARDIAN:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(atan2(go->dir.y, go->dir.x)) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_GUARDIAN], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_ASTEROID:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_Asteroid], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_BULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_BALL], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_MISSILE:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Rotate(Math::RadianToDegree(atan2(go->dir.y, go->dir.x)) - 90.0f, 0, 0, 1);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_MISSILE], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_PULSEBULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_PULSEBULLET], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_ENEMYSHIP:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_ENEMY], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_BOSS:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_BOSS], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_ENEMYSHIP_BULLET:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_ENEMYBALL], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_POWERUP:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		modelStack.Rotate(Math::RadianToDegree(go->angle) - 90.0f, 0, 0, 1);
		RenderMesh(meshList[GEO_POWERUP], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_PLANET1:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_PLANET1], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_PLANET2:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_PLANET2], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_PLANET3:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_PLANET3], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_PLANET4:
		modelStack.PushMatrix();
		modelStack.Translate(go->pos.x, go->pos.y, go->pos.z);
		modelStack.Scale(go->scale.x, go->scale.y, go->scale.z);
		RenderMesh(meshList[GEO_PLANET4], false);
		modelStack.PopMatrix();
		break;
	case GameObject::GO_BLACKHOLE:
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
	
	//Calculating aspect ratio
	m_worldHeight = 100.f;
	m_worldWidth = m_worldHeight * (float)Application::GetWindowWidth() / Application::GetWindowHeight();

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

	/*****************************************************************************************************
	BackGround
	*****************************************************************************************************/
	
	if (WorldBackGroundNumber == 1)
	{
		modelStack.PushMatrix();
		modelStack.Translate(BackgroundPos.x, BackgroundPos.y, BackgroundPos.z);
		modelStack.Scale(m_worldWidth * 2, m_worldHeight * 2, 0.f);
		for (int x = 0; x < 10; x++)
		{
			for (int y = 0; y < 10; y++)
			{
				modelStack.PushMatrix();
				modelStack.Translate(x - 5.0f, y - 5.0f, 0.0f);
				RenderMesh(meshList[GEO_BACKGROUND1], false);
				modelStack.PopMatrix();
			}
		}
		modelStack.PopMatrix();
	}
	else if (WorldBackGroundNumber == 2)
	{
		modelStack.PushMatrix();
		modelStack.Translate(BackgroundPos.x, BackgroundPos.y, BackgroundPos.z);
		modelStack.Scale(m_worldWidth * 2, m_worldHeight * 2, 0.f);
		for (int x = 0; x < 10; x++)
		{
			for (int y = 0; y < 10; y++)
			{
				modelStack.PushMatrix();
				modelStack.Translate(x - 5.0f, y - 5.0f, 0.0f);
				RenderMesh(meshList[GEO_BACKGROUND2], false);
				modelStack.PopMatrix();
			}
		}
		modelStack.PopMatrix();
	}
	else
	{
		modelStack.PushMatrix();
		modelStack.Translate(BackgroundPos.x, BackgroundPos.y, BackgroundPos.z);
		modelStack.Scale(m_worldWidth * 2, m_worldHeight * 2, 0.f);
		for (int x = 0; x < 10; x++)
		{
			for (int y = 0; y < 10; y++)
			{
				modelStack.PushMatrix();
				modelStack.Translate(x - 5.0f, y - 5.0f, 0.0f);
				RenderMesh(meshList[GEO_BACKGROUND3], false);
				modelStack.PopMatrix();
			}
		}
		modelStack.PopMatrix();
	}

	modelStack.PushMatrix();
	modelStack.Translate(ParallaxLayer2.x, ParallaxLayer2.y, ParallaxLayer2.z);
	modelStack.Scale(m_worldWidth/2, m_worldHeight/2, 0.f);
	for (int x = 0; x < 10; x++)
	{
		for (int y = 0; y < 10; y++)
		{
			modelStack.PushMatrix();
			modelStack.Translate(x - 5.0f, y - 5.0f, 0.0f);
			RenderMesh(meshList[GEO_PARALLAXLAYER2], false);
			modelStack.PopMatrix();
		}
	}
	modelStack.PopMatrix();

	modelStack.PushMatrix();
	modelStack.Translate(ParallaxLayer3.x, ParallaxLayer3.y, ParallaxLayer3.z);
	modelStack.Scale(m_worldWidth/2, m_worldHeight/2, 0.f);
	for (int x = 0; x < 10; x++)
	{
		for (int y = 0; y < 10; y++)
		{
			modelStack.PushMatrix();
			modelStack.Translate(x - 5.0f, y - 5.0f, 0.0f);
			RenderMesh(meshList[GEO_PARALLAXLAYER3], false);
			modelStack.PopMatrix();
		}
	}
	modelStack.PopMatrix();


	//Render list
	for (std::vector<GameObject*>::iterator it = m_goCelestialBodies.begin(); it != m_goCelestialBodies.end(); ++it)
	{
		GameObject* go = (GameObject*)* it;
		if (go->active)
		{
			RenderGO(go);
		}
	}
	for(std::vector<GameObject *>::iterator it = m_goAsteroid.begin(); it != m_goAsteroid.end(); ++it)
	{
		GameObject *go = (GameObject *)*it;
		if(go->active)
		{
			RenderGO(go);
		}
	}
	for(std::vector<GameObject *>::iterator it = m_goBullet.begin(); it != m_goBullet.end(); ++it)
	{
		GameObject *go = (GameObject *)*it;
		if(go->active)
		{
			RenderGO(go);
		}
	}
	for (std::vector<GameObject*>::iterator it = m_goShip.begin(); it != m_goShip.end(); ++it)
	{
		GameObject* go = (GameObject*)* it;
		if (go->active)
		{
			RenderGO(go);
		}
	}
	int NumberofShips = 0;
	if (BossSpawned)
	{
		for (std::vector<GameObject*>::iterator it = m_goBossShips.end() - 1; it != m_goBossShips.begin(); --it)
		{
			GameObject* go = (GameObject*)* it;
			if (go->active)
			{
				RenderGO(go);
				NumberofShips++;
			}
		}
		//Render Boss head;
		std::vector<GameObject*>::iterator it = m_goBossShips.begin();
		GameObject* go = (GameObject*)* it;
		if (go->active)
		{
			RenderGO(go);
		}
	}
	for (std::vector<GameObject*>::iterator it = m_goPowerUp.begin(); it != m_goPowerUp.end(); ++it)
	{
		GameObject* go = (GameObject*)* it;
		if (go->active)
		{
			RenderGO(go);
		}
	}
	//On screen text
	if (StartGame && !EndGame)
	{
		RenderGO(m_ship);
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
				modelStack.Translate(-0.085f / 100 * (100 - m_ship->health), 0.0f, 0.0f);
				modelStack.Scale(0.17f / 100 * m_ship->health, 0.05f, 0.0f);
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
				modelStack.Translate(-0.085f / 100 * (100 - m_ship->health), 0.0f, 0.0f);
				modelStack.Scale(0.17f / 100 * m_ship->health, 0.05f, 0.0f);
			}
			else
				modelStack.Scale(0.17f, 0.05f, 0.0f);
			RenderMesh(meshList[GEO_HEALTHYELLOW], false);
			modelStack.PopMatrix();
		}
		if (m_lives == 3)
		{
			modelStack.PushMatrix();
			modelStack.Translate(-0.085f / 100 * (100 - m_ship->health), 0.0f, 0.0f);
			modelStack.Scale(0.17f / 100 * m_ship->health, 0.05f, 0.0f);
			RenderMesh(meshList[GEO_HEALTHGREEN], false);
			modelStack.PopMatrix();
		}
		modelStack.PopMatrix();
		//WeaponLocks
		if (!m_WeaponUnlock1)
		{
			modelStack.PushMatrix();
			modelStack.Translate(-0.108f, -0.438f, 0.0f);
			modelStack.Scale(0.03f, 0.07f, 0.0f);
			RenderMesh(meshList[GEO_LOCKED], false);
			modelStack.PopMatrix();
		}
		if (!m_WeaponUnlock2)
		{
			modelStack.PushMatrix();
			modelStack.Translate(0.025f, -0.438f, 0.0f);
			modelStack.Scale(0.03f, 0.07f, 0.0f);
			RenderMesh(meshList[GEO_LOCKED], false);
			modelStack.PopMatrix();
		}
		if (!m_WeaponUnlock3)
		{
			modelStack.PushMatrix();
			modelStack.Translate(0.157f, -0.438f, 0.0f);
			modelStack.Scale(0.03f, 0.07f, 0.0f);
			RenderMesh(meshList[GEO_LOCKED], false);
			modelStack.PopMatrix();
		}
		if (!m_WeaponUnlock4)
		{
			modelStack.PushMatrix();
			modelStack.Translate(0.285f, -0.438f, 0.0f);
			modelStack.Scale(0.03f, 0.07f, 0.0f);
			RenderMesh(meshList[GEO_LOCKED], false);
			modelStack.PopMatrix();
		}
		modelStack.PopMatrix();

		std::ostringstream ss;
		//ss.str("");
		ss.precision(1);
		//ss << "Lives: " << m_ship->health;//m_lives
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

	
	
	////Exercise 5b: Render position, velocity & mass of ship
	//ss.str("");
	//ss << "Mass: " << m_ship->mass;
	//RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0, 1, 0), 3, 0, 15);

	//ss.str("");
	//ss << "Velocity: " << m_ship->vel;
	//RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0, 1, 0), 3, 0, 12);

	//ss.str("");
	//ss << "Position: " << m_ship->pos;
	//RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0, 1, 0), 3, 0, 9);

	//ss.str("");
	//ss << "Speed: " << m_speed;
	//RenderTextOnScreen(meshList[GEO_TEXT], ss.str(), Color(0, 1, 0), 3, 0, 6);

	if (!StartGame)
	{
		modelStack.PushMatrix();
		modelStack.Translate(m_worldWidth / 2, m_worldHeight / 2, 0.0f);
		modelStack.Scale(m_worldWidth, m_worldHeight, 0.0f);
		RenderMesh(meshList[GEO_MENU], false);
		modelStack.PopMatrix();
	}
	if (EndGame)
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

/****************************************************************************************************************
Exit
****************************************************************************************************************/
void SceneAsteroid::Exit()
{
	SceneBase::Exit();
	//Cleanup GameObjects
	while (m_goAsteroid.size() > 0)
	{
		GameObject* go = m_goAsteroid.back();
		delete go;
		m_goAsteroid.pop_back();
	}
	while (m_goBullet.size() > 0)
	{
		GameObject* go = m_goBullet.back();
		delete go;
		m_goBullet.pop_back();
	}
	while (m_goShip.size() > 0)
	{
		GameObject* go = m_goShip.back();
		delete go;
		m_goShip.pop_back();
	}
	while (m_goPowerUp.size() > 0)
	{
		GameObject* go = m_goPowerUp.back();
		delete go;
		m_goPowerUp.pop_back();
	}
	while(m_goCelestialBodies.size() > 0)
	{
		GameObject *go = m_goCelestialBodies.back();
		delete go;
		m_goCelestialBodies.pop_back();
	}
	while (m_goBossShips.size() > 0)
	{
		GameObject* go = m_goBossShips.back();
		delete go;
		m_goBossShips.pop_back();
	}
	if(m_ship)
	{
		delete m_ship;
		m_ship = NULL;
	}
}
