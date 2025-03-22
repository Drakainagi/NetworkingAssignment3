#ifndef SCENE_BASE_H
#define SCENE_BASE_H

#include "Scene.h"
#include "Mtx44.h"
#include "Camera.h"
#include "Mesh.h"
#include "MatrixStack.h"
#include "Light.h"
#include "GameObject.h"
#include <vector>

/**
 * @brief Base class for scenes.
 *
 * This class provides common functionalities such as initializing the scene,
 * updating logic, rendering, and cleanup. It also handles the setup of shaders,
 * camera, lighting, and various meshes used in the scene.
 */
class SceneBase : public Scene
{
public:
    /**
     * @brief Enumeration of uniform types used in shader programs.
     */
    enum UNIFORM_TYPE
    {
        U_MVP = 0,
        U_MODELVIEW,
        U_MODELVIEW_INVERSE_TRANSPOSE,
        U_MATERIAL_AMBIENT,
        U_MATERIAL_DIFFUSE,
        U_MATERIAL_SPECULAR,
        U_MATERIAL_SHININESS,
        U_LIGHTENABLED,
        U_NUMLIGHTS,
        U_LIGHT0_TYPE,
        U_LIGHT0_POSITION,
        U_LIGHT0_COLOR,
        U_LIGHT0_POWER,
        U_LIGHT0_KC,
        U_LIGHT0_KL,
        U_LIGHT0_KQ,
        U_LIGHT0_SPOTDIRECTION,
        U_LIGHT0_COSCUTOFF,
        U_LIGHT0_COSINNER,
        U_LIGHT0_EXPONENT,
        U_COLOR_TEXTURE_ENABLED,
        U_COLOR_TEXTURE,
        U_TEXT_ENABLED,
        U_TEXT_COLOR,
        U_TOTAL,
    };

    /**
     * @brief Enumeration of geometry types (meshes) used in the scene.
     */
    enum GEOMETRY_TYPE
    {
        GEO_MENU,
        GEO_GAMESCRN,
        GEO_GAMEOVER,
        GEO_SHIP,
        GEO_Asteroid,
        GEO_ENEMY,
        GEO_BOSS,
        GEO_GUARDIAN,
        GEO_POWERUP,
        GEO_AXES,
        GEO_TEXT,
        GEO_BALL,
        GEO_ENEMYBALL,
        GEO_PULSEBULLET,
        GEO_MISSILE,
        GEO_CUBE,
        GEO_BACKGROUND1,
        GEO_BACKGROUND2,
        GEO_BACKGROUND3,
        GEO_PARALLAXLAYER2,
        GEO_PARALLAXLAYER3,
        GEO_PLANET1,
        GEO_PLANET2,
        GEO_PLANET3,
        GEO_PLANET4,
        GEO_BLACKHOLE,
        GEO_HEALTHGREEN,
        GEO_HEALTHYELLOW,
        GEO_HEALTHRED,
        GEO_LOCKED,
        GEO_CAMOGREY,
        NUM_GEOMETRY,
    };

public:
    /**
     * @brief Constructor for SceneBase.
     */
    SceneBase();

    /**
     * @brief Destructor for SceneBase.
     */
    ~SceneBase();

    /**
     * @brief Initializes the scene.
     *
     * This function sets up OpenGL states, loads shaders, retrieves uniform locations,
     * configures lighting, initializes the camera, and loads meshes.
     */
    virtual void Init();

    /**
     * @brief Updates the scene.
     *
     * @param dt Delta time (in seconds) since the last frame.
     */
    virtual void Update(double dt);

    /**
     * @brief Renders the scene.
     *
     * Clears the screen and draws all objects within the scene.
     */
    virtual void Render();

    /**
     * @brief Cleans up the scene.
     *
     * Deletes meshes, shader programs, and any allocated resources.
     */
    virtual void Exit();

    /**
     * @brief Renders text using the specified mesh.
     *
     * @param mesh Pointer to the Mesh object representing the text.
     * @param text The string to be rendered.
     * @param color The color of the text.
     */
    void RenderText(Mesh* mesh, std::string text, Color color);

    /**
     * @brief Renders text on the screen with an orthographic projection.
     *
     * @param mesh Pointer to the Mesh object representing the text.
     * @param text The string to be rendered.
     * @param color The color of the text.
     * @param size Scale factor for the text.
     * @param x Horizontal screen coordinate.
     * @param y Vertical screen coordinate.
     */
    void RenderTextOnScreen(Mesh* mesh, std::string text, Color color, float size, float x, float y);

    /**
     * @brief Renders a mesh.
     *
     * @param mesh Pointer to the Mesh object to render.
     * @param enableLight Boolean flag indicating whether lighting should be applied.
     */
    void RenderMesh(Mesh* mesh, bool enableLight);

protected:
    unsigned m_vertexArrayID;            ///< Vertex Array Object ID.
    Mesh* meshList[NUM_GEOMETRY];          ///< List of mesh pointers.
    unsigned m_programID;                ///< Shader program ID.
    unsigned m_parameters[U_TOTAL];      ///< Array of shader uniform locations.

    Camera camera;                       ///< Camera object.

    MS modelStack;                       ///< Model matrix stack.
    MS viewStack;                        ///< View matrix stack.
    MS projectionStack;                  ///< Projection matrix stack.

    Light lights[1];                     ///< Array of lights (currently only one).

    bool bLightEnabled;                  ///< Flag for whether lighting is enabled.

    float fps;                           ///< Frames per second.
};

#endif
