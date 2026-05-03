#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- Hardware Configuration ---
// Uncomment the line below to use SSD1306 (e.g. for Wokwi)
// #define USE_SSD1306 

#ifdef USE_SSD1306
  #include <Adafruit_SSD1306.h>
  #define OLED_ADDRESS 0x3C
  #define COLOR_WHITE SSD1306_WHITE
  #define SDA_PIN 17
  #define SCL_PIN 18
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#else
  #include <Adafruit_SH110X.h>
  #define OLED_ADDRESS 0x3C
  #define COLOR_WHITE SH110X_WHITE
  #define SDA_PIN 21
  #define SCL_PIN 22
  Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
#endif

// --- Constants ---
const float SHIP_SIZE = 4.0f;
const float ROT_SPEED = 0.3f;
const float THRUST = 0.15f;
const float FRICTION = 0.98f;
const float BULLET_SPEED = 2.0f;
const int BULLET_LIFE = 30;
const float ASTEROID_SPEED = 0.5f;
const float DANGER_ZONE = 25.0f;
const float BRAKE_THRESHOLD = 1.0f;

struct Vector2D {
    float x, y;
    Vector2D operator+(const Vector2D& other) const { return {x + other.x, y + other.y}; }
    Vector2D operator-(const Vector2D& other) const { return {x - other.x, y - other.y}; }
    Vector2D operator*(float scalar) const { return {x * scalar, y * scalar}; }
    void operator+=(const Vector2D& other) { x += other.x; y += other.y; }
    float magnitude() const { return sqrt(x * x + y * y); }
    Vector2D normalized() const {
        float mag = magnitude();
        return mag > 0 ? Vector2D{x / mag, y / mag} : Vector2D{0, 0};
    }
};

float dist(Vector2D a, Vector2D b) {
    return (a - b).magnitude();
}

float wrap(float val, float max) {
    if (val < 0) return val + max;
    if (val >= max) return val - max;
    return val;
}

class Bullet {
public:
    Vector2D pos, vel;
    int life;
    bool active;

    Bullet() : active(false) {}
    void spawn(Vector2D p, float angle) {
        pos = p;
        vel = {cos(angle) * BULLET_SPEED, sin(angle) * BULLET_SPEED};
        life = BULLET_LIFE;
        active = true;
    }
    void update() {
        pos.x = wrap(pos.x + vel.x, SCREEN_WIDTH);
        pos.y = wrap(pos.y + vel.y, SCREEN_HEIGHT);
        life--;
        if (life <= 0) active = false;
    }
    void draw() {
        display.drawPixel((int)pos.x, (int)pos.y, COLOR_WHITE);
    }
};

class Asteroid {
public:
    Vector2D pos, vel;
    float r;
    int level;
    bool active;

    Asteroid() : active(false) {}
    void spawn(float x, float y, float radius, int lvl) {
        pos = {x, y};
        r = radius;
        level = lvl;
        float angle = (float)random(0, 360) * 3.14159f / 180.0f;
        float speed = (float)random(5, 15) / 10.0f;
        vel = {cos(angle) * speed, sin(angle) * speed};
        active = true;
    }
    void update() {
        pos.x = wrap(pos.x + vel.x, SCREEN_WIDTH);
        pos.y = wrap(pos.y + vel.y, SCREEN_HEIGHT);
    }
    void draw() {
        display.drawCircle((int)pos.x, (int)pos.y, (int)r, COLOR_WHITE);
    }
};

struct Particle {
    Vector2D pos, vel;
    int life;
    bool active;

    Particle() : active(false) {}
    void spawn(Vector2D p, float speed) {
        pos = p;
        float angle = (float)random(0, 360) * 3.14159f / 180.0f;
        vel = {cos(angle) * speed, sin(angle) * speed};
        life = random(10, 30);
        active = true;
    }
    void update() {
        pos.x += vel.x;
        pos.y += vel.y;
        life--;
        if (life <= 0) active = false;
    }
    void draw() {
        display.drawPixel((int)pos.x, (int)pos.y, COLOR_WHITE);
    }
};

class ParticleSystem {
public:
    Particle particles[40];

    void explode(Vector2D p, int count, float speed) {
        int spawned = 0;
        for (int i = 0; i < 40 && spawned < count; i++) {
            if (!particles[i].active) {
                particles[i].spawn(p, speed);
                spawned++;
            }
        }
    }
    void update() {
        for (int i = 0; i < 40; i++) {
            if (particles[i].active) particles[i].update();
        }
    }
    void draw() {
        for (int i = 0; i < 40; i++) {
            if (particles[i].active) particles[i].draw();
        }
    }
};

class Ship {
public:
    Vector2D pos, vel;
    float angle;
    bool thrusting;
    bool rotatingLeft, rotatingRight;

    Ship() {
        pos = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
        vel = {0, 0};
        angle = -M_PI / 2.0f;
        thrusting = false;
        rotatingLeft = rotatingRight = false;
    }

    void update() {
        if (thrusting) {
            vel.x += cos(angle) * THRUST;
            vel.y += sin(angle) * THRUST;
        }
        if (rotatingLeft) angle -= ROT_SPEED;
        if (rotatingRight) angle += ROT_SPEED;

        vel = vel * FRICTION;
        pos.x = wrap(pos.x + vel.x, SCREEN_WIDTH);
        pos.y = wrap(pos.y + vel.y, SCREEN_HEIGHT);
    }

    void draw() {
        float x1 = pos.x + cos(angle) * SHIP_SIZE * 1.5f;
        float y1 = pos.y + sin(angle) * SHIP_SIZE * 1.5f;
        float x2 = pos.x + cos(angle + 2.5f) * SHIP_SIZE;
        float y2 = pos.y + sin(angle + 2.5f) * SHIP_SIZE;
        float x3 = pos.x + cos(angle - 2.5f) * SHIP_SIZE;
        float y3 = pos.y + sin(angle - 2.5f) * SHIP_SIZE;
        display.drawLine((int)x1, (int)y1, (int)x2, (int)y2, COLOR_WHITE);
        display.drawLine((int)x1, (int)y1, (int)x3, (int)y3, COLOR_WHITE);
    }
};

// --- Game State ---
Ship ship;
ParticleSystem particles;
bool shipAlive = true;
Asteroid asteroids[10];
Bullet bullets[10];
int asteroidCount = 0;
int score = 0;

void spawnAsteroid() {
    if (asteroidCount < 10) {
        asteroids[asteroidCount].spawn(random(0, SCREEN_WIDTH), random(0, SCREEN_HEIGHT), 4.0f, 3);
        asteroidCount++;
    }
}

void setup() {
    Serial.begin(115200);
    
    Wire.begin(SDA_PIN, SCL_PIN);
    
    #ifdef USE_SSD1306
        if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
            Serial.println("SSD1306 allocation failed");
            for (;;);
        }
    #else
        if (!display.begin(OLED_ADDRESS, true)) {
            Serial.println("SH110X allocation failed");
            for (;;);
        }
    #endif

    display.clearDisplay();
    display.display();

    for (int i = 0; i < 5; i++) spawnAsteroid();
}

void loop() {
    // --- AI Logic ---
    float minDist = 1000.0f;
    int targetIdx = -1;
    for (int i = 0; i < asteroidCount; i++) {
        float d = dist(ship.pos, asteroids[i].pos);
        if (d < minDist) {
            minDist = d;
            targetIdx = i;
        }
    }

    // Priority 1: Dodge
    bool dodging = false;
    for (int i = 0; i < asteroidCount; i++) {
        if (dist(ship.pos, asteroids[i].pos) < DANGER_ZONE) {
            Vector2D diff = ship.pos - asteroids[i].pos;
            float escapeAngle = atan2(diff.y, diff.x);
            
            float angleDiff = escapeAngle - ship.angle;
            while (angleDiff < -M_PI) angleDiff += 2 * M_PI;
            while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
            
            ship.rotatingLeft = (angleDiff < -0.1f);
            ship.rotatingRight = (angleDiff > 0.1f);
            ship.thrusting = true;
            dodging = true;
            break;
        }
    }

    if (!dodging) {
        // Priority 2: Brake
        if (ship.vel.magnitude() > BRAKE_THRESHOLD) {
            float brakeAngle = atan2(-ship.vel.y, -ship.vel.x);
            float angleDiff = brakeAngle - ship.angle;
            while (angleDiff < -M_PI) angleDiff += 2 * M_PI;
            while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
            
            ship.rotatingLeft = (angleDiff < -0.1f);
            ship.rotatingRight = (angleDiff > 0.1f);
            ship.thrusting = true;
        } else if (targetIdx != -1) {
            // Priority 3: Target and Shoot
            float targetAngle = atan2(asteroids[targetIdx].pos.y - ship.pos.y, asteroids[targetIdx].pos.x - ship.pos.x);
            float angleDiff = targetAngle - ship.angle;
            while (angleDiff < -M_PI) angleDiff += 2 * M_PI;
            while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
            
            ship.rotatingLeft = (angleDiff < -0.1f);
            ship.rotatingRight = (angleDiff > 0.1f);
            ship.thrusting = false;

            if (abs(angleDiff) < 0.15f) {
                for (int i = 0; i < 10; i++) {
                    if (!bullets[i].active) {
                        bullets[i].spawn(ship.pos, ship.angle);
                        break;
                    }
                }
            }
        }
    }

    // --- Physics ---
    ship.update();
    for (int i = 0; i < 10; i++) {
        if (bullets[i].active) bullets[i].update();
    }
    for (int i = 0; i < asteroidCount; i++) {
        asteroids[i].update();
    }

    // --- Collisions ---
    if (shipAlive) {
        // Ship vs Asteroid
        for (int i = 0; i < asteroidCount; i++) {
            if (dist(ship.pos, asteroids[i].pos) < asteroids[i].r + SHIP_SIZE) {
                particles.explode(ship.pos, 20, 1.5f);
                shipAlive = false;
                break;
            }
        }

        // Bullet vs Asteroid
        for (int i = 0; i < asteroidCount; i++) {
            for (int j = 0; j < 10; j++) {
                if (bullets[j].active && dist(bullets[j].pos, asteroids[i].pos) < asteroids[i].r) {
                    bullets[j].active = false;
                    score += 10;
                    particles.explode(asteroids[i].pos, 10, 0.8f);
                    // Simple respawn logic: remove current and spawn new
                    for(int k = i; k < asteroidCount - 1; k++) {
                        asteroids[k] = asteroids[k+1];
                    }
                    asteroidCount--;
                    spawnAsteroid();
                    i--; // Re-check this index since we shifted elements
                    break;
                }
            }
        }
    } else {
        // Respawn ship after some time (simulated by checking if particles are still active or just a simple timer)
        // For this sim, we'll just respawn after a short delay
        static int respawnTimer = 0;
        if (++respawnTimer > 60) {
            ship.pos = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
            ship.vel = {0, 0};
            score = 0;
            shipAlive = true;
            respawnTimer = 0;
        }
    }

    // --- Render ---
    display.clearDisplay();
    if (shipAlive) {
        ship.draw();
    }
    
    particles.update();
    particles.draw();
    
    for (int i = 0; i < 10; i++) if (bullets[i].active) bullets[i].draw();
    for (int i = 0; i < asteroidCount; i++) asteroids[i].draw();
    
    display.setTextSize(1);
    display.setTextColor(COLOR_WHITE);
    display.setCursor(0, 0);
    display.print("Score: "); display.print(score);
    
    display.display();
    delay(20);
}