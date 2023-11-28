
#include <stdio.h>
#include <chrono>
#include <thread>
#include <sstream>

#include "imgui/imgui.h"
#include "imgui_impl_glfw_game.h"
#include "imgui_impl_opengl3_game.h"
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "draw_game.h"
#include <iostream>
#include <random>
#include <cmath>
#include <unordered_set>
#include "box2d/box2d.h"

class GameObject;
class Projectile;
class Asteroid;
class Heroship;
class Saucer;

class MyContactListener;

float GenerateRandom(float lower, float upper);
float GenerateRandomDirection();
void ClearWorld();
void CreateWorldStart();
void UpdateTextDisplay();
void UpdateGameObjects();
void UpdateGameState();
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void MouseMotionCallback(GLFWwindow*, double xd, double yd);
void MouseButtonCallback(GLFWwindow* window, int32 button, int32 action, int32 mods);

GLFWwindow* g_mainWindow;
b2World* g_world;
std::unordered_set<GameObject*> g_gameObjects; // Keep a unique set of game objects to parallel the world bodies
Heroship* g_heroShip;  // It's easier to call OnKeyPress() on the hero ship.
int g_score = 0;
int g_high_score = 0;
int g_lives = 3;  // Three lives to begin with. This can go up or down!
int g_large_asteroid_count = 4; // This will increase by one asteroid every level.
int g_wait_counter = 0;  // Let the game run but wait until next phase to do something.
int g_saucer_timer = 0;  // Mainly to spawn saucers to harass the hero ship.
bool g_gameOver = false;
bool g_lifeLost = false;
bool g_board_won = false;
const float PIXELS_PER_UNIT = 20.0f;
const float TIME_STEP = 60 > 0.0f ? 1.0f / 60 : float(0.0f);

enum Size
{
    Small = 1,
    Medium = 2,
    Large = 3
};

class GameObject
{
public:
    ~GameObject()
    {
        g_world->DestroyBody(body); // This is where the body is destroyed in the physics engine
    }

    virtual void Update(){}; // Update game object every frame

    virtual void OnDestruction(){}; // Just before the game object is destroyed, this happens.

    void UpdateScoreAndLives(int scoreToAdd)
    {
        if (!willScorePoints)
            return;

        int oldScore = g_score;
        g_score += scoreToAdd;
        if (oldScore / 10000 < g_score / 10000)
            ++g_lives; // New life added every 10000 points.
    }

    void WorldWrapAround()
    {
        b2Vec2 pos = body->GetPosition();
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
            wrapped = true;
            pos.y = 0;
        }
        else if (pos.y < 0)
        {
            pos.y = g_camera.m_height / PIXELS_PER_UNIT;
            wrapped = true;
        }

        // Adjust position
        if (wrapped)
        {
            body->SetTransform(pos, body->GetAngle());
        }
    }

    b2Body* body;
    char* gameObjectType = "Unknown";
    bool willDestruct = false;
    bool willScorePoints = false;

protected:

    // The type of game entity.
    //
    const uint16 CATEGORY_SPACESHIP = 0x0001;
    const uint16 CATEGORY_SHIP_PROJECTILE = 0x0002;
    const uint16 CATEGORY_LARGE_ASTEROID = 0x0004;
    const uint16 CATEGORY_MEDIUM_ASTEROID = 0x0008;
    const uint16 CATEGORY_SMALL_ASTEROID = 0x0010;
    const uint16 CATEGORY_DESTROYED_SHIP = 0x0020;
    const uint16 CATEGORY_LARGE_SAUCER = 0x0040;
    const uint16 CATEGORY_SMALL_SAUCER = 0x0080;
    const uint16 CATEGORY_SAUCER_PROJECTILE = 0x0100;

    const uint16 CATEGORY_ASTEROID = CATEGORY_LARGE_ASTEROID |
                                     CATEGORY_MEDIUM_ASTEROID |
                                     CATEGORY_SMALL_ASTEROID;

    const uint16 CATEGORY_SAUCER = CATEGORY_LARGE_SAUCER |
                                   CATEGORY_SMALL_SAUCER |
                                   CATEGORY_SAUCER_PROJECTILE;

    const uint16 CATEGORY_HEROSHIP = CATEGORY_SPACESHIP |
                                     CATEGORY_SHIP_PROJECTILE;

    // The masks determine which objects may collide and be destroyed with what.
    // Generally, game objects collide with anything not in their own category,
    // for example, asteroids don't collide with each other, but everything else
    // collides with asteroids.
    //
    const uint16 MASK_HEROSHIP = CATEGORY_ASTEROID | CATEGORY_SAUCER;
    const uint16 MASK_ASTEROID = CATEGORY_HEROSHIP | CATEGORY_SAUCER;
    const uint16 MASK_SAUCER = CATEGORY_HEROSHIP | CATEGORY_ASTEROID;
};

class Projectile : public GameObject
{
public:
    Projectile(b2Vec2 position, b2Vec2 velocity)
    {
        gameObjectType = "Hero Projectile";

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = position;
        bodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&bodyDef);

        b2CircleShape shape;
        shape.m_radius = 0.1f;

        b2FixtureDef projectileFixture;
        projectileFixture.shape = &shape;
        projectileFixture.density = 1.0f;
        projectileFixture.filter.categoryBits = CATEGORY_SHIP_PROJECTILE;
        projectileFixture.filter.maskBits = MASK_HEROSHIP;
        body->CreateFixture(&projectileFixture);
        body->SetLinearVelocity(velocity);
        body->SetBullet(true);
    }

    ~Projectile() {}

    void Update() override
    {
        b2Vec2 velocity = body->GetLinearVelocity();
        float traveledThisFrame = velocity.Length() * TIME_STEP; // TIME_STEP is the duration of a frame
        distanceTraveled += traveledThisFrame;

        if (distanceTraveled > maxRange)
        {
            this->willDestruct = true;
        }

        WorldWrapAround();
    }

    void OnDestruction() override
    {
        // Nothing to do here. The projectile will simply be destroyed.
    }

private:
    float distanceTraveled = 0.0f;
    const float maxRange = 25.0f;
};

class Asteroid : public GameObject
{
public:
    Asteroid()
    {
        gameObjectType = "Large Asteroid";

        b2BodyDef asteroidBodyDef;
        asteroidBodyDef.type = b2_dynamicBody;
        b2Vec2 randomPos = GenerateRandomAsteroidPos();
        asteroidBodyDef.position.Set(randomPos.x, randomPos.y);
        asteroidBodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&asteroidBodyDef);

        b2PolygonShape asteroid_shape = GetRandomAsteroidShape(sizeOfAsteroid);

        b2FixtureDef asteroidFixture;
        asteroidFixture.shape = &asteroid_shape;
        asteroidFixture.density = 1.0f;
        asteroidFixture.filter.categoryBits = CATEGORY_LARGE_ASTEROID;
        asteroidFixture.filter.maskBits = MASK_ASTEROID;
        body->CreateFixture(&asteroidFixture);

        // Starting asteroids have a random direction of motion
        //
        float desiredVelocity = 4.0f; // Adjust as needed
        float angle = GenerateRandomDirection(); // Radians are based on 2*pi
        b2Vec2 directionOfMotion(cos(angle), sin(angle));
        directionOfMotion *= desiredVelocity;
        body->SetLinearVelocity(directionOfMotion);
    }

    Asteroid(b2Vec2 pos, Size size)
    {
        sizeOfAsteroid = size;
        b2BodyDef asteroidBodyDef;
        asteroidBodyDef.type = b2_dynamicBody;
        float x = pos.x + GenerateRandom(-1.0f, 1.0f);
        float y = pos.y + GenerateRandom(-1.0f, 1.0f);
        asteroidBodyDef.position.Set(x, y);
        asteroidBodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&asteroidBodyDef);

        b2PolygonShape asteroid_shape = GetRandomAsteroidShape(sizeOfAsteroid);

        b2FixtureDef asteroidFixture;
        asteroidFixture.shape = &asteroid_shape;
        asteroidFixture.density = 1.0f;
        float desiredVelocity = 5.0f;
        if (sizeOfAsteroid == Size::Medium)
        {
            asteroidFixture.filter.categoryBits = CATEGORY_MEDIUM_ASTEROID;
            gameObjectType = "Medium Asteroid";
        }
        else if (sizeOfAsteroid == Size::Small)
        {
            asteroidFixture.filter.categoryBits = CATEGORY_SMALL_ASTEROID;
            desiredVelocity = 6.0f;
            gameObjectType = "Small Asteroid";
        }
        asteroidFixture.filter.maskBits = MASK_ASTEROID;
        body->CreateFixture(&asteroidFixture);

        // Starting asteroids have a random direction of motion
        //
        float angle = GenerateRandomDirection(); // Radians are based on 2*pi
        b2Vec2 directionOfMotion(cos(angle), sin(angle));
        directionOfMotion *= desiredVelocity;
        body->SetLinearVelocity(directionOfMotion);
    }

    virtual ~Asteroid() {}

    void Update() override
    {
        WorldWrapAround();
    }

    void OnDestruction() override
    {
        if (sizeOfAsteroid == Size::Large)
        {
            b2Vec2 pos = body->GetPosition();
            UpdateScoreAndLives(20);
            // When a large asteroid is destroyed, two medium asteroids are created.
            g_gameObjects.emplace(new Asteroid(pos, Size::Medium));
            g_gameObjects.emplace(new Asteroid(pos, Size::Medium));
        }
        if (sizeOfAsteroid == Size::Medium)
        {
            b2Vec2 pos = body->GetPosition();
            UpdateScoreAndLives(50);
            // When a medium asteroid is destroyed, two small asteroids are created.
            g_gameObjects.emplace(new Asteroid(pos, Size::Small));
            g_gameObjects.emplace(new Asteroid(pos, Size::Small));
        }
        if (sizeOfAsteroid == Size::Small)
        {
            UpdateScoreAndLives(100);
            // When a small asteroid is destroyed, no new asteroids are created.
        }
    }

private:
    Size sizeOfAsteroid = Size::Large;

    b2Vec2 GenerateRandomAsteroidPos()
    {
        // Window width and height in Box2D terms, estimated.
        float width = 70.0f;
        float height = 50.0f;

        // Make asteroids appear near the edge of the screen.
        //
        float large = GenerateRandom(0.0f, height);
        float small = GenerateRandom(0.0f, 10.0f);
        int side = static_cast<int>(std::ceil(GenerateRandom(0.0f, 4.0f)));

        b2Vec2 vec;

        switch (side)
        {
        case 1:
            // Translate random position to correct coordinates
            vec.Set(large - height / 2, small);            break;
        case 2:
            // Translate random position to correct coordinates
            vec.Set(small - width / 2, large);
            break;
        case 3:
            // Translate random position to correct coordinates
            vec.Set(large - height / 2, height - small);
            break;
        case 4:
            // Translate random position to correct coordinates
            vec.Set(width / 2 - small, large);
            break;
        default:
            vec.Set(0.0f, 0.0f);
            break;
        }

        return vec;
    }

    b2PolygonShape GetRandomAsteroidShape(Size size)
    {
        // Unlike the original game, the asteroids will only have convex shapes in order to
        // simplify collision detection.
        //
        float factor = 3.0f;
        if (size == Size::Medium)
            factor = 2.0f;
        if (size == Size::Small)
            factor = 1.0f;
        float low = 0.5f * factor;
        float high = 1.5f * factor;
        b2PolygonShape asteroid_shape;
        b2Vec2 vertices[8];
        vertices[0].Set(GenerateRandom(low, high), GenerateRandom(low, high));
        vertices[1].Set(GenerateRandom(low, high), 0.0f);
        vertices[2].Set(GenerateRandom(low, high), -GenerateRandom(low, high));
        vertices[3].Set(0.0f, -GenerateRandom(low, high));
        vertices[4].Set(-GenerateRandom(low, high), -GenerateRandom(low, high));
        vertices[5].Set(-GenerateRandom(low, high), 0.0f);
        vertices[6].Set(-GenerateRandom(low, high), GenerateRandom(low, high));
        vertices[7].Set(0.0f, GenerateRandom(low, high));
        asteroid_shape.Set(vertices, 8);

        return asteroid_shape;
    }
};

class Heroship : public GameObject
{
public:
    Heroship()
    {
        gameObjectType = "Heroship";
        g_lifeLost = false;

        b2BodyDef shipBodyDef;
        shipBodyDef.type = b2_dynamicBody; // the ship is a movable object
        shipBodyDef.position.Set(0.0f, 20.0f); // starting position is roughly in the center of the screen
        shipBodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&shipBodyDef);

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
        shipFixture.filter.maskBits = MASK_HEROSHIP;
        body->CreateFixture(&shipFixture);
        body->SetLinearDamping(0.5f); // Creates "drag" for the ship
        body->SetBullet(true);
    }

    virtual ~Heroship()
    {
    }

    void OnKeyPress(int key, int action)
    {
        // Left, right keys to rotate spaceship, up arrow for thrust, space bar to fire projectiles,
        // Either Ctrl key for emergency teleport.
        //
        switch (key)
        {
        case GLFW_KEY_LEFT:
            if (action == GLFW_PRESS)
                rotateLeft = true;
            else if (action == GLFW_RELEASE)
                rotateLeft = false;
            break;

        case GLFW_KEY_RIGHT:
            if (action == GLFW_PRESS)
                rotateRight = true;
            else if (action == GLFW_RELEASE)
                rotateRight = false;
            break;

        case GLFW_KEY_UP:
            if (action == GLFW_PRESS)
                accelerate = true;
            else if (action == GLFW_RELEASE)
                accelerate = false;
            break;

        case GLFW_KEY_SPACE:
            if (action == GLFW_PRESS)
                fireProjectile = true;
            break;

        case GLFW_KEY_RIGHT_CONTROL:
        case GLFW_KEY_LEFT_CONTROL:
            if (action == GLFW_PRESS)
                teleport = true;
            break;

        default:
            break;
        }
    }

    void Update() override
    {
        if (rotateLeft)
            RotateShip(0.1f);
        if (rotateRight)
            RotateShip(-0.1f);
        if (accelerate)
            AccelerateSpaceship();
        if (fireProjectile)
            FireProjectile();
        if (teleport)
            RandomTeleport();

        WorldWrapAround();
    }

    void OnDestruction() override
    {
        --g_lives;
        g_lifeLost = true;
    }

private:
    void RotateShip(float amount)
    {
        float adjustedAngle = body->GetAngle() + amount;
        body->SetTransform(body->GetPosition(), adjustedAngle);
    }

    void AccelerateSpaceship()
    {
    // Calculate the force of accelation with a diminishing return. The faster
    // the ship goes, the less able the ship's propulsion is to accelerate the
    // ship. This is to prevent unlimited acceleration in the game.
    //
    float currentVelocityMagnitude = body->GetLinearVelocity().Length();
    float maxVelocity = 50.0f; // Define a maximum reasonable velocity for your spaceship.
    float adjustmentFactor = 1.0f - (currentVelocityMagnitude / maxVelocity);
    float baseMagnitude = 300.0f; // Define this value based on desired acceleration.
    float adjustedMagnitude = baseMagnitude * adjustmentFactor;

    // Calculate the direction of acceleration.
    //
    float angle = body->GetAngle();
    angle += b2_pi / 2; // Adjust angle by 90 degrees
    b2Vec2 forceDirection(cos(angle), sin(angle));

    // Apply the magnitude of acceleration to the direction of acceleration.
    //
    forceDirection *= adjustedMagnitude;
    body->ApplyForceToCenter(forceDirection, true);
}

    void FireProjectile()
{
    fireProjectile = false;
    float angle = body->GetAngle();
    angle += b2_pi / 2; // Adjust angle by 90 degrees for proper direction of fire.
    b2Vec2 directionOfFire(cos(angle), sin(angle));
    b2Vec2 launchPosition = body->GetPosition() + directionOfFire;
    float projectileSpeed = 40.0f; // Adjust as needed
    directionOfFire *= projectileSpeed;

    // Create the projectile
    g_gameObjects.emplace(new Projectile(launchPosition, directionOfFire));
}

    void RandomTeleport()
{
    teleport = false;
    float x = GenerateRandom(-35.0f, 35.0f);
    float y = GenerateRandom(0.0f, 50.0f);
    b2Vec2 newPosition(x, y);
    body->SetTransform(newPosition, body->GetAngle());
}

    // We can't rely on the GLFW_REPEAT key event to handle smooth action when
    // holding a key down. The flags make for a better user experience.
    //
    bool rotateLeft = false;
    bool rotateRight = false;
    bool accelerate = false;
    bool teleport = false;
    bool fireProjectile = false;
};

class SaucerProjectile : public GameObject
{
public:
    SaucerProjectile(b2Vec2 position, b2Vec2 velocity)
    {
        gameObjectType = "Saucer Projectile";

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = position;
        bodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&bodyDef);

        b2CircleShape shape;
        shape.m_radius = 0.1f;

        b2FixtureDef projectileFixture;
        projectileFixture.shape = &shape;
        projectileFixture.density = 1.0f;
        projectileFixture.filter.categoryBits = CATEGORY_SAUCER_PROJECTILE;
        projectileFixture.filter.maskBits = MASK_SAUCER;
        body->CreateFixture(&projectileFixture);
        body->SetLinearVelocity(velocity);
        body->SetBullet(true);
    }

    ~SaucerProjectile() {}

    void Update() override
    {
        b2Vec2 velocity = body->GetLinearVelocity();
        float traveledThisFrame = velocity.Length() * TIME_STEP; // TIME_STEP is the duration of a frame
        distanceTraveled += traveledThisFrame;

        if (distanceTraveled > maxRange)
        {
            this->willDestruct = true;
        }

        WorldWrapAround();
    }

    void OnDestruction() override
    {
        // Nothing to do here. The projectile will simply be destroyed.
    }

private:
    float distanceTraveled = 0.0f;
    const float maxRange = 30.0f;
};

class Saucer : public GameObject
{
public:
    Saucer(Size size)
    {
        b2BodyDef shipBodyDef;
        shipBodyDef.type = b2_dynamicBody; // the ship is a movable object
        float x = 0.0f;
        if (size == Size::Medium)
        {
            sizeOfSaucer = Size::Medium;
            x = -1 * ((g_camera.m_width / PIXELS_PER_UNIT) / 2.0f);
        }
        else if (size == Size::Small)
        {
            sizeOfSaucer = Size::Small;
            x = (g_camera.m_width / PIXELS_PER_UNIT) / 2.0f;
        }
        shipBodyDef.position.Set(x, 20.0f);
        shipBodyDef.userData.pointer = (uintptr_t)this;
        body = g_world->CreateBody(&shipBodyDef);

        b2PolygonShape shipShape;
        b2Vec2 vertices[7] = {};
        vertices[0].Set(1.0f * size, 0.0f);
        vertices[1].Set(0.5f * size, -0.5f * size);
        vertices[2].Set(-0.5f * size, -0.5f * size);
        vertices[3].Set(-1.0f * size, 0.0f);
        vertices[4].Set(-0.5f * size, 0.5f * size);
        vertices[5].Set(0.5f * size, 0.5f * size);
        vertices[6].Set(1.0f * size, 0.0f);
        shipShape.Set(vertices, 7);

        b2FixtureDef shipFixture;
        shipFixture.shape = &shipShape;
        shipFixture.density = 1.0f; // determines the mass of the ship
        if (sizeOfSaucer == Size::Medium)
        {
                shipFixture.filter.categoryBits = CATEGORY_LARGE_SAUCER;
                gameObjectType = "Large Saucer";
        }
        if (sizeOfSaucer == Size::Small)
        {
                shipFixture.filter.categoryBits = CATEGORY_SMALL_SAUCER;
                gameObjectType = "Small Saucer";
        }
        shipFixture.filter.maskBits = MASK_SAUCER;
        body->CreateFixture(&shipFixture);
    }

    virtual ~Saucer() {}

    void RandomSaucerDirection()
    {
        float desiredVelocity = 0.0f;
        if (sizeOfSaucer == Size::Medium)
        {
            desiredVelocity = 8.0f; // Adjust as needed
        }
        else
        {
            desiredVelocity = 12.0f; // Adjust as needed
        }
        float angle = 0.0f;
        if (sizeOfSaucer == Size::Medium)
        {
            angle = GenerateRandom(0, b2_pi) - (0.5f * b2_pi); // Towards right of screen
        }
        else if (sizeOfSaucer == Size::Small)
        {
            angle = GenerateRandom(0, b2_pi) + (0.5f * b2_pi); // Towards left of screen
        }
        b2Vec2 directionOfMotion(cos(angle), sin(angle));
        directionOfMotion *= desiredVelocity;
        body->SetLinearVelocity(directionOfMotion);
    }

    void FireSaucerProjectile()
    {
        float angle = GenerateRandomDirection();
        b2Vec2 directionOfFire(cos(angle), sin(angle));
        b2Vec2 launchPosition = body->GetPosition() + directionOfFire;
        float projectileSpeed = 40.0f;
        directionOfFire *= projectileSpeed;

        // Create the projectile
        g_gameObjects.emplace(new SaucerProjectile(launchPosition, directionOfFire));
    }

    void Update() override
    {
        if (g_saucer_timer % 100 == 0)
        {
            RandomSaucerDirection();
        }

        b2Vec2 pos = body->GetPosition();
        if (sizeOfSaucer == Size::Medium &&
            pos.x > (g_camera.m_width / PIXELS_PER_UNIT) / 2.0f)
        {
            this->willDestruct = true;
        }
        else if (sizeOfSaucer == Size::Small &&
                 pos.x < -1 * ((g_camera.m_width / PIXELS_PER_UNIT) / 2.0f))
        {
            this->willDestruct = true;
        }

        WorldWrapAround();

        if (g_saucer_timer % 30 == 0)
        {
            FireSaucerProjectile();
        }
    }

    void OnDestruction() override
    {
        if (sizeOfSaucer == Size::Medium)
        {
            UpdateScoreAndLives(200);        }
        if (sizeOfSaucer == Size::Small)
        {
            UpdateScoreAndLives(1000);
        }
    }

private:
    Size sizeOfSaucer = Size::Large;
};

void ClearWorld()
{
    g_heroShip = nullptr;
    std::vector<GameObject*> toDestroy;

    for (GameObject* obj : g_gameObjects)
    {
        toDestroy.push_back(obj);
    }

    for (GameObject* objToDestroy : toDestroy)
    {
        g_gameObjects.erase(objToDestroy);
        delete objToDestroy;
    }
}

void ClearWorldExceptHero()
{
    std::vector<GameObject*> toDestroy;

    for (GameObject* obj : g_gameObjects)
    {
        if (strcmp(obj->gameObjectType, "Heroship") == 0)
            continue;

        toDestroy.push_back(obj);
    }

    for (GameObject* objToDestroy : toDestroy)
    {
        g_gameObjects.erase(objToDestroy);
        delete objToDestroy;
    }
}

void CreateWorldStart()
{
    if (g_heroShip == nullptr)
    {
        g_heroShip = new Heroship();
        g_gameObjects.emplace(g_heroShip);
    }
    for (int i = 0; i < g_large_asteroid_count; i++)
    {
        g_gameObjects.emplace(new Asteroid());
    }
    g_saucer_timer = 0;
}

void UpdateGameObjects()
{
    std::vector<GameObject*> toDestroy;

    for (GameObject* gameObject : g_gameObjects)
    {
        if (gameObject == nullptr)
            continue;

        gameObject->Update();

        if (gameObject->willDestruct)
        {
            toDestroy.push_back(gameObject);
        }
    }

    for (GameObject* objToDestroy : toDestroy)
    {
        objToDestroy->OnDestruction();
        g_gameObjects.erase(objToDestroy);
        delete objToDestroy;
    }
}

void CheckCreateSaucer()
{
    if (g_gameOver)
        return;

    g_saucer_timer++;
    if (g_saucer_timer % 1200 == 0)
    {
        if (GenerateRandom(0, 1) > 0.45f)
        {
            g_gameObjects.emplace(new Saucer(Size::Medium));
        }
        else
        {
            g_gameObjects.emplace(new Saucer(Size::Small));
        }
    }
}

bool IsOnlyHeroShipLeft()
{
    if (g_world->GetBodyCount() != 1)
        return false;

    auto body = g_world->GetBodyList();
    auto obj = (GameObject*)body->GetUserData().pointer;
    if (strcmp(obj->gameObjectType, "Heroship") == 0)
        return true;

    return false;
}

void CheckForTransition()
{
    if (g_lifeLost)
    {
        g_lifeLost = false;
        if (g_lives < 1)
        {
            // If a life was lost and one has zero lives left, it's GAME OVER!
            g_gameOver = true;
            g_wait_counter = 180;
        }
        else
        {
            // If a life was lost and the player still has lives left, the next spaceship
            // is activated. There are still asteroids in the world.
            g_wait_counter = 60; // Wait one second before enabling the next life.
        }
    }
    else if (IsOnlyHeroShipLeft() && !g_board_won)
    {
        g_board_won = true;
        g_wait_counter = 120;
    }
}

void RegenerateWorld()
{
    // If it is time for the regeneration of the game objects,
    // clear the world and create them.
    if (g_wait_counter-- == 1) // This means that it had been set and has just run out.
    {
        if (g_gameOver)
        {
            g_gameOver = false;
            if (g_score > g_high_score)
            {
                g_high_score = g_score;
            }
            g_score = 0;
            g_lives = 3;
            g_large_asteroid_count = 4;
            ClearWorld();
            CreateWorldStart();
        }
        else if (g_board_won)
        {
            g_board_won = false;
            g_large_asteroid_count++;
            ClearWorldExceptHero();
            CreateWorldStart();
        }
        else // The level was not yet complete. Need a new Spaceship for our hero!
        {
            g_heroShip = new Heroship();
            g_gameObjects.emplace(g_heroShip);
        }
    }
}

void UpdateGameState()
{
    CheckCreateSaucer(); // If not game over, check if it is time for a saucer attack!
    CheckForTransition(); // Sets a wait counter for world regeneration
    RegenerateWorld(); // Take care of the regeneration
}

//
// Entity collision detection.
//
class MyContactListener : public b2ContactListener
{
    void BeginContact(b2Contact* contact) override
    {
        // All collisions lead to the destruction of both game objects involved.
        //
        auto a = (GameObject*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
        a->willDestruct = true;

        auto b = (GameObject*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;
        b->willDestruct = true;

        if (strcmp(a->gameObjectType, "Heroship") == 0 ||
            strcmp(a->gameObjectType, "Hero Projectile") == 0)
        {
            b->willScorePoints = true;
        }
        else if (strcmp(b->gameObjectType, "Heroship") == 0 ||
                 strcmp(b->gameObjectType, "Hero Projectile") == 0)
        {
            a->willScorePoints = true;
        }
    }
};

//
// Setup and main game loop.
//
int main()
{
    // glfw initialization things
    if (glfwInit() == 0)
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    g_mainWindow = glfwCreateWindow(g_camera.m_width, g_camera.m_height,
        "ASTEROIDS: Using Box2D and written in C++ by Mark Sulkowski",
        NULL, NULL);

    if (g_mainWindow == NULL)
    {
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

    CreateWorldStart();

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
    while (!glfwWindowShouldClose(g_mainWindow))
    {
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

        // Enable objects to be drawn
        uint32 flags = 0;
        flags += b2Draw::e_shapeBit;
        g_debugDraw.SetFlags(flags);

        // When we call Step(), we run the simulation for one frame
        g_world->Step(TIME_STEP, 8, 3);

        UpdateTextDisplay();

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

        UpdateGameObjects();
        UpdateGameState();

        // Throttle to cap at 60 FPS. Which means if it's going to be past
        // 60FPS, sleeps a while instead of doing more frames.
        //
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        std::chrono::duration<double> target(1.0 / 60.0);
        std::chrono::duration<double> timeUsed = t2 - t1;
        std::chrono::duration<double> sleepTime = target - timeUsed + sleepAdjust;
        if (sleepTime > std::chrono::duration<double>(0))
        {
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

float GenerateRandom(float lower, float upper)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(lower, upper);
    float random_value = dis(gen);  // The loss of data from double to float is acceptable.
    return random_value;
}

float GenerateRandomDirection()
{
    return GenerateRandom(0, 2 * b2_pi);  // Random direction in radians. 0 to 2*pi.
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

//
// Controller functions
//

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (g_heroShip == nullptr)
        return;

    // Set flags on the hero ship game object based on key presses.
    // Update() takes care of any changes to game behavior.
    //
    g_heroShip->OnKeyPress(key, action);
}

void UpdateTextDisplay()
{
    std::string displayText = "Score: " + std::to_string(g_score);
    displayText += "\nHigh Score: " + std::to_string(g_high_score);
    if (g_lives > 0)
    {
        displayText += "\nLives: " + std::to_string(g_lives);
    }
    else
    {
        displayText += "\nGAME OVER!";
    }
    displayText += "\n\nShoot: Space\nTurn: Left/Right\nThrust: Up\nTeleport: Ctrl";
    g_debugDraw.DrawString(15, 15, displayText.c_str());
}
