
#include <stdio.h>
#include <chrono>
#include <thread>

#include "imgui/imgui.h"
#include "imgui_impl_glfw_game.h"
#include "imgui_impl_opengl3_game.h"
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "draw_game.h"
#include <iostream>
#include <random>
#include <cmath>

#include "box2d/box2d.h"

// GLFW main window pointer
GLFWwindow* g_mainWindow = nullptr;
b2World* g_world;
b2Body* g_shipBody;

const float PIXELS_PER_UNIT = 20.0f;
const uint16 CATEGORY_SPACESHIP = 0x0001;
const uint16 CATEGORY_ASTEROID = 0x0002;
const uint16 MASK_SPACESHIP = CATEGORY_ASTEROID;
const uint16 MASK_ASTEROID = CATEGORY_SPACESHIP;

bool g_rotateLeft = false;
bool g_rotateRight = false;
bool g_accelerate = false;


double GenerateRandom(float lower, float upper)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(lower, upper);
    double random_value = dis(gen);
    // std::cout << "Random value: " << random_value << std::endl;
    return random_value;
}

b2Vec2 GenerateRandomAsteroidPos()
{
    // Window width and height
    int width = 70;
    int height = 50;

    double large = GenerateRandom(0.0f, height);
    double small = GenerateRandom(0.0f, 10.0f);
    int side = static_cast<int>(std::ceil(GenerateRandom(0.0f, 4.0f)));

    b2Vec2 vec;

    switch (side)
    {
    case 1:
        // Translate random position to correct coordinates
        vec.Set(large - height / 2, small);
        std::cout << "Asteroid position: " << vec.x << ", " << vec.y << ". Bottom side." << std::endl;
        break;
    case 2:
        // Translate random position to correct coordinates
        vec.Set(small - width / 2, large);
        std::cout << "Asteroid position: " << vec.x << ", " << vec.y << ". Left side." << std::endl;
        break;
    case 3:
        // Translate random position to correct coordinates
        vec.Set(large - height / 2, height - small);
        std::cout << "Asteroid position: " << vec.x << ", " << vec.y << ". Top side." << std::endl;
        break;
    case 4:
        // Translate random position to correct coordinates
        vec.Set(width / 2 - small, large);
        std::cout << "Asteroid position: " << vec.x << ", " << vec.y << ". Right side." << std::endl;
        break;
    default:
        vec.Set(0.0f, 0.0f);
        std::cout << "Warning: Invalid side index: " << side << std::endl;
        break;
    }

    return vec;
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Left, right to rotate spaceship, up arrow for thrust, space to fire projectiles
    // Choose a key for random teleport

    if (action == GLFW_RELEASE)
    {
        std::cout << "Release key" << std::endl;
        switch (key)
        {
        case GLFW_KEY_LEFT:
            std::cout << "Stop rotate counterclockwise" << std::endl;
            g_rotateLeft = false;
            break;
        case GLFW_KEY_RIGHT:
            std::cout << "Stop rotate clockwise" << std::endl;
            g_rotateRight = false;
            break;
        case GLFW_KEY_UP:
            std::cout << "Accelerate" << std::endl;
            g_accelerate = false;
            break;
        default:
            break;
        }
    }
    else if (action == GLFW_REPEAT)
    {
        std::cout << "Hold down key" << std::endl;
    }
    else if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_LEFT:
            std::cout << "Rotate counterclockwise" << std::endl;
            g_rotateLeft = true;
            break;
        case GLFW_KEY_RIGHT:
            std::cout << "Rotate clockwise" << std::endl;
            g_rotateRight = true;
            break;
        case GLFW_KEY_UP:
            std::cout << "Accelerate" << std::endl;
            g_accelerate = true;
            break;
        case GLFW_KEY_SPACE:
            std::cout << "Fire projectile" << std::endl;
            break;
        case GLFW_KEY_RIGHT_CONTROL:
            std::cout << "Random teleport" << std::endl;
            {
                double x = GenerateRandom(-35.0f, 35.0f);
                double y = GenerateRandom(0.0f, 50.0f);
                b2Vec2 newPosition(x, y);
                g_shipBody->SetTransform(newPosition, g_shipBody->GetAngle());
            }
            break;
        default:
            break;
        }
    }

    // code for keys here https://www.glfw.org/docs/3.3/group__keys.html
    // and modifiers https://www.glfw.org/docs/3.3/group__mods.html
}

void MouseMotionCallback(GLFWwindow*, double xd, double yd)
{
    // get the position where the mouse was pressed
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
}

void MouseButtonCallback(GLFWwindow* window, int32 button, int32 action, int32 mods)
{
    // code for mouse button keys https://www.glfw.org/docs/3.3/group__buttons.html
    // and modifiers https://www.glfw.org/docs/3.3/group__buttons.html
    // action is either GLFW_PRESS or GLFW_RELEASE
    double xd, yd;
    // get the position where the mouse was pressed
    glfwGetCursorPos(g_mainWindow, &xd, &yd);
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);

}

void CreateSpaceship(float x, float y)
{
    b2BodyDef shipBodyDef;
    shipBodyDef.type = b2_dynamicBody;
    shipBodyDef.position.Set(0.0f, 20.0f); // starting position
    g_shipBody = g_world->CreateBody(&shipBodyDef);

    b2PolygonShape shipShape;
    // Define the vertices for the triangle (representing the ship)
    b2Vec2 vertices[3];
    vertices[0].Set(-1, -2);
    vertices[1].Set(1, -2);
    vertices[2].Set(0, 2);
    shipShape.Set(vertices, 3);

    b2FixtureDef shipFixture;
    shipFixture.shape = &shipShape;
    shipFixture.density = 1.0f;
    shipFixture.filter.categoryBits = CATEGORY_SPACESHIP;
    shipFixture.filter.maskBits = MASK_SPACESHIP;
    g_shipBody->CreateFixture(&shipFixture);
    g_shipBody->SetLinearDamping(0.5f);
}

void CreateAsteroid(float x, float y, float size)
{
    b2BodyDef asteroidBodyDef;
    asteroidBodyDef.type = b2_dynamicBody;
    asteroidBodyDef.position.Set(x, y);

    b2Body* asteroidBody = g_world->CreateBody(&asteroidBodyDef);

    b2PolygonShape box_shape;
    box_shape.SetAsBox(size, size);

    b2FixtureDef asteroidFixture;
    asteroidFixture.shape = &box_shape;
    asteroidFixture.density = 1.0f;
    asteroidFixture.filter.categoryBits = CATEGORY_ASTEROID;
    asteroidFixture.filter.maskBits = MASK_ASTEROID;
    asteroidBody->CreateFixture(&asteroidFixture);

    double y_speed = GenerateRandom(0.0f, 8.0f) - 4.0f;
    double x_direction = GenerateRandom(0.0f, 2.0f) - 1.0f > 0.0f ? 1 : -1;
    double x_speed = (4.0f - fabs(y_speed)) * x_direction;
    b2Vec2 initialVelocity(x_speed, y_speed); // Define velocity vector
    asteroidBody->SetLinearVelocity(initialVelocity);
    std::cout << "Vel: " << x_speed << ", " << y_speed << ": " << x_direction << std::endl;
}


int main()
{

    // glfw initialization things
    if (glfwInit() == 0) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    g_mainWindow = glfwCreateWindow(g_camera.m_width, g_camera.m_height, "My game", NULL, NULL);

    if (g_mainWindow == NULL) {
        fprintf(stderr, "Failed to open GLFW g_mainWindow.\n");
        glfwTerminate();
        return -1;
    }

    // Set callbacks using GLFW
    glfwSetMouseButtonCallback(g_mainWindow, MouseButtonCallback);
    glfwSetKeyCallback(g_mainWindow, KeyCallback);
    glfwSetCursorPosCallback(g_mainWindow, MouseMotionCallback);

    glfwMakeContextCurrent(g_mainWindow);

    // Load OpenGL functions using glad
    int version = gladLoadGL(glfwGetProcAddress);

    // Setup Box2D world and with some gravity
    b2Vec2 gravity;
    //gravity.Set(0.0f, -10.0f);
    gravity.Set(0.0f, 0.0f);
    g_world = new b2World(gravity);

    // Create debug draw. We will be using the debugDraw visualization to create
    // our games. Debug draw calls all the OpenGL functions for us.
    g_debugDraw.Create();
    g_world->SetDebugDraw(&g_debugDraw);
    CreateUI(g_mainWindow, 20.0f /* font size in pixels */);


    // Some starter objects are created here, such as the ground
    /* b2Body* ground;
    b2EdgeShape ground_shape;
    ground_shape.SetTwoSided(b2Vec2(-40.0f, 0.0f), b2Vec2(40.0f, 0.0f));
    b2BodyDef ground_bd;
    ground = g_world->CreateBody(&ground_bd);
    ground->CreateFixture(&ground_shape, 0.0f);

    b2Body* box;
    b2PolygonShape box_shape;
    box_shape.SetAsBox(1.0f, 1.0f);
    b2FixtureDef box_fd;
    box_fd.shape = &box_shape;
    box_fd.density = 20.0f;
    box_fd.friction = 0.1f;
    b2BodyDef box_bd;
    box_bd.type = b2_dynamicBody;
    box_bd.position.Set(-5.0f, 11.25f);
    box = g_world->CreateBody(&box_bd);
    box->CreateFixture(&box_fd);*/

    // Create Spaceship

    CreateSpaceship(0.0f, 0.0f);

    // Create Asteroids

    b2Vec2 v1 = GenerateRandomAsteroidPos(); 
    CreateAsteroid(v1.x, v1.y, 3.0f);

    b2Vec2 v2 = GenerateRandomAsteroidPos();
    CreateAsteroid(v2.x, v2.y, 3.0f);

    b2Vec2 v3 = GenerateRandomAsteroidPos();
    CreateAsteroid(v3.x, v3.y, 3.0f);

    b2Vec2 v4 = GenerateRandomAsteroidPos();
    CreateAsteroid(v4.x, v4.y, 3.0f);

    //

    // This is the color of our background in RGB components
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Control the frame rate. One draw per monitor refresh.
    std::chrono::duration<double> frameTime(0.0);
    std::chrono::duration<double> sleepAdjust(0.0);

    // Main application loop
    while (!glfwWindowShouldClose(g_mainWindow)) {
        // Use std::chrono to control frame rate. Objective here is to maintain
        // a steady 60 frames per second (no more, hopefully no less)
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

        glfwGetWindowSize(g_mainWindow, &g_camera.m_width, &g_camera.m_height);

        int bufferWidth, bufferHeight;
        glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
        glViewport(0, 0, bufferWidth, bufferHeight);

        // Clear previous frame (avoid creates shadows)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Setup ImGui attributes so we can draw text on the screen. Basically
        // create a window of the size of our viewport.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
        ImGui::End();

        // Enable objects to be draw
        uint32 flags = 0;
        flags += b2Draw::e_shapeBit;
        g_debugDraw.SetFlags(flags);

        // When we call Step(), we run the simulation for one frame
        float timeStep = 60 > 0.0f ? 1.0f / 60 : float(0.0f);
        g_world->Step(timeStep, 8, 3);

        if (g_rotateLeft)
        {
            g_shipBody->SetTransform(g_shipBody->GetPosition(), g_shipBody->GetAngle() + 0.1f);
        }

        if (g_rotateRight)
        {
            g_shipBody->SetTransform(g_shipBody->GetPosition(), g_shipBody->GetAngle() - 0.1f);
        }

        if (g_accelerate)
        {
            // Calculate the force of accelation with a diminishing return. The faster
            // the ship goes, the less able the ship's propulsion is to accelerate the
            // ship. This is to prevent unlimited acceleration in the game.
            //
            float currentVelocityMagnitude = g_shipBody->GetLinearVelocity().Length();
            float maxVelocity = 50.0f; // Define a maximum reasonable velocity for your spaceship.
            float adjustmentFactor = 1.0f - (currentVelocityMagnitude / maxVelocity);
            float baseMagnitude = 80.0f; // Define this value based on desired acceleration.
            float adjustedMagnitude = baseMagnitude * adjustmentFactor;

            // Calculate the direction of acceleration.
            //
            float angle = g_shipBody->GetAngle();
            angle += b2_pi / 2; // Adjust angle by 90 degrees
            b2Vec2 forceDirection(cos(angle), sin(angle));

            // Apply the magnitude of acceleration to the direction of acceleration.
            //
            forceDirection *= adjustedMagnitude;
            g_shipBody->ApplyForceToCenter(forceDirection, true);
        }

        // World wrap-around
        for (b2Body* b = g_world->GetBodyList(); b; b = b->GetNext())
        {
            b2Vec2 pos = b->GetPosition();
            bool wrapped = false;

            // Check outside right and left borders
            if (pos.x > (g_camera.m_width / PIXELS_PER_UNIT) / 2.0f)
            {
                pos.x = -(g_camera.m_width / PIXELS_PER_UNIT) / 2;
                wrapped = true;
            }
            else if (pos.x < -(g_camera.m_width / PIXELS_PER_UNIT) / 2.0f)
            {
                pos.x = (g_camera.m_width / PIXELS_PER_UNIT) / 2;
                wrapped = true;
            }

            // Check outside top and bottom borders
            if (pos.y > g_camera.m_height / PIXELS_PER_UNIT)
            {
                pos.y = 0;
                wrapped = true;
            }
            else if (pos.y < 0)
            {
                pos.y = g_camera.m_height / PIXELS_PER_UNIT;
                wrapped = true;
            }

            // Adjust position
            if (wrapped)
            {
                b->SetTransform(pos, b->GetAngle());
            }
        }

        // Render everything on the screen
        g_world->DebugDraw();
        g_debugDraw.Flush();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(g_mainWindow);

        // Process events (mouse and keyboard) and call the functions we
        // registered before.
        glfwPollEvents();

        // Throttle to cap at 60 FPS. Which means if it's going to be past
        // 60FPS, sleeps a while instead of doing more frames.
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        std::chrono::duration<double> target(1.0 / 60.0);
        std::chrono::duration<double> timeUsed = t2 - t1;
        std::chrono::duration<double> sleepTime = target - timeUsed + sleepAdjust;
        if (sleepTime > std::chrono::duration<double>(0)) {
            // Make the framerate not go over by sleeping a little.
            std::this_thread::sleep_for(sleepTime);
        }
        std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();
        frameTime = t3 - t1;

        // Compute the sleep adjustment using a low pass filter
        sleepAdjust = 0.9 * sleepAdjust + 0.1 * (target - frameTime);

    }

    // Terminate the program if it reaches here
    glfwTerminate();
    g_debugDraw.Destroy();
    delete g_world;

    return 0;
}
