///////////////////////////////////////////////////////////////////////////////
// viewmanager.cpp
// ============
// manage the viewing of 3D objects within the viewport - camera, projection
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	EDITOR: Jacob Hasbrook - SNHU Student
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//	EDITED June 15th 2024
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Enum for orthogonal view states
enum OrthogonalView { TOP_DOWN, SIDE, FRONT };
OrthogonalView currentOrthogonalView = TOP_DOWN; // Start with top-down view, obviously

// declaration of the global variables and defines
namespace
{
    // Apparently, window dimensions need to be constants
    const int WINDOW_WIDTH = 1000;
    const int WINDOW_HEIGHT = 800;
    const char* g_ViewName = "view";
    const char* g_ProjectionName = "projection";

    // The ever-watchful camera object for all your viewing needs
    Camera* g_pCamera = nullptr;

    // Mouse movement variables to make sure we know where you're pointing
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;

    // Time-travel variables to keep track of frame times
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    // Toggle for deciding if you want to see things flat or in 3D
    bool bOrthographicProjection = false;

    // Default camera speed, because moving too fast might make you dizzy
    const float DEFAULT_SPEED = 10.0f;
    const float SPEED_INCREMENT = 1.0f;
}

/***********************************************************
 *  ViewManager()
 *
 *  The constructor for the class. Initializes everything
 *  you might need, or not.
 ***********************************************************/
ViewManager::ViewManager(
    ShaderManager* pShaderManager)
{
    // Initialize the vital components of your view
    m_pShaderManager = pShaderManager;
    m_pWindow = NULL;
    g_pCamera = new Camera();
    // Default camera settings because we all love defaults
    g_pCamera->Position = glm::vec3(0.0f, 5.0f, 12.0f);
    g_pCamera->Front = glm::vec3(0.0f, -0.5f, -2.0f);
    g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
    g_pCamera->Zoom = 80;
    g_pCamera->MovementSpeed = 20;
}

/***********************************************************
 *  ~ViewManager()
 *
 *  The destructor for the class. Cleaning up the mess
 *  we made in the constructor.
 ***********************************************************/
ViewManager::~ViewManager()
{
    // Free up the memory we so generously allocated
    m_pShaderManager = NULL;
    m_pWindow = NULL;
    if (NULL != g_pCamera)
    {
        delete g_pCamera;
        g_pCamera = NULL;
    }
}

/***********************************************************
 *  CreateDisplayWindow()
 *
 *  This method is used to create the main display window.
 *  Because what's a 3D scene without a window, right?
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
    GLFWwindow* window = nullptr;

    // Try not to fail creating the OpenGL window
    window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        windowTitle,
        NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(window);

    // Capture the mouse, like a cat with a laser pointer
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Callback for mouse movements - because why not?
    glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);

    // Callback for mouse scrolling - in case you're into that
    glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Callback);

    // Enable blending because transparency is trendy
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_pWindow = window;

    return(window);
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  Automatically called by GLFW when you move the mouse.
 *  Keeping track of your every move.
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos)
{
    // First mouse move? Record it like it's your first step
    if (gFirstMouse)
    {
        gLastX = static_cast<float>(xMousePos);
        gLastY = static_cast<float>(yMousePos);
        gFirstMouse = false;
    }

    // Calculate the offsets and move the camera
    float xOffset = static_cast<float>(xMousePos) - gLastX;
    float yOffset = gLastY - static_cast<float>(yMousePos); // reversed since y-coordinates go from bottom to top

    // Update the last known positions
    gLastX = static_cast<float>(xMousePos);
    gLastY = static_cast<float>(yMousePos);

    // Move the camera based on your shaky hand movements
    g_pCamera->ProcessMouseMovement(xOffset, yOffset);
}

/***********************************************************
 *  Mouse_Scroll_Callback()
 *
 *  Called automatically by GLFW when you scroll.
 *  Adjusting camera speed because why not?
 ***********************************************************/
void ViewManager::Mouse_Scroll_Callback(GLFWwindow* window, double xOffset, double yOffset)
{
    // Adjust the camera speed with the scroll wheel
    g_pCamera->MovementSpeed += yOffset * SPEED_INCREMENT;

    // Prevent the camera from moving backwards through time
    if (g_pCamera->MovementSpeed < SPEED_INCREMENT)
    {
        g_pCamera->MovementSpeed = SPEED_INCREMENT;
    }
}

/***********************************************************
 *  ProcessKeyboardEvents()
 *
 *  Handling keyboard events like a pro. Or at least trying to.
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
    // Toggle between perspective and orthographic projection
    if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS)
    {
        bOrthographicProjection = false;
        g_pCamera->Position = glm::vec3(0.5f, 5.5f, 10.0f); // Reset to perspective view position
        g_pCamera->Front = glm::vec3(0.0f, 0.0f, -1.0f);    // Ensure the front direction is correct
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS)
    {
        bOrthographicProjection = true;
        // Set to default orthogonal view
        currentOrthogonalView = TOP_DOWN;
        g_pCamera->Position = glm::vec3(0.0f, 20.0f, 0.0f); // Top-down view
        g_pCamera->Front = glm::vec3(0.0f, -1.0f, 0.0f);    // Look direction straight down
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_L) == GLFW_PRESS && bOrthographicProjection)
    {
        // Cycle through orthogonal views
        switch (currentOrthogonalView)
        {
        case TOP_DOWN:
            currentOrthogonalView = SIDE;
            g_pCamera->Position = glm::vec3(20.0f, 5.0f, 0.0f); // Side view
            g_pCamera->Front = glm::vec3(-1.0f, 0.0f, 0.0f);    // Look direction straight at the scene
            break;
        case SIDE:
            currentOrthogonalView = FRONT;
            g_pCamera->Position = glm::vec3(0.0f, 5.0f, 20.0f); // Front view
            g_pCamera->Front = glm::vec3(0.0f, 0.0f, -1.0f);    // Look direction straight at the scene
            break;
        case FRONT:
            currentOrthogonalView = TOP_DOWN;
            g_pCamera->Position = glm::vec3(0.0f, 20.0f, 0.0f); // Top-down view
            g_pCamera->Front = glm::vec3(0.0f, -1.0f, 0.0f);    // Look direction straight down
            break;
        }
    }

    // Close the window if you're tired of this view
    if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_pWindow, true);
    }

    // Move the camera forward, because why not
    if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(FORWARD, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(BACKWARD, gDeltaTime);
    }

    // Move the camera left and right
    if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(LEFT, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(RIGHT, gDeltaTime);
    }

    // Move the camera up and down
    if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(UP, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(DOWN, gDeltaTime);
    }
}

/***********************************************************
 *  PrepareSceneView()
 *
 *  Preparing the scene view. Because you need to see things.
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
    glm::mat4 view;
    glm::mat4 projection;

    // Keep track of time, because why not
    float currentFrame = static_cast<float>(glfwGetTime());
    gDeltaTime = currentFrame - gLastFrame;
    gLastFrame = currentFrame;

    // Process those oh-so-important keyboard events
    ProcessKeyboardEvents();

    // Get the camera's view matrix
    view = g_pCamera->GetViewMatrix();

    // Define the projection matrix based on your toggle
    if (bOrthographicProjection)
    {
        float orthoSize = 10.0f;
        projection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);
    }
    else
    {
        projection = glm::perspective(glm::radians(g_pCamera->Zoom), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
    }

    // If the shader manager is still around, set the view and projection
    if (NULL != m_pShaderManager)
    {
        // Set the view matrix in the shader
        m_pShaderManager->setMat4Value(g_ViewName, view);
        // Set the projection matrix in the shader
        m_pShaderManager->setMat4Value(g_ProjectionName, projection);
        // Set the camera's position in the shader
        m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
    }
}
