
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


GLFWwindow* g_mainWindow;
b2World* g_world;
b2Body* g_shipBody;
std::vector<b2Body*> g_bodiesToDestroy;


const float PIXELS_PER_UNIT = 20.0f;

// The following will make it possible for asteroids to pass through each other
// just like in the classic arcade game. The spaceship will still collide with
// the asteroids, but this should lead to the immediate destruction of the ship.
// Projectiles will also collide with asteroids, and this should trigger the
// destruction of the projectile and the break up or destruction of the asteroid.
//
const uint16 CATEGORY_SPACESHIP = 0x0001;
const uint16 CATEGORY_PROJECTILE = 0x0002;
const uint16 CATEGORY_ASTEROID = 0x0004;
const uint16 CATEGORY_MEDIUM = 0x0008;
const uint16 CATEGORY_SMALL = 0x0010;
const uint16 MASK_PROJECTILE = CATEGORY_ASTEROID;
const uint16 MASK_SPACESHIP = CATEGORY_ASTEROID;
const uint16 MASK_ASTEROID = CATEGORY_SPACESHIP | CATEGORY_PROJECTILE;

// We can't rely on the GLFW_REPEAT key event to handle smooth action when
// holding a key down. The flags make for a better user experience.
bool g_rotateLeft = false;
bool g_rotateRight = false;
bool g_accelerate = false;


double GenerateRandom(float lower, float upper)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(lower, upper);
    double random_value = dis(gen);
    return random_value;
}

b2Vec2 GenerateRandomAsteroidPos()
{
    // Window width and height in Box2D terms, estimated.
    int width = 70;
    int height = 50;

    // Make asteroids appear near the edge of the screen.
    //
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

void CreateStartingAsteroid()
{
    b2BodyDef asteroidBodyDef;
    asteroidBodyDef.type = b2_dynamicBody;
    b2Vec2 randomPos = GenerateRandomAsteroidPos();
    asteroidBodyDef.position.Set(randomPos.x, randomPos.y);
    b2Body* asteroidBody = g_world->CreateBody(&asteroidBodyDef);

    b2PolygonShape box_shape;
    box_shape.SetAsBox(3.0f, 3.0f); // TODO: Improve shape

    b2FixtureDef asteroidFixture;
    asteroidFixture.shape = &box_shape;
    asteroidFixture.density = 1.0f;
    asteroidFixture.filter.categoryBits = CATEGORY_ASTEROID;
    asteroidFixture.filter.maskBits = MASK_ASTEROID;
    asteroidBody->CreateFixture(&asteroidFixture);

    // Starting asteroids have a random direction of motion
    //
    float desiredVelocity = 4.0f; // Adjust as needed
    float angle = GenerateRandom(0, 2 * b2_pi); // Radians are based on 2*pi
    b2Vec2 directionOfMotion(cos(angle), sin(angle));
    directionOfMotion *= desiredVelocity;
    asteroidBody->SetLinearVelocity(directionOfMotion);
    //std::cout << "Vel: " << x_speed << ", " << y_speed << ": " << x_direction << std::endl;
}

class MyContactListener : public b2ContactListener
{
    void BeginContact(b2Contact* contact) override
    {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        b2Body* bodyA = fixtureA->GetBody();
        b2Body* bodyB = fixtureB->GetBody();
        uint16 categoryOfA = fixtureA->GetFilterData().categoryBits;
        uint16 categoryOfB = fixtureB->GetFilterData().categoryBits;

        // Both the projectile and the object hit will be scheduled for destruction.
        //
        g_bodiesToDestroy.push_back(bodyA);
        g_bodiesToDestroy.push_back(bodyB);
    }
};

class Projectile
{
    b2Body* body = nullptr;
    float distanceTraveled = 0.0f;
    const float maxRange = 50.0f; // Define your desired max range

public:
    Projectile(b2World* world, b2Vec2 position, b2Vec2 velocity)
    {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = position;
        body = world->CreateBody(&bodyDef);

        b2CircleShape shape;
        shape.m_radius = 0.2f; // Adjust based on desired size

        b2FixtureDef projectileFixture;
        projectileFixture.shape = &shape;
        projectileFixture.density = 1.0f;
        projectileFixture.filter.categoryBits = CATEGORY_PROJECTILE;
        projectileFixture.filter.maskBits = MASK_PROJECTILE;
        body->CreateFixture(&projectileFixture);
        body->SetLinearVelocity(velocity);
    }
};

std::vector<Projectile*> g_activeProjectiles;

void OnShootKeyPressed()
{
    float angle = g_shipBody->GetAngle();
    angle += b2_pi / 2; // Adjust angle by 90 degrees
    b2Vec2 directionOfFire(cos(angle), sin(angle));

    // Assuming the spaceship's nose is at its center for this example
    b2Vec2 launchPosition = g_shipBody->GetPosition() + directionOfFire;

    float projectileSpeed = 40.0f; // Adjust as needed
    directionOfFire *= projectileSpeed;

    // Create the projectile
    Projectile* newProjectile = new Projectile(g_world, launchPosition, directionOfFire);
    g_activeProjectiles.push_back(newProjectile);
}


void RotateShip(float amount)
{
    float adjustedAngle = g_shipBody->GetAngle() + amount;
    g_shipBody->SetTransform(g_shipBody->GetPosition(), adjustedAngle);
    std::cout << "Rotate to " << adjustedAngle << std::endl;
}

void AccelerateSpaceship()
{
    // Calculate the force of accelation with a diminishing return. The faster
    // the ship goes, the less able the ship's propulsion is to accelerate the
    // ship. This is to prevent unlimited acceleration in the game.
    //
    float currentVelocityMagnitude = g_shipBody->GetLinearVelocity().Length();
    float maxVelocity = 50.0f; // Define a maximum reasonable velocity for your spaceship.
    float adjustmentFactor = 1.0f - (currentVelocityMagnitude / maxVelocity);
    float baseMagnitude = 300.0f; // Define this value based on desired acceleration.
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
    std::cout << "Current velocity: " << currentVelocityMagnitude << ", Accelerate by " << adjustedMagnitude << std::endl;
}

void WorldWraparound()
{
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
}

void RandomTeleport()
{
    double x = GenerateRandom(-35.0f, 35.0f);
    double y = GenerateRandom(0.0f, 50.0f);
    b2Vec2 newPosition(x, y);
    g_shipBody->SetTransform(newPosition, g_shipBody->GetAngle());
    std::cout << "Random teleport to " << x << ", " << y << std::endl;
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Left, right keys to rotate spaceship, up arrow for thrust, Space bar to fire projectiles,
    // Right-Ctrl key for emergency random teleport.
    //
    switch (key)
    {
    case GLFW_KEY_LEFT:
        if (action == GLFW_PRESS)
        {
            g_rotateLeft = true;
        }
        else if (action == GLFW_RELEASE)
        {
            g_rotateLeft = false;
        }
        break;
    case GLFW_KEY_RIGHT:
        if (action == GLFW_PRESS)
        {
            g_rotateRight = true;
        }
        else if (action == GLFW_RELEASE)
        {
            g_rotateRight = false;
        }
        break;
    case GLFW_KEY_UP:
        if (action == GLFW_PRESS)
        {
            g_accelerate = true;
        }
        else if (action == GLFW_RELEASE)
        {
            g_accelerate = false;
        }
        break;
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS)
        {
            OnShootKeyPressed();  // This creates a new Projectile.
        }
        break;
    case GLFW_KEY_RIGHT_CONTROL:
        if (action == GLFW_PRESS)
        {
            RandomTeleport();
        }
        break;
    default:
        break;
    }
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

void CreateSpaceship()
{
    b2BodyDef shipBodyDef;
    shipBodyDef.type = b2_dynamicBody; // the ship is a movable object
    shipBodyDef.position.Set(0.0f, 20.0f); // starting position is roughly in the center of the screen
    g_shipBody = g_world->CreateBody(&shipBodyDef);

    // Define the vertices for the triangle (representing the ship)
    b2PolygonShape shipShape;
    b2Vec2 vertices[3];
    vertices[0].Set(-1, -2);
    vertices[1].Set(1, -2);
    vertices[2].Set(0, 2);
    shipShape.Set(vertices, 3);

    b2FixtureDef shipFixture;
    shipFixture.shape = &shipShape;
    shipFixture.density = 1.0f; // determines the mass of the ship
    shipFixture.filter.categoryBits = CATEGORY_SPACESHIP;
    shipFixture.filter.maskBits = MASK_SPACESHIP;
    g_shipBody->CreateFixture(&shipFixture);
    g_shipBody->SetLinearDamping(0.5f); // Creates "drag" for the ship
}

void DestroyBodies()
{
    for (b2Body* body : g_bodiesToDestroy)
    {
        if (body == nullptr || !body->IsEnabled() || body->GetFixtureList() == nullptr) {
            continue;
        }

        uint16 body_category = body->GetFixtureList()[0].GetFilterData().categoryBits;
        std::cout << "Detected destroy: " << body_category << std::endl;
        if (body_category == CATEGORY_ASTEROID)
        {
            std::cout << "Detected destroy large asteroid" << std::endl;
            // Large asteroid becomes two medium asteroids.
        }
        else if (body_category & CATEGORY_MEDIUM)
        {
            std::cout << "Detected destroy medium asteroid" << std::endl;
            // Medium asteroid becomes two small asteroids.
        }
        else if (body_category & CATEGORY_SMALL)
        {
            std::cout << "Detected destroy small asteroid" << std::endl;
            // Small asteroid is simply destroyed.
        }

        std::cout << "Destroying body" << std::endl;
        g_world->DestroyBody(body);
    }
    g_bodiesToDestroy.clear(); // Clear the list for the next frame.
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

    // Set up outer space with no gravity
    b2Vec2 gravity{};
    gravity.Set(0.0f, 0.0f);
    g_world = new b2World(gravity);

    // Create debug draw. We will be using the debugDraw visualization to create
    // our games. Debug draw calls all the OpenGL functions for us.
    g_debugDraw.Create();
    g_world->SetDebugDraw(&g_debugDraw);
    CreateUI(g_mainWindow, 20.0f /* font size in pixels */);

    CreateSpaceship();
    CreateStartingAsteroid();
    CreateStartingAsteroid();
    CreateStartingAsteroid();
    CreateStartingAsteroid();

    // Body collision detection.
    //
    MyContactListener myContactListenerInstance;
    g_world->SetContactListener(&myContactListenerInstance);

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
            RotateShip(0.1f);
        }

        if (g_rotateRight)
        {
            RotateShip(-0.1f);
        }

        if (g_accelerate)
        {
            AccelerateSpaceship();
        }

        // Destroy any bodies that should be destroyed, such as asteroids.
        //
        DestroyBodies();
 
        // When objects reach one side of the screen, they should "teleport" to the other side
        //
        WorldWraparound();

        // Render everything on the screen
        //
        g_world->DebugDraw();
        g_debugDraw.Flush();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(g_mainWindow);

        // Process events (mouse and keyboard) and call the functions we
        // registered before.
        //
        glfwPollEvents();

        // Throttle to cap at 60 FPS. Which means if it's going to be past
        // 60FPS, sleeps a while instead of doing more frames.
        //
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
        //
        sleepAdjust = 0.9 * sleepAdjust + 0.1 * (target - frameTime);
    }

    // Terminate the program if it reaches here
    glfwTerminate();
    g_debugDraw.Destroy();
    delete g_world;

    return 0;
}
