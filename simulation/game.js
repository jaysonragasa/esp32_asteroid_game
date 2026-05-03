const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d');
const aiActionText = document.getElementById('ai-action');
const scoreText = document.getElementById('score');

canvas.width = window.innerWidth;
canvas.height = window.innerHeight;

window.addEventListener('resize', () => {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
});

// --- Constants ---
const SHIP_SIZE = 15;
const ROT_SPEED = 0.1;
const THRUST = 0.1;
const FRICTION = 0.99;
const BULLET_SPEED = 7;
const BULLET_LIFE = 60;
const ASTEROID_SPEED = 1.5;
const DANGER_ZONE = 150;
const BRAKE_THRESHOLD = 3;

// --- Helper Functions ---
function dist(x1, y1, x2, y2) {
    return Math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2);
}

function wrap(val, max) {
    return (val + max) % max;
}

// --- Game Objects ---
class Ship {
    constructor() {
        this.x = canvas.width / 2;
        this.y = canvas.height / 2;
        this.r = SHIP_SIZE;
        this.angle = -Math.PI / 2;
        this.vx = 0;
        this.vy = 0;
        this.thrusting = false;
        this.rotatingLeft = false;
        this.rotatingRight = false;
    }

    update() {
        if (this.thrusting) {
            this.vx += Math.cos(this.angle) * THRUST;
            this.vy += Math.sin(this.angle) * THRUST;
        }

        if (this.rotatingLeft) this.angle -= ROT_SPEED;
        if (this.rotatingRight) this.angle += ROT_SPEED;

        this.vx *= FRICTION;
        this.vy *= FRICTION;
        this.x += this.vx;
        this.y += this.vy;

        this.x = wrap(this.x, canvas.width);
        this.y = wrap(this.y, canvas.height);
    }

    draw() {
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(
            this.x + 4/3 * this.r * Math.cos(this.angle),
            this.y + 4/3 * this.r * Math.sin(this.angle)
        );
        ctx.lineTo(
            this.x - this.r * (2/3 * Math.cos(this.angle) + Math.sin(this.angle)),
            this.y - this.r * (2/3 * Math.sin(this.angle) - Math.cos(this.angle))
        );
        ctx.lineTo(
            this.x - this.r * (2/3 * Math.cos(this.angle) - Math.sin(this.angle)),
            this.y - this.r * (2/3 * Math.sin(this.angle) + Math.cos(this.angle))
        );
        ctx.closePath();
        ctx.stroke();

        if (this.thrusting) {
            ctx.fillStyle = '#f00';
            ctx.beginPath();
            ctx.moveTo(this.x - this.r * Math.cos(this.angle), this.y - this.r * Math.sin(this.angle));
            ctx.lineTo(this.x - this.r * 1.5 * Math.cos(this.angle), this.y - this.r * 1.5 * Math.sin(this.angle));
            ctx.stroke();
        }
    }
}

class Asteroid {
    constructor(x, y, r, level = 3) {
        this.x = x || Math.random() * canvas.width;
        this.y = y || Math.random() * canvas.height;
        this.r = r || 40;
        this.level = level;
        const speed = Math.random() * ASTEROID_SPEED + 0.5;
        const angle = Math.random() * Math.PI * 2;
        this.vx = Math.cos(angle) * speed;
        this.vy = Math.sin(angle) * speed;
    }

    update() {
        this.x += this.vx;
        this.y += this.vy;
        this.x = wrap(this.x, canvas.width);
        this.y = wrap(this.y, canvas.height);
    }

    draw() {
        ctx.strokeStyle = '#aaa';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.r, 0, Math.PI * 2);
        ctx.stroke();
    }
}

class Bullet {
    constructor(x, y, angle) {
        this.x = x;
        this.y = y;
        this.vx = Math.cos(angle) * BULLET_SPEED;
        this.vy = Math.sin(angle) * BULLET_SPEED;
        this.life = BULLET_LIFE;
    }

    update() {
        this.x += this.vx;
        this.y += this.vy;
        this.x = wrap(this.x, canvas.width);
        this.y = wrap(this.y, canvas.height);
        this.life--;
    }

    draw() {
        ctx.fillStyle = '#ff0';
        ctx.beginPath();
        ctx.arc(this.x, this.y, 2, 0, Math.PI * 2);
        ctx.fill();
    }
}

// --- AI Controller ---
class AIController {
    constructor(ship) {
        this.ship = ship;
    }

    update(asteroids) {
        if (asteroids.length === 0) {
            aiActionText.innerText = "Searching for targets...";
            this.stopAll();
            return;
        }

        // 1. Find nearest asteroid
        let nearest = null;
        let minDist = Infinity;
        for (let a of asteroids) {
            let d = dist(this.ship.x, this.ship.y, a.x, a.y);
            if (d < minDist) {
                minDist = d;
                nearest = a;
            }
        }

        // 2. DODGE: If something is way too close, priority 1 is to move away
        for (let a of asteroids) {
            if (dist(this.ship.x, this.ship.y, a.x, a.y) < DANGER_ZONE) {
                aiActionText.innerText = "DODGING!";
                const angleToAsteroid = Math.atan2(a.y - this.ship.y, a.x - this.ship.x);
                const escapeAngle = angleToAsteroid + Math.PI; // Move in opposite direction
                this.faceAngle(escapeAngle);
                this.ship.thrusting = true;
                return;
            }
        }

        // 3. BRAKE: If moving too fast, counter-act momentum
        const speed = Math.sqrt(this.ship.vx**2 + this.ship.vy**2);
        if (speed > BRAKE_THRESHOLD) {
            aiActionText.innerText = "Braking...";
            const brakeAngle = Math.atan2(-this.ship.vy, -this.ship.vx);
            this.faceAngle(brakeAngle);
            this.ship.thrusting = true;
            return;
        }

        // 4. TARGET & SHOOT
        aiActionText.innerText = "Targeting Asteroid";
        const targetAngle = Math.atan2(nearest.y - this.ship.y, nearest.x - this.ship.x);
        this.faceAngle(targetAngle);

        // If aligned enough, shoot
        const angleDiff = Math.abs(this.ship.angle - targetAngle);
        if (angleDiff < 0.1) {
            aiActionText.innerText = "Firing!";
            this.ship.thrusting = false; // Stop moving while firing for accuracy
            return { action: 'shoot' };
        }

        this.ship.thrusting = false;
    }

    faceAngle(targetAngle) {
        let diff = targetAngle - this.ship.angle;
        while (diff < -Math.PI) diff += Math.PI * 2;
        while (diff > Math.PI) diff -= Math.PI * 2;

        if (diff > 0.05) {
            this.ship.rotatingRight = true;
            this.ship.rotatingLeft = false;
        } else if (diff < -0.05) {
            this.ship.rotatingLeft = true;
            this.ship.rotatingRight = false;
        } else {
            this.ship.rotatingLeft = false;
            this.ship.rotatingRight = false;
        }
    }

    stopAll() {
        this.ship.thrusting = false;
        this.ship.rotatingLeft = false;
        this.ship.rotatingRight = false;
    }
}

// --- Game Loop ---
const ship = new Ship();
const ai = new AIController(ship);
let asteroids = [];
let bullets = [];
let score = 0;

function spawnAsteroids(count) {
    for (let i = 0; i < count; i++) {
        asteroids.push(new Asteroid());
    }
}

spawnAsteroids(5);

function gameLoop() {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const aiDecision = ai.update(asteroids);
    if (aiDecision && aiDecision.action === 'shoot') {
        bullets.push(new Bullet(ship.x, ship.y, ship.angle));
    }

    ship.update();
    ship.draw();

    for (let i = asteroids.length - 1; i >= 0; i--) {
        const a = asteroids[i];
        a.update();
        a.draw();

        // Ship collision
        if (dist(ship.x, ship.y, a.x, a.y) < a.r + ship.r) {
            // Simple reset on death
            score = 0;
            scoreText.innerText = `Score: ${score}`;
            ship.x = canvas.width / 2;
            ship.y = canvas.height / 2;
            ship.vx = 0;
            ship.vy = 0;
        }
    }

    for (let i = bullets.length - 1; i >= 0; i--) {
        const b = bullets[i];
        b.update();
        b.draw();

        if (b.life <= 0) {
            bullets.splice(i, 1);
            continue;
        }

        // Bullet-Asteroid collision
        for (let j = asteroids.length - 1; j >= 0; j--) {
            const a = asteroids[j];
            if (dist(b.x, b.y, a.x, a.y) < a.r) {
                bullets.splice(i, 1);
                if (a.level > 1) {
                    asteroids.push(new Asteroid(a.x, a.y, a.r / 2, a.level - 1));
                    asteroids.push(new Asteroid(a.x, a.y, a.r / 2, a.level - 1));
                }
                asteroids.splice(j, 1);
                score += 10;
                scoreText.innerText = `Score: ${score}`;
                break;
            }
        }
    }

    if (asteroids.length === 0) {
        spawnAsteroids(5);
    }

    requestAnimationFrame(gameLoop);
}

gameLoop();