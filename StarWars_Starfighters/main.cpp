#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <iostream>
#include <cmath>
#include <vector>

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

using namespace std;

struct Vec3 {
    double x, y, z;

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(double s) const { return { x * s, y * s, z * s }; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z;  return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z;  return *this; }
};

struct Player {
    Vector3 pos;
    Quaternion orientation;
    Vector3 targetOrientation;
};

struct Star {
    Vector3 direction;
    Color color;
};

struct shot {
    Quaternion direction;
    Vector3 pos;
};

struct Bot {
    Vector3 pos;
    Quaternion orientation;
    Quaternion targetOrientation;
    bool alive;
    float radius;
    int HP;
};

struct Explosion {
    Vector3 pos;
    int fstep;
};

Quaternion RotateToTarget(Quaternion current, Quaternion target, float pitchSpeed, float rollSpeed, float dt) {
    Vector3 localPitchAxis = { 1.0f, 0.0f, 0.0f };
    Vector3 localYawAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 localRollAxis = { 0.0f, 0.0f, 1.0f };

    localPitchAxis = Vector3RotateByQuaternion(localPitchAxis, current);
    localYawAxis = Vector3RotateByQuaternion(localYawAxis, current);
    localRollAxis = Vector3RotateByQuaternion(localRollAxis, current);

    Quaternion pitchMod = QuaternionFromAxisAngle(localPitchAxis, target.x * DEG2RAD);
    Quaternion yawMod = QuaternionFromAxisAngle(localYawAxis, target.y * DEG2RAD);
    Quaternion rollMod = QuaternionFromAxisAngle(localRollAxis, target.z * DEG2RAD);

    current = QuaternionMultiply(pitchMod, current);
    current = QuaternionMultiply(yawMod, current);
    current = QuaternionMultiply(rollMod, current);
    current = QuaternionNormalize(current);
    Vector3 currentEuler = QuaternionToEuler(current);
    Vector3 targetEuler = QuaternionToEuler(target);

    /*float pitch = currentEuler.x;
    float roll = currentEuler.z;

    auto angleDiff = [](float target, float current) {
        float d = target - current;

        while (d > PI)
            d -= 2.0f * PI;
        while (d < -PI)
            d += 2.0f * PI;

        return d;
        };
    float pitchDiff = angleDiff(targetEuler.x, pitch);
    float rollDiff = angleDiff(targetEuler.z, roll);

    float maxPitch = pitchSpeed * dt;
    float maxRoll = rollSpeed * dt;

    if (pitchDiff > maxPitch) pitchDiff = maxPitch;
    else if (pitchDiff < -maxPitch) pitchDiff = -maxPitch;

    if (rollDiff > maxRoll) rollDiff = maxRoll;
    else if (rollDiff < -maxRoll) rollDiff = -maxRoll;

    if (rollDiff > 3.0f * DEG2RAD) {
        roll += rollDiff;
        pitch += 0.2 * pitchDiff;
    }
    else {
        roll += rollDiff;
        pitch += pitchDiff;
    }
    cout << rollDiff << "\n" << pitchDiff << endl;*/

    return current;
}

int targetLockingPlayer(Player player, vector<Bot> bots) {
    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    forward = Vector3RotateByQuaternion(forward, player.orientation);

    int targetID = -1;
    float minAngle = 1000.0f;
    for (int i = 0; i < bots.size(); i++) {
        if (!bots[i].alive) continue;
        Vector3 toTarget = Vector3Subtract(bots[i].pos, player.pos);
        toTarget = Vector3Normalize(toTarget);
        float dot = Vector3DotProduct(forward, toTarget);
        dot = Clamp(dot, -1.0f, 1.0f);
        float angle = acosf(dot);
        if (angle < minAngle) {
            targetID = i;
            minAngle = angle;
        }
    }
    return targetID;
}

Quaternion QuaternionToTarget(Player& player, Vector3 targetPos) {
    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    Vector3 direction = Vector3Subtract(targetPos, player.pos);
    direction = Vector3Normalize(direction);
    return QuaternionFromVector3ToVector3(forward, direction);
}

void UpdateFlight(Player& player, float vel) {
    Vector3 localPitchAxis = {1.0f, 0.0f, 0.0f};
    Vector3 localYawAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 localRollAxis = { 0.0f, 0.0f, 1.0f };

    localPitchAxis = Vector3RotateByQuaternion(localPitchAxis, player.orientation);
    localYawAxis = Vector3RotateByQuaternion(localYawAxis, player.orientation);
    localRollAxis = Vector3RotateByQuaternion(localRollAxis, player.orientation);

    Quaternion pitchMod = QuaternionFromAxisAngle(localPitchAxis, player.targetOrientation.x * DEG2RAD);
    Quaternion yawMod = QuaternionFromAxisAngle(localYawAxis, player.targetOrientation.y * DEG2RAD);
    Quaternion rollMod = QuaternionFromAxisAngle(localRollAxis, player.targetOrientation.z * DEG2RAD);

    player.orientation = QuaternionMultiply(pitchMod, player.orientation);
    player.orientation = QuaternionMultiply(yawMod, player.orientation);
    player.orientation = QuaternionMultiply(rollMod, player.orientation);
    player.orientation = QuaternionNormalize(player.orientation);

    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    forward = Vector3RotateByQuaternion(forward, player.orientation);
    float dt = GetFrameTime();
    player.pos = Vector3Add(player.pos, Vector3Scale(forward, vel * dt));
}

void Targeting(Vector2 screenSize) {
    DrawRing(screenSize / 2, 30, 35, 0, 360, 100, Color(RED));
    DrawLine(screenSize.x / 2 + 3.0, screenSize.y / 2, screenSize.x / 2 + 27.0, screenSize.y / 2, Color(RED));
    DrawLine(screenSize.x / 2 - 3.0, screenSize.y / 2, screenSize.x / 2 - 27.0, screenSize.y / 2, Color(RED));
    DrawLine(screenSize.x / 2, screenSize.y / 2 + 3.0, screenSize.x / 2, screenSize.y / 2 + 27.0, Color(RED));
    DrawLine(screenSize.x / 2, screenSize.y / 2 - 3.0, screenSize.x / 2, screenSize.y / 2 - 27.0, Color(RED));
}

void CameraUpdate(Player player, int state, Camera3D& camera) {
    if (state == 1) {
        Vector3 modelForward = { 0.0f, 0.0f, 1.0f };
        Vector3 modelUp = { 0.0f, 1.0f, 0.0f };
        modelForward = Vector3RotateByQuaternion(modelForward, player.orientation);
        modelUp = Vector3RotateByQuaternion(modelUp, player.orientation);

        Vector3 camOffset = { 0.0f, 0.095f, -0.02f };
        camOffset = Vector3RotateByQuaternion(camOffset, player.orientation);

        camera.position = Vector3Add(player.pos, camOffset);
        camera.target = Vector3Add(player.pos, Vector3Scale(modelForward, 10000000.0f));
        camera.up = modelUp;
    }
    else if (state == 3) {
        Vector3 modelForward = { 0.0f, 0.0f, 1.0f };
        Vector3 modelUp = { 0.0f, 1.0f, 0.0f };
        modelForward = Vector3RotateByQuaternion(modelForward, player.orientation);
        modelUp = Vector3RotateByQuaternion(modelUp, player.orientation);

        Vector3 camOffset = { 0.0f, 2.0f, -20.0f };
        camOffset = Vector3RotateByQuaternion(camOffset, player.orientation);

        camera.position = Vector3Add(player.pos, camOffset);
        camera.target = Vector3Add(player.pos, Vector3Scale(modelForward, 10000000.0f));
        camera.up = modelUp;
    }
    else if (state == 2) {
        Vector3 modelForward = { 0.0f, 0.0f, -1.0f };
        Vector3 modelUp = { 0.0f, 1.0f, 0.0f };
        modelForward = Vector3RotateByQuaternion(modelForward, player.orientation);
        modelUp = Vector3RotateByQuaternion(modelUp, player.orientation);

        Vector3 camOffset = { 0.0f, 2.0f, 20.0f };
        camOffset = Vector3RotateByQuaternion(camOffset, player.orientation);

        camera.position = Vector3Add(player.pos, camOffset);
        camera.target = Vector3Add(player.pos, Vector3Scale(modelForward, 10000000.0f));
        camera.up = modelUp;
    }
}

void ModelOffset(Model& model, int type) {
    BoundingBox box = GetModelBoundingBox(model);

    Vector3 modelCenter = {
        (box.min.x + box.max.x) / 2.0f,
        (box.min.y + box.max.y) / 2.0f,
        (box.min.z + box.max.z) / 2.0f
    };
    Vector3 centerOffset = Vector3Scale(modelCenter, -2.0f);
    if (type == 1) {
        Matrix matOffset = MatrixTranslate(centerOffset.x, centerOffset.y, centerOffset.z);
        Matrix matRotation180 = MatrixRotateY(180.0f * DEG2RAD);
        model.transform = MatrixMultiply(matOffset, matRotation180);
    }
    else model.transform = MatrixTranslate(centerOffset.x, centerOffset.y, centerOffset.z);
}

void UpdateShots(vector<shot>& shots, float ShotVel) {
    for (int i = 0; i < shots.size(); i++) {
        Vector3 forward = { 0.0, 0.0, 1.0 };
        forward = Vector3RotateByQuaternion(forward, shots[i].direction);
        shots[i].pos = Vector3Add(shots[i].pos, Vector3Scale(forward, ShotVel));
        if (abs(shots[i].pos.x) > 1000.0 || abs(shots[i].pos.y) > 1000.0 || abs(shots[i].pos.z) > 1000.0) shots.erase(shots.begin() + i);
    }
}
void DrawShots(const vector<shot>& shots) {
    for (size_t i = 0; i < shots.size(); i++) {
        if (Vector3Length(shots[i].pos) > 300.0f) continue;
        Vector3 forward = { 0.0f, 0.0f, 1.0f };
        forward = Vector3RotateByQuaternion(forward, shots[i].direction);
        Vector3 startPos = shots[i].pos;
        Vector3 endPos = Vector3Add(startPos, Vector3Scale(forward, 16.0f));
        DrawLine3D(startPos, endPos, RED);
    }
}
void Shot(vector<shot>& shots, Player player) {
    float wingX = 2.7f;  // Szerokość skrzydeł (rozpiętość w bok)
    float wingY = 2.0f;  // Wysokość skrzydeł (góra/dół od kadłuba)
    float gunZ = -8.0f;  // Wysunięcie luf działek przed środek ciężkości statku
    Vector3 leftUpperLoc = { -wingX,  0.0, gunZ };
    Vector3 rightUpperLoc = { wingX,  0.0, gunZ };
    Vector3 leftLowerLoc = { -wingX, -wingY, gunZ };
    Vector3 rightLowerLoc = { wingX, -wingY, gunZ };
    leftUpperLoc = Vector3RotateByQuaternion(leftUpperLoc, player.orientation);
    rightUpperLoc = Vector3RotateByQuaternion(rightUpperLoc, player.orientation);
    leftLowerLoc = Vector3RotateByQuaternion(leftLowerLoc, player.orientation);
    rightLowerLoc = Vector3RotateByQuaternion(rightLowerLoc, player.orientation);
    Vector3 t1_pos = Vector3Add(player.pos, leftUpperLoc);
    Vector3 t2_pos = Vector3Add(player.pos, rightUpperLoc);
    Vector3 t3_pos = Vector3Add(player.pos, leftLowerLoc);
    Vector3 t4_pos = Vector3Add(player.pos, rightLowerLoc);
    shot t1 = { player.orientation, t1_pos };
    shot t2 = { player.orientation, t2_pos };
    shot t3 = { player.orientation, t3_pos };
    shot t4 = { player.orientation, t4_pos };
    shots.emplace_back(t1);
    shots.emplace_back(t2);
    shots.emplace_back(t3);
    shots.emplace_back(t4);
}

void UpdateEnemies(vector<Bot>& enemies, vector<shot>& shots, Player player, vector<Explosion>& explosions, int step, int& targetID, float vel) {
    float dt = GetFrameTime();
    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].alive) continue;
        Vector3 upVector = { 0.0f, 1.0f, 0.0f };
        Vector3 forward1 = { 0.0f, 0.0f, -1.0f };
        Vector3 direction = Vector3RotateByQuaternion(forward1, player.orientation);
        Vector3 targetPos = Vector3Add(player.pos, Vector3Scale(direction, 30.0f));
        Matrix lookAt = MatrixLookAt(enemies[i].pos, targetPos, upVector);
        lookAt = MatrixInvert(lookAt);
        Matrix lookAtPlayer = MatrixLookAt(enemies[i].pos, player.pos, upVector);
        lookAtPlayer = MatrixInvert(lookAtPlayer);
        enemies[i].orientation = QuaternionFromMatrix(lookAtPlayer);
        Vector3 forward = { 0.0f, 0.0f, -1.0f };
        forward = Vector3RotateByQuaternion(forward, QuaternionFromMatrix(lookAt));
        enemies[i].pos = Vector3Add(enemies[i].pos, Vector3Scale(forward, vel * dt));

        float range = 20.0f;
        float rangeSqr = range * range;
        float pushSpeed = 5.0f;
        Vector3 separation = { 0.0f, 0.0f, 0.0f };
        for (int j = 0; j < enemies.size(); j++) {
            if (i == j) continue;
            Vector3 toTarget = Vector3Subtract(enemies[i].pos, enemies[j].pos);
            float distSqr = Vector3LengthSqr(toTarget);
            if (distSqr < rangeSqr && distSqr > 0.001f) {
                float dist = sqrtf(distSqr);
                Vector3 direction = Vector3Scale(toTarget, 1.0f / dist);
                float strength = (range - dist) / range;
                separation = Vector3Add(separation, Vector3Scale(direction, strength * pushSpeed * dt));
            }
        }
        enemies[i].pos = Vector3Add(enemies[i].pos, separation);

        for (size_t j = 0; j < shots.size(); j++) {
            float dist = Vector3Distance(shots[j].pos, enemies[i].pos);
            if (dist < enemies[i].radius) {
                enemies[i].HP -= 200;
                if (enemies[i].HP < 0) {
                    enemies[i].alive = false;
                    shots.erase(shots.begin() + j);
                    Explosion t = { enemies[i].pos, step };
                    explosions.push_back(t);
                    if (targetID == i) targetID = -1;
                    enemies.erase(enemies.begin() + i);
                    break;
                }
            }
        }
    }
}

void UpdateExplosions(vector<Explosion>& explosions, int step, Model& model) {
    // Czas trwania wybuchu: 45 klatek (niecała sekunda przy 60 FPS)
    const int EXPLOSION_DURATION = 40;

    for (size_t i = 0; i < explosions.size(); ) {
        int currentAnimFrame = step - explosions[i].fstep;

        if (currentAnimFrame < EXPLOSION_DURATION) {
            // 1. Obliczamy postęp wybuchu od 0.0 do 1.0
            float progress = (float)currentAnimFrame / (float)EXPLOSION_DURATION;

            // 2. Animacja Skali: Wybuch gwałtownie rośnie (od 0 do dużej wartości, np. 80.0)
            float currentScale = progress * 8000.0f;

            // 3. Animacja Zanikania (Alpha Blending): Wybuch staje się coraz bardziej przezroczysty
            unsigned char alpha = (unsigned char)((1.0f - progress) * 255);
            Color tintColor = Color{ 255, 255, 255, alpha };

            // 4. Resetujemy transformację macierzy, aby czysto narysować dany wybuch
            model.transform = MatrixIdentity();

            // 5. Rysujemy model wybuchu w pozycji zniszczonego bota z wyliczoną skalą i zanikaniem
            Vector3 targetPos = explosions[i].pos;
            DrawModelEx(model, targetPos, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, Vector3{ currentScale, currentScale, currentScale }, tintColor);
            i++;
        }
        else {
            // Po 45 klatkach usuwamy wybuch
            explosions.erase(explosions.begin() + i);
        }
    }
}

void DrawEnemies(vector<Bot> enemies, Model model, float scale) {
    for (const auto& e : enemies) {
        if (!e.alive) continue;

        Vector3 axis;
        float angle;
        QuaternionToAxisAngle(e.orientation, &axis, &angle);
        angle *= RAD2DEG;

        DrawModelEx(model, e.pos, axis, angle, Vector3{ scale, scale, scale }, WHITE);
        //DrawSphere(e.pos, e.radius, GREEN);
        DrawCubeWires(e.pos, e.radius * 2, e.radius * 2, e.radius * 2, RED);
    }
}

float alphaFun(float x) {
    float b = 1.0f / 200.0f * x - 0.6f;
    float c = -30 * pow(b, 4);
    return pow(2, c);
}
float zoomFun(float x) {
    float b = 1.0f / 1000.0f * x - 0.15f;
    float c = -1000 * pow(b, 3);
    return pow(2, c);
}

int main() {
    int GameState = 0; // 0 - loading, 1 - press to start, 2 - playing

    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Star Wars: X-Wing Flight Simulator 3D");
    SetTargetFPS(60);
    bool paused = false;

    InitAudioDevice();
    bool isAudioReady = IsAudioDeviceReady();

    Camera3D camera = { 0 };
    camera.fovy = 80.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Player player;
    player.pos = { 0.0f, 0.0f, 0.0f };
    player.orientation = QuaternionIdentity();

    float a = 20.0f;
    float speed = a;
    float BotSpeed = a;
    float ShotVel = 2 * a;

    Model xWing;
    Model xWingFPV;
    Model TIE;
    Model ExplosionModel;

    Model Death_Star;
    Model Destroyer;
    Model Tatooine;
    Model Sun;

    Font OrbitronFont;
    Font JediFont;

    Sound laserSound;
    Sound baseLaser;
    Music musicIntro;
    Music music01;

    Texture2D introBG;

    Shader lightingShader;

    const int MAX_CHANNELS = 4;
    Sound laserChannels[MAX_CHANNELS];
    int currentChannel = 0;

    int animCount;
    ModelAnimation* animations;
    int animFrame = 0;

    float scale = 0.06f;
    int camState = 1;
    ToggleFullscreen();

    Vector2 screenSize = { GetRenderWidth(), GetRenderHeight() };

    const int StarsCount = 500;
    Star stars[StarsCount];

    for (int i = 0; i < StarsCount; i++) {
        Vector3 randPoint = {
            (float)GetRandomValue(-100, 100),
            (float)GetRandomValue(-100, 100),
            (float)GetRandomValue(-100, 100)
        };
        stars[i].direction = Vector3Normalize(randPoint);

        int brightness = GetRandomValue(100, 205);
        int type = GetRandomValue(0, 3);
        if (type == 0)      stars[i].color = Color{ (unsigned char)brightness, (unsigned char)brightness, 205, 205 };
        else if (type == 0) stars[i].color = Color{ 205, (unsigned char)brightness, 205, 205 };
        else                stars[i].color = Color{ (unsigned char)brightness, (unsigned char)brightness, (unsigned char)brightness, 205 };
    }
    Image starImg = GenImageGradientRadial(16, 16, 0.0f, WHITE, BLACK);
    Texture2D starTexture = LoadTextureFromImage(starImg);
    UnloadImage(starImg);

    Image glowImg = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLANK);
    Texture2D sunGlowTexture = LoadTextureFromImage(glowImg);
    UnloadImage(glowImg);

    vector<shot> shots;
    vector<Bot> bots;
    vector<Explosion> explosions;
    int NoE = 10; // number of enemies

    for (int i = 0; i < NoE; i++) {
        Bot t;
        t.pos = { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200), (float)GetRandomValue(300, 800) };
        t.orientation = QuaternionIdentity();
        t.alive = true;
        t.radius = 5.0f;
        t.HP = 1000;
        bots.push_back(t);
    }

    float step = 0.0;

    float targetPitch = 0.0f;
    float targetYaw = 0.0f;
    float targetRoll = 0.0f;

    bool toggleLock = false;
    int targetID = -1;

    float ToR = 0.0f; // Time of Reload
    float ToI = 300.0f; // Time of Intro
    bool zoom = false;
    int magazine = 300;

    bool assetsLoaded = false;
    RenderTexture2D menuTarget = LoadRenderTexture(screenSize.x, screenSize.y);
    bool out = false;
    Light light;

    while (!WindowShouldClose()) {
        if (GameState >= 0 && GameState <= 3 && step > 1) {
            UpdateMusicStream(musicIntro);
        }
        else if (GameState == 4) {
            UpdateMusicStream(music01);
        }
        if (GameState == 0) {
            // 1. Wyświetlamy ekran ładowania
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Loading assets... please wait", screenSize.x/2 - 400.0, screenSize.y/2, 50, LIGHTGRAY);
            EndDrawing();

            // 2. Ładujemy modele (bez żadnych dodatkowych wewnętrznych if-ów)
            xWing =             LoadModel("resources/models/xwingAnimated.glb");
            xWingFPV =          LoadModel("resources/models/xwing.glb");
            TIE =               LoadModel("resources/models/tie_fighter.glb");
            ExplosionModel =    LoadModel("resources/models/TIE_explosion.glb");

            Death_Star =    LoadModel("resources/models/death_star.glb");
            Destroyer =     LoadModel("resources/models/destroyer.glb");
            Tatooine =      LoadModel("resources/models/tatooine.glb");
            Sun =           LoadModel("resources/models/sun.glb");

            lightingShader = LoadShader("resources/shaders/lighting.vs", "resources/shaders/lighting.fs");

            lightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightingShader, "viewPos");

            light = CreateLight(LIGHT_DIRECTIONAL, Vector3{ 10000.0f, 0.0f, 0.0f }, Vector3Zero(), WHITE, lightingShader);

            int ambientLoc = GetShaderLocation(lightingShader, "ambient");
            float ambientColor[4] = { 0.15f, 0.15f, 0.2f, 1.0f };
            SetShaderValue(lightingShader, ambientLoc, ambientColor, SHADER_UNIFORM_VEC4);

            for (int i = 0; i < xWing.materialCount; i++) xWing.materials[i].shader = lightingShader;
            for (int i = 0; i < xWingFPV.materialCount; i++) xWingFPV.materials[i].shader = lightingShader;
            for (int i = 0; i < TIE.materialCount; i++) TIE.materials[i].shader = lightingShader;
            for (int i = 0; i < Destroyer.materialCount; i++) Destroyer.materials[i].shader = lightingShader;
            for (int i = 0; i < Tatooine.materialCount; i++) Tatooine.materials[i].shader = lightingShader;

            OrbitronFont = LoadFontEx("resources/fonts/Orbitron/static/Orbitron-Bold.ttf", 400, NULL, 0);
            JediFont = LoadFontEx("resources/fonts/star_jedi/stjedise/STJEDISE.TTF", 400, NULL, 0);

            introBG = LoadTexture("resources/images/introBG.png");

            if (isAudioReady) {
                laserSound =    LoadSound("resources/sounds/LaserShot.mp3");
                baseLaser =     LoadSound("resources/sounds/LaserShot.mp3");
                musicIntro =    LoadMusicStream("resources/sounds/musicIntro.mp3");
                music01 =       LoadMusicStream("resources/sounds/music01.mp3");
                musicIntro.looping = true;
                music01.looping = true;
                step+=2;
                SetMusicVolume(musicIntro, 1.0f);
                SetMusicVolume(music01, 1.0f);

                for (int i = 0; i < MAX_CHANNELS; i++) {
                    laserChannels[i] = LoadSoundAlias(baseLaser);
                }

                PlayMusicStream(musicIntro);
            }
            animations = LoadModelAnimations("resources/models/TIE_explosion.glb", &animCount);

            Destroyer.transform = MatrixIdentity();
            Destroyer.transform = MatrixMultiply(Destroyer.transform, MatrixRotateX(90.0f * DEG2RAD));
            Destroyer.transform = MatrixMultiply(Destroyer.transform, MatrixRotateY(180.0f * DEG2RAD));

            ModelOffset(xWing, 0);
            ModelOffset(xWingFPV, 1);
            ModelOffset(TIE, 0);
            ModelOffset(Sun, 0);

            GameState = 1;
        }
        else if (GameState == 1) {
            if (IsKeyPressed(KEY_TAB)) {
                GameState = 4;
                StopMusicStream(musicIntro);
                PlayMusicStream(music01);
            }
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);

            const char* menuText = "STAR WARS";
            float fontSize = 300.0f;
            float spacing = 0.0f;
            Vector2 textSize = MeasureTextEx(JediFont, menuText, fontSize * zoomFun(ToI), spacing);
            Vector2 textPos = { (screenSize.x / 2.0f) - (textSize.x / 2.0f), (screenSize.y / 2.0f)  - (textSize.y / 2.0f) };

            DrawTextEx(JediFont, menuText, textPos, fontSize * zoomFun(ToI), spacing, YELLOW);
            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                introBG,
                Rectangle{ 0.0f, 0.0f, (float)introBG.width, (float)introBG.height }, // Źródło (cały obrazek)
                Rectangle{ 0.0f, 0.0f, screenSize.x, screenSize.y },   // Cel (cały ekran)
                Vector2{ 0.0f, 0.0f },                                             // Środek obrotu
                0.0f,                                                              // Kąt obrotu
                WHITE                                                              // Kolor filtrowania (brak)
            );

            float currentAlpha = alphaFun(ToI);
            Rectangle source = { 0.0f, 0.0f, (float)menuTarget.texture.width, -(float)menuTarget.texture.height };
            Rectangle dest = { 0.0f, 0.0f, screenSize.x, screenSize.y };
            Vector2 origin = { 0.0f, 0.0f };
            DrawTexturePro(menuTarget.texture, source, dest, origin, 0.0f, ColorAlpha(WHITE, currentAlpha));
            EndDrawing();
            ToI--;
            if (ToI <= 0.0f) {
                ToI = 300.0f;
                GameState = 2;
            }
        }
        else if (GameState == 2) {
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);

            const char* menuText = "starfighters";
            float fontSize = 300.0f;
            float spacing = 0.0f;
            Vector2 textSize = MeasureTextEx(JediFont, menuText, fontSize * zoomFun(ToI), spacing);
            Vector2 textPos = { (screenSize.x / 2.0f) - (textSize.x / 2.0f), (screenSize.y / 2.0f) - (textSize.y / 2.0f) };

            DrawTextEx(JediFont, menuText, textPos, fontSize * zoomFun(ToI), spacing, YELLOW);
            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                introBG,
                Rectangle{ 0.0f, 0.0f, (float)introBG.width, (float)introBG.height }, // Źródło (cały obrazek)
                Rectangle{ 0.0f, 0.0f, screenSize.x, screenSize.y },   // Cel (cały ekran)
                Vector2{ 0.0f, 0.0f },                                             // Środek obrotu
                0.0f,                                                              // Kąt obrotu
                WHITE                                                              // Kolor filtrowania (brak)
            );

            float currentAlpha = alphaFun(ToI);
            Rectangle source = { 0.0f, 0.0f, (float)menuTarget.texture.width, -(float)menuTarget.texture.height };
            Rectangle dest = { 0.0f, 0.0f, screenSize.x, screenSize.y };
            Vector2 origin = { 0.0f, 0.0f };
            DrawTexturePro(menuTarget.texture, source, dest, origin, 0.0f, ColorAlpha(WHITE, currentAlpha));
            EndDrawing();
            ToI--;
            if (ToI <= 0.0f) {
                ToI = 0.0f;
                GameState = 3;
            }
        }
        else if (GameState == 3) {
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);

            const char* menuText = "press any button\nto start";
            float fontSize = 150.0f;
            float spacing = 0.0f;
            Vector2 textSize = MeasureTextEx(JediFont, menuText, fontSize * 2.0f / PI * atan(step / 100.0f), spacing);
            Vector2 textPos = { (screenSize.x / 2.0f) - (textSize.x / 2.0f), (screenSize.y / 2.0f) - (textSize.y / 2.0f) };

            DrawTextEx(JediFont, menuText, textPos, fontSize * 2.0f / PI * atan(step / 100.0f), spacing, YELLOW);
            EndTextureMode();
            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                introBG,
                Rectangle{ 0.0f, 0.0f, (float)introBG.width, (float)introBG.height }, // Źródło (cały obrazek)
                Rectangle{ 0.0f, 0.0f, screenSize.x, screenSize.y },   // Cel (cały ekran)
                Vector2{ 0.0f, 0.0f },                                             // Środek obrotu
                0.0f,                                                              // Kąt obrotu
                WHITE                                                              // Kolor filtrowania (brak)
            );

            Rectangle source = { 0.0f, 0.0f, (float)menuTarget.texture.width, -(float)menuTarget.texture.height };
            Rectangle dest = { 0.0f, 0.0f, screenSize.x, screenSize.y };
            Vector2 origin = { 0.0f, 0.0f };
            DrawTexturePro(menuTarget.texture, source, dest, origin, 0.0f, ColorAlpha(WHITE, 2.0f / PI * atan(step / 100.0f)));
            EndDrawing();
            if (GetKeyPressed() > 0) {
                out = true;
            }
            if (out) {
                ToI++;
                if (isAudioReady) {
                    float a = exp(-0.01 * ToI);
                    SetMusicVolume(musicIntro, a);
                    if (a < 0.01) {
                        StopMusicStream(musicIntro);
                    }
                }
                if (ToI >= 300.0f) {
                    ToI = 0.0f;
                    StopMusicStream(musicIntro);
                    PlayMusicStream(music01);
                    GameState = 4;
                }
            }
            if (step < 200) step++;
        }
        else if (GameState == 4) {
            float dt = GetFrameTime();

            //bool t = IsKeyDown(KEY_SPACE);
            /*if (IsKeyDown(KEY_S))
                if (t) targetPitch -= 0.5f * GetFrameTime();
                else targetPitch -= 3.0f * GetFrameTime();

            if (IsKeyDown(KEY_W))
                if (t) targetPitch += 0.3f * GetFrameTime();
                else targetPitch += 3.0f * GetFrameTime();

            if (IsKeyDown(KEY_A))
                if (t) targetRoll -= 0.3f * GetFrameTime();
                else targetRoll -= 3.0f * GetFrameTime();

            if (IsKeyDown(KEY_D))
                if (t) targetRoll += 0.3f * GetFrameTime();
                else targetRoll += 3.0f * GetFrameTime();

            if (IsKeyDown(KEY_E))
                if (t) targetYaw -= 0.3f * GetFrameTime();
                else targetYaw -= 3.0f * GetFrameTime();

            if (IsKeyDown(KEY_Q))
                if (t) targetYaw += 0.3f * GetFrameTime();
                else targetYaw += 3.0f * GetFrameTime();*/

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                zoom = !zoom; 
            }
            if (zoom) camera.fovy = 10.0f;
            else camera.fovy = 70.0f;

            float t2 = dt * 200.0f;
            targetRoll = 1.5 * t2 * (GetMousePosition().x - screenSize.x / 2.0) / screenSize.x;
            targetPitch = 1.0 * t2 * (GetMousePosition().y - screenSize.y / 2.0) / screenSize.y;

            if (IsKeyPressed(KEY_F)) {
                targetID = targetLockingPlayer(player, bots);
                if (toggleLock) targetID = -1;
                toggleLock = !toggleLock;
            }

            if (toggleLock && targetID != -1) {
                targetRoll += 1.5f * t2 * (GetWorldToScreen(bots[targetID].pos, camera).x - screenSize.x / 2.0) / screenSize.x;
                targetPitch = 1.0 * t2 * (GetWorldToScreen(bots[targetID].pos, camera).y - screenSize.y / 2.0) / screenSize.y;
                if (abs((GetWorldToScreen(bots[targetID].pos, camera).x - screenSize.x / 2.0) / screenSize.x) < 0.1f)
                    targetYaw = -1.5f * t2 * (GetWorldToScreen(bots[targetID].pos, camera).x - screenSize.x / 2.0) / screenSize.x;

                player.targetOrientation = { targetPitch, targetYaw, targetRoll };
            }
            else {
                player.targetOrientation = { targetPitch, targetYaw, targetRoll };
            }

            if (IsKeyPressed(KEY_ONE)) camState = 1;
            else if (IsKeyPressed(KEY_TWO)) camState = 2;
            else if (IsKeyPressed(KEY_THREE)) camState = 3;

            if (IsKeyPressed(KEY_V)) {
                if (camState > 1) camState--;
                else camState = 3;
            }

            if (ToR >= 0.0f) ToR -= dt;
            if (IsKeyPressed(KEY_R) && ToR <= 0.0f) {
                magazine += 300;
                ToR = 3.0f;
            }

            if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ToR <= 0.0f) {
                if (magazine >= 4) {
                    Shot(shots, player);
                    if (isAudioReady) {
                        PlaySound(laserChannels[currentChannel]);
                        currentChannel++;
                        if (currentChannel >= MAX_CHANNELS) currentChannel = 0;
                    }
                    magazine -= 4;
                }
            }

            UpdateFlight(player, speed);
            UpdateShots(shots, ShotVel);
            UpdateEnemies(bots, shots, player, explosions, step, targetID, speed * 0.8);

            Vector3 rotationAxis;
            float rotationAngle;
            QuaternionToAxisAngle(player.orientation, &rotationAxis, &rotationAngle);
            rotationAngle *= RAD2DEG;

            CameraUpdate(player, camState, camera);
            float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
            SetShaderValue(lightingShader, lightingShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

            UpdateLightValues(lightingShader, light);

            BeginDrawing();
            ClearBackground(Color{ 5, 5, 15, 255 });

            rlSetClipPlanes(0.01f, 1000000.0f);
            BeginMode3D(camera);

            EndShaderMode();
            for (int i = 0; i < StarsCount; i++) {
                Vector3 starPos = Vector3Add(player.pos, Vector3Scale(stars[i].direction, 20000.0f));
                Rectangle sourceRec = { 0.0f, 0.0f, (float)starTexture.width, (float)starTexture.height };
                Vector2 size = { 120.0f, 120.0f };

                DrawBillboardPro(camera, starTexture, sourceRec, starPos, camera.up, size, { size.x / 2, size.y / 2 }, 0.0f, stars[i].color);
            }
            BeginShaderMode(lightingShader);

            DrawModelEx(Tatooine, { 0.0, 0.0, 7500 }, { 0.0f, 0.0f, 0.0f }, 0.0, { 5000.0, 5000.0, 5000.0 }, WHITE);
            DrawModelEx(Destroyer, { 0.0, 3000.0, 0.0 }, { 0.0f, 0.0f, 0.0f }, 0.0, { 0.5, 0.5, 0.5 }, WHITE);
            DrawModelEx(Destroyer, { 500.0, 3000.0, 200.0 }, { 0.0f, 0.0f, 0.0f }, 0.0, { 0.5, 0.5, 0.5 }, WHITE);
            DrawModelEx(Destroyer, { 1300.0, 3000.0, 0.0 }, { 0.0f, 0.0f, 0.0f }, 0.0, { 0.5, 0.5, 0.5 }, WHITE);
            DrawModelEx(Destroyer, { -800.0, 3000.0, -300.0 }, { 0.0f, 0.0f, 0.0f }, 0.0, { 0.5, 0.5, 0.5 }, WHITE);

            EndShaderMode();
            DrawModelEx(Sun, { 10000.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0, { 3, 3, 3 }, WHITE);
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthTest();
            rlDisableDepthMask();
            Vector3 toCamera = Vector3Normalize(Vector3Subtract(camera.position, {9000.0f, 0.0f, 0.0f }));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, toCamera));
            Vector3 up = Vector3CrossProduct(toCamera, right);
            Rectangle sourceRec = { 0.0f, 0.0f, (float)sunGlowTexture.width, (float)sunGlowTexture.height };
            Vector2 size = { 16000.0f, 16000.0f };

            DrawBillboardPro(camera, sunGlowTexture, sourceRec, {9000.0f, 0.0f, 0.0f }, up, size, {size.x / 2.0f, size.y / 2.0f}, 0.0f, WHITE);

            rlEnableDepthMask();
            rlEnableDepthTest();
            EndBlendMode();

            BeginShaderMode(lightingShader);

            UpdateExplosions(explosions, step, ExplosionModel);
            DrawEnemies(bots, TIE, scale);

            DrawShots(shots);

            if (camState == 3) DrawModelEx(xWing, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);
            else if (camState == 2) DrawModelEx(xWing, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);
            else if (camState == 1) DrawModelEx(xWingFPV, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);

            EndMode3D();

            // Wyświetlanie HUD-a
            //DrawText("SYSTEMY NAWIGACJI I RENDEROWANIA AKTYWNE", 10, 10, 20, GREEN);
            //DrawText("Sterowanie: W/S (Pitch) | A/D (Roll) | Q/E (Yaw)", 10, 40, 20, LIGHTGRAY);
            //DrawText(TextFormat("Rotation: X: %.1f, Y: %.1f, Z: %.1f", player.orientation.x, player.orientation.y, player.orientation.z), 10, 70, 20, WHITE);
            DrawText(TextFormat("Enemies left: %.1f", (float)bots.size()), 10, 70, 20, WHITE);
            EndShaderMode();
            if (camState != 2) Targeting(screenSize);
            BeginShaderMode(lightingShader);

            EndDrawing();
            step++;
            targetRoll *= 0.99;
            targetPitch *= 0.9;
            targetYaw *= 0.4;
        }
    }

    UnloadModel(xWing);
    UnloadModel(xWingFPV);
    UnloadModel(Tatooine);
    UnloadModel(Destroyer);
    UnloadModel(Death_Star);
    UnloadModel(ExplosionModel);
    UnloadRenderTexture(menuTarget);
    UnloadShader(lightingShader);
    UnloadTexture(starTexture);
    if (isAudioReady) {
        UnloadSound(laserSound);
        UnloadSound(baseLaser);
        UnloadMusicStream(musicIntro);
        UnloadMusicStream(music01);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
