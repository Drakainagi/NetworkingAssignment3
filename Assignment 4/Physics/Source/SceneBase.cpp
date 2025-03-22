#include "SceneBase.h"
#include "GL/glew.h"
#include "shader.hpp"
#include "MeshBuilder.h"
#include "Application.h"
#include "Utility.h"
#include "LoadTGA.h"
#include <sstream>

/**
 * @brief Constructor for SceneBase.
 */
SceneBase::SceneBase() {}

/**
 * @brief Destructor for SceneBase.
 */
SceneBase::~SceneBase() {}

/**
 * @brief Initializes the scene, loads shaders, sets up lighting, camera, and meshes.
 */
void SceneBase::Init()
{
    // Set black background and enable OpenGL states
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenVertexArrays(1, &m_vertexArrayID);
    glBindVertexArray(m_vertexArrayID);

    // Load shader and retrieve uniform locations
    m_programID = LoadShaders("Shader//comg.vertexshader", "Shader//comg.fragmentshader");
    m_parameters[U_MVP] = glGetUniformLocation(m_programID, "MVP");
    m_parameters[U_MODELVIEW] = glGetUniformLocation(m_programID, "MV");
    m_parameters[U_MODELVIEW_INVERSE_TRANSPOSE] = glGetUniformLocation(m_programID, "MV_inverse_transpose");
    m_parameters[U_MATERIAL_AMBIENT] = glGetUniformLocation(m_programID, "material.kAmbient");
    m_parameters[U_MATERIAL_DIFFUSE] = glGetUniformLocation(m_programID, "material.kDiffuse");
    m_parameters[U_MATERIAL_SPECULAR] = glGetUniformLocation(m_programID, "material.kSpecular");
    m_parameters[U_MATERIAL_SHININESS] = glGetUniformLocation(m_programID, "material.kShininess");
    m_parameters[U_LIGHTENABLED] = glGetUniformLocation(m_programID, "lightEnabled");
    m_parameters[U_NUMLIGHTS] = glGetUniformLocation(m_programID, "numLights");
    m_parameters[U_LIGHT0_TYPE] = glGetUniformLocation(m_programID, "lights[0].type");
    m_parameters[U_LIGHT0_POSITION] = glGetUniformLocation(m_programID, "lights[0].position_cameraspace");
    m_parameters[U_LIGHT0_COLOR] = glGetUniformLocation(m_programID, "lights[0].color");
    m_parameters[U_LIGHT0_POWER] = glGetUniformLocation(m_programID, "lights[0].power");
    m_parameters[U_LIGHT0_KC] = glGetUniformLocation(m_programID, "lights[0].kC");
    m_parameters[U_LIGHT0_KL] = glGetUniformLocation(m_programID, "lights[0].kL");
    m_parameters[U_LIGHT0_KQ] = glGetUniformLocation(m_programID, "lights[0].kQ");
    m_parameters[U_LIGHT0_SPOTDIRECTION] = glGetUniformLocation(m_programID, "lights[0].spotDirection");
    m_parameters[U_LIGHT0_COSCUTOFF] = glGetUniformLocation(m_programID, "lights[0].cosCutoff");
    m_parameters[U_LIGHT0_COSINNER] = glGetUniformLocation(m_programID, "lights[0].cosInner");
    m_parameters[U_LIGHT0_EXPONENT] = glGetUniformLocation(m_programID, "lights[0].exponent");
    m_parameters[U_COLOR_TEXTURE_ENABLED] = glGetUniformLocation(m_programID, "colorTextureEnabled");
    m_parameters[U_COLOR_TEXTURE] = glGetUniformLocation(m_programID, "colorTexture");
    m_parameters[U_TEXT_ENABLED] = glGetUniformLocation(m_programID, "textEnabled");
    m_parameters[U_TEXT_COLOR] = glGetUniformLocation(m_programID, "textColor");
    glUseProgram(m_programID);

    // Set up default directional light
    lights[0].type = Light::LIGHT_DIRECTIONAL;
    lights[0].position.Set(0, 20, 0); lights[0].color.Set(1, 1, 1);
    lights[0].power = 1; lights[0].kC = 1.f; lights[0].kL = 0.01f; lights[0].kQ = 0.001f;
    lights[0].cosCutoff = cos(Math::DegreeToRadian(45));
    lights[0].cosInner = cos(Math::DegreeToRadian(30));
    lights[0].exponent = 3.f; lights[0].spotDirection.Set(0.f, 1.f, 0.f);

    // Set default light uniforms
    glUniform1i(m_parameters[U_NUMLIGHTS], 0);
    glUniform1i(m_parameters[U_TEXT_ENABLED], 0);
    glUniform1i(m_parameters[U_LIGHT0_TYPE], lights[0].type);
    glUniform3fv(m_parameters[U_LIGHT0_COLOR], 1, &lights[0].color.r);
    glUniform1f(m_parameters[U_LIGHT0_POWER], lights[0].power);
    glUniform1f(m_parameters[U_LIGHT0_KC], lights[0].kC);
    glUniform1f(m_parameters[U_LIGHT0_KL], lights[0].kL);
    glUniform1f(m_parameters[U_LIGHT0_KQ], lights[0].kQ);
    glUniform1f(m_parameters[U_LIGHT0_COSCUTOFF], lights[0].cosCutoff);
    glUniform1f(m_parameters[U_LIGHT0_COSINNER], lights[0].cosInner);
    glUniform1f(m_parameters[U_LIGHT0_EXPONENT], lights[0].exponent);

    // Initialize camera
    camera.Init(Vector3(0, 0, 1), Vector3(0, 0, 0), Vector3(0, 1, 0));

    // Initialize mesh list
    for (int i = 0; i < NUM_GEOMETRY; ++i) { meshList[i] = NULL; }
    meshList[GEO_SHIP] = MeshBuilder::GenerateQuad("ship", Color(1, 1, 1), 1);
    meshList[GEO_SHIP]->textureID = LoadTGA("Image//PlayerShipBlue.tga");
    meshList[GEO_GUARDIAN] = MeshBuilder::GenerateQuad("Guardian", Color(1, 1, 1), 1);
    meshList[GEO_GUARDIAN]->textureID = LoadTGA("Image//Guardian.tga");
    meshList[GEO_ENEMY] = MeshBuilder::GenerateQuad("Enemy", Color(1, 1, 1), 1);
    meshList[GEO_ENEMY]->textureID = LoadTGA("Image//Enemy.tga");
    meshList[GEO_BOSS] = MeshBuilder::GenerateQuad("Boss", Color(1, 1, 1), 1);
    meshList[GEO_BOSS]->textureID = LoadTGA("Image//BOSS.tga");
    meshList[GEO_Asteroid] = MeshBuilder::GenerateQuad("Asteroid", Color(1, 1, 1), 1);
    meshList[GEO_Asteroid]->textureID = LoadTGA("Image//Asteroid.tga");
    meshList[GEO_POWERUP] = MeshBuilder::GenerateQuad("Powerup", Color(1, 1, 1), 1);
    meshList[GEO_POWERUP]->textureID = LoadTGA("Image//PowerUp.tga");
    meshList[GEO_AXES] = MeshBuilder::GenerateAxes("reference", 1000, 1000, 1000);
    meshList[GEO_BALL] = MeshBuilder::GenerateSphere("ball", Color(1, 1, 0), 10, 10, 1.f);
    meshList[GEO_ENEMYBALL] = MeshBuilder::GenerateSphere("ball", Color(1, 0, 1), 10, 10, 1.f);
    meshList[GEO_PULSEBULLET] = MeshBuilder::GenerateTriangle("Triangle", Color(0, 1, 0), 1.f);
    meshList[GEO_MISSILE] = MeshBuilder::GenerateQuad("Missile", Color(1, 1, 1), 1);
    meshList[GEO_MISSILE]->textureID = LoadTGA("Image//Missile.tga");
    meshList[GEO_CUBE] = MeshBuilder::GenerateCube("cube", Color(1, 1, 1), 2.f);
    meshList[GEO_BACKGROUND1] = MeshBuilder::GenerateQuad("BackGround", Color(1, 1, 1), 1);
    meshList[GEO_BACKGROUND1]->textureID = LoadTGA("Image//BackGround1.tga");
    meshList[GEO_BACKGROUND2] = MeshBuilder::GenerateQuad("BackGround", Color(1, 1, 1), 1);
    meshList[GEO_BACKGROUND2]->textureID = LoadTGA("Image//BackGround2.tga");
    meshList[GEO_BACKGROUND3] = MeshBuilder::GenerateQuad("BackGround", Color(1, 1, 1), 1);
    meshList[GEO_BACKGROUND3]->textureID = LoadTGA("Image//BackGround3.tga");
    meshList[GEO_PARALLAXLAYER2] = MeshBuilder::GenerateQuad("ParallaxLayer", Color(1, 1, 1), 1);
    meshList[GEO_PARALLAXLAYER2]->textureID = LoadTGA("Image//ParallaxLayer2.tga");
    meshList[GEO_PARALLAXLAYER3] = MeshBuilder::GenerateQuad("ParallaxLayer", Color(1, 1, 1), 1);
    meshList[GEO_PARALLAXLAYER3]->textureID = LoadTGA("Image//ParallaxLayer3.tga");
    meshList[GEO_BLACKHOLE] = MeshBuilder::GenerateQuad("BlackHole", Color(1, 1, 1), 1);
    meshList[GEO_BLACKHOLE]->textureID = LoadTGA("Image//BlackHole.tga");
    meshList[GEO_PLANET1] = MeshBuilder::GenerateQuad("Planet1", Color(1, 1, 1), 1);
    meshList[GEO_PLANET1]->textureID = LoadTGA("Image//Planet1.tga");
    meshList[GEO_PLANET2] = MeshBuilder::GenerateQuad("Planet2", Color(1, 1, 1), 1);
    meshList[GEO_PLANET2]->textureID = LoadTGA("Image//Planet2.tga");
    meshList[GEO_PLANET3] = MeshBuilder::GenerateQuad("Planet3", Color(1, 1, 1), 1);
    meshList[GEO_PLANET3]->textureID = LoadTGA("Image//Planet3.tga");
    meshList[GEO_PLANET4] = MeshBuilder::GenerateQuad("Planet4", Color(1, 1, 1), 1);
    meshList[GEO_PLANET4]->textureID = LoadTGA("Image//Planet4.tga");
    meshList[GEO_MENU] = MeshBuilder::GenerateQuad("Menu", Color(1, 1, 1), 1);
    meshList[GEO_MENU]->textureID = LoadTGA("Image//StartScreen.tga");
    meshList[GEO_GAMESCRN] = MeshBuilder::GenerateQuad("GameScrn", Color(1, 1, 1), 1);
    meshList[GEO_GAMESCRN]->textureID = LoadTGA("Image//InGame.tga");
    meshList[GEO_GAMEOVER] = MeshBuilder::GenerateQuad("GameOver", Color(1, 1, 1), 1);
    meshList[GEO_GAMEOVER]->textureID = LoadTGA("Image//GameOver.tga");
    meshList[GEO_HEALTHGREEN] = MeshBuilder::GenerateQuad("Health", Color(0, 1, 0), 1);
    meshList[GEO_HEALTHYELLOW] = MeshBuilder::GenerateQuad("Health", Color(1, 1, 0), 1);
    meshList[GEO_HEALTHRED] = MeshBuilder::GenerateQuad("Health", Color(1, 0, 0), 1);
    meshList[GEO_LOCKED] = MeshBuilder::GenerateQuad("Lock", Color(1, 1, 1), 1);
    meshList[GEO_LOCKED]->textureID = LoadTGA("Image//Lock.tga");
    meshList[GEO_TEXT] = MeshBuilder::GenerateText("text", 16, 16);
    meshList[GEO_TEXT]->textureID = LoadTGA("Image//calibri.tga");
    meshList[GEO_TEXT]->material.kAmbient.Set(1, 0, 0);

    bLightEnabled = false;
}

/**
 * @brief Updates scene logic and toggles rendering options.
 * @param dt Delta time (in seconds).
 */
void SceneBase::Update(double dt)
{
    if (Application::IsKeyPressed('5')) glEnable(GL_CULL_FACE);
    if (Application::IsKeyPressed('6')) glDisable(GL_CULL_FACE);
    if (Application::IsKeyPressed('7')) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (Application::IsKeyPressed('8')) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    fps = 1.f / static_cast<float>(dt);
}

/**
 * @brief Renders text using the specified mesh.
 * @param mesh Pointer to the mesh used for text.
 * @param text String to render.
 * @param color Color of the text.
 */
void SceneBase::RenderText(Mesh* mesh, std::string text, Color color)
{
    if (!mesh || mesh->textureID <= 0) return;
    glDisable(GL_DEPTH_TEST);
    glUniform1i(m_parameters[U_TEXT_ENABLED], 1);
    glUniform3fv(m_parameters[U_TEXT_COLOR], 1, &color.r);
    glUniform1i(m_parameters[U_LIGHTENABLED], 0);
    glUniform1i(m_parameters[U_COLOR_TEXTURE_ENABLED], 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mesh->textureID);
    glUniform1i(m_parameters[U_COLOR_TEXTURE], 0);
    for (unsigned i = 0; i < text.length(); ++i)
    {
        Mtx44 characterSpacing;
        characterSpacing.SetToTranslation(i * 1.0f, 0, 0);
        Mtx44 MVP = projectionStack.Top() * viewStack.Top() * modelStack.Top() * characterSpacing;
        glUniformMatrix4fv(m_parameters[U_MVP], 1, GL_FALSE, &MVP.a[0]);
        mesh->Render((unsigned)text[i] * 6, 6);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUniform1i(m_parameters[U_TEXT_ENABLED], 0);
    glEnable(GL_DEPTH_TEST);
}

/**
 * @brief Renders text on the screen with an orthographic projection.
 * @param mesh Pointer to the mesh used for text.
 * @param text String to render.
 * @param color Text color.
 * @param size Scale factor.
 * @param x Horizontal position.
 * @param y Vertical position.
 */
void SceneBase::RenderTextOnScreen(Mesh* mesh, std::string text, Color color, float size, float x, float y)
{
    if (!mesh || mesh->textureID <= 0) return;
    glDisable(GL_DEPTH_TEST);
    Mtx44 ortho; ortho.SetToOrtho(0, 80, 0, 60, -10, 10);
    projectionStack.PushMatrix(); projectionStack.LoadMatrix(ortho);
    viewStack.PushMatrix(); viewStack.LoadIdentity();
    modelStack.PushMatrix(); modelStack.LoadIdentity();
    modelStack.Translate(x, y, 0); modelStack.Scale(size, size, size);
    glUniform1i(m_parameters[U_TEXT_ENABLED], 1);
    glUniform3fv(m_parameters[U_TEXT_COLOR], 1, &color.r);
    glUniform1i(m_parameters[U_LIGHTENABLED], 0);
    glUniform1i(m_parameters[U_COLOR_TEXTURE_ENABLED], 1);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mesh->textureID);
    glUniform1i(m_parameters[U_COLOR_TEXTURE], 0);
    for (unsigned i = 0; i < text.length(); ++i)
    {
        Mtx44 characterSpacing;
        characterSpacing.SetToTranslation(i * 1.0f + 0.5f, 0.5f, 0);
        Mtx44 MVP = projectionStack.Top() * viewStack.Top() * modelStack.Top() * characterSpacing;
        glUniformMatrix4fv(m_parameters[U_MVP], 1, GL_FALSE, &MVP.a[0]);
        mesh->Render((unsigned)text[i] * 6, 6);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUniform1i(m_parameters[U_TEXT_ENABLED], 0);
    modelStack.PopMatrix(); viewStack.PopMatrix(); projectionStack.PopMatrix();
    glEnable(GL_DEPTH_TEST);
}

/**
 * @brief Renders a mesh with optional lighting.
 * @param mesh Pointer to the mesh.
 * @param enableLight Set to true to apply lighting.
 */
void SceneBase::RenderMesh(Mesh* mesh, bool enableLight)
{
    Mtx44 MVP = projectionStack.Top() * viewStack.Top() * modelStack.Top();
    glDisable(GL_DEPTH_TEST);
    glUniformMatrix4fv(m_parameters[U_MVP], 1, GL_FALSE, &MVP.a[0]);
    if (enableLight && bLightEnabled)
    {
        glUniform1i(m_parameters[U_LIGHTENABLED], 1);
        Mtx44 modelView = viewStack.Top() * modelStack.Top();
        glUniformMatrix4fv(m_parameters[U_MODELVIEW], 1, GL_FALSE, &modelView.a[0]);
        Mtx44 modelView_IT = modelView.GetInverse().GetTranspose();
        glUniformMatrix4fv(m_parameters[U_MODELVIEW_INVERSE_TRANSPOSE], 1, GL_FALSE, &modelView.a[0]);
        glUniform3fv(m_parameters[U_MATERIAL_AMBIENT], 1, &mesh->material.kAmbient.r);
        glUniform3fv(m_parameters[U_MATERIAL_DIFFUSE], 1, &mesh->material.kDiffuse.r);
        glUniform3fv(m_parameters[U_MATERIAL_SPECULAR], 1, &mesh->material.kSpecular.r);
        glUniform1f(m_parameters[U_MATERIAL_SHININESS], mesh->material.kShininess);
    }
    else glUniform1i(m_parameters[U_LIGHTENABLED], 0);
    if (mesh->textureID > 0)
    {
        glUniform1i(m_parameters[U_COLOR_TEXTURE_ENABLED], 1);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mesh->textureID);
        glUniform1i(m_parameters[U_COLOR_TEXTURE], 0);
    }
    else glUniform1i(m_parameters[U_COLOR_TEXTURE_ENABLED], 0);
    mesh->Render();
    if (mesh->textureID > 0) glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

/**
 * @brief Clears the screen.
 */
void SceneBase::Render() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

/**
 * @brief Cleans up allocated resources.
 */
void SceneBase::Exit()
{
    for (int i = 0; i < NUM_GEOMETRY; ++i)
        if (meshList[i]) delete meshList[i];
    glDeleteProgram(m_programID);
    glDeleteVertexArrays(1, &m_vertexArrayID);
}
