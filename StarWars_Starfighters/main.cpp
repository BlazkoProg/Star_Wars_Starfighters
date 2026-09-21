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
    float HP;
};

struct Star {
    Vector3 direction;
    Color color;
};

enum ShotTeam {
    shot_p, // player
    shot_a, // ally
    shot_e  // enemy
};
struct shot {
    Quaternion direction;
    Vector3 pos;
    ShotTeam team;
};

struct Bot {
    Vector3 pos;
    Quaternion orientation;
    Quaternion targetOrientation;
    bool alive;
    float radius;
    int HP;
    int targetID;
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

int targetLockingAlliesPlayer(Player player, vector<Bot> bots) {
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

const int TARGET_NONE = -1;
const int TARGET_PLAYER = -2;
const float MIN_LOCK_DISTANCE = 20.0f; // nie lockuj na cele bliższe niż to
const float TARGET_STOP_DISTANCE = 8.0f;  // zatrzymaj/zredukuj napęd gdy bardzo blisko celu
const float SEPARATION_RANGE = 100.0f; // zasięg unikania sąsiadów
const float SEPARATION_STRENGTH = 20.0f; // siła separacji
const float SEPARATION_WEIGHT = 0.25f;  // waga separacji względem dążenia do celu

const float ENEMY_ROT_SPEED = 0.5f; // mniejsza zwrotność enemy
const float ALLY_ROT_SPEED = 0.5f; // jeszcze mniejsza zwrotność allies
const float MODEL_CORRECTION_Y_ANGLE = PI; // 180° korekcja wokół Y dla modeli które "patrzą w tył"

int PickTargetForEnemy(const Bot& enemy, const Player& player, const vector<Bot>& allies) {
    int bestId = TARGET_NONE;
    float bestDist = 1e30f;

    // Rozważ gracza tylko jeśli dalej niż MIN_LOCK_DISTANCE
    float dPlayer = Vector3Distance(enemy.pos, player.pos);
    if (dPlayer >= MIN_LOCK_DISTANCE) {
        bestId = TARGET_PLAYER;
        bestDist = dPlayer;
    }

    // Rozważ allies (tylko te dalej niż MIN_LOCK_DISTANCE)
    for (int i = 0; i < (int)allies.size(); ++i) {
        if (!allies[i].alive) continue;
        float d = Vector3Distance(enemy.pos, allies[i].pos);
        if (d < MIN_LOCK_DISTANCE) continue; // ignoruj zbyt bliskie cele
        if (d < bestDist) {
            bestDist = d;
            bestId = i;
        }
    }

    return bestId;
}

Quaternion QuaternionToTarget(Player& player, Vector3 targetPos) {
    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    Vector3 direction = Vector3Subtract(targetPos, player.pos);
    direction = Vector3Normalize(direction);
    return QuaternionFromVector3ToVector3(forward, direction);
}

void UpdateFlight(Player& player, float vel, vector<shot> shots, float radius) {
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

    for (int j = 0; j < (int)shots.size(); j++) {
        if (shots[j].team == shot_p) continue;
        float dist = Vector3Distance(shots[j].pos, player.pos);
        if (dist < radius) {
            player.HP -= 200;
        }
    }
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
void DrawShots(const vector<shot>& shots, Player player) {
    for (size_t i = 0; i < shots.size(); i++) {
        if (Vector3Length(Vector3Subtract(shots[i].pos, player.pos)) > 500.0f) continue;
        Vector3 forward = { 0.0f, 0.0f, 1.0f };
        forward = Vector3RotateByQuaternion(forward, shots[i].direction);
        Vector3 startPos = shots[i].pos;
        Vector3 endPos = Vector3Add(startPos, Vector3Scale(forward, 16.0f));
        Color color = WHITE;
        if (Vector3Length(Vector3Subtract(shots[i].pos, player.pos)) > 20.0f) {
            if (shots[i].team == shot_p)
                color = GREEN;
            else if (shots[i].team == shot_a)
                color = BLUE;
            else if (shots[i].team == shot_e)
                color = RED;
            DrawCapsule(startPos, endPos, 1.0f, 10, 1, color);
        }
    }
}
void Shot(vector<shot>& shots, Vector3 pos, Quaternion orientation, ShotTeam team) {
    float wingX = 2.7f;  // Szerokość skrzydeł (rozpiętość w bok)
    float wingY = 2.0f;  // Wysokość skrzydeł (góra/dół od kadłuba)
    float gunZ = -8.0f;  // Wysunięcie luf działek przed środek ciężkości statku
    Vector3 leftUpperLoc = { -wingX,  0.0, gunZ };
    Vector3 rightUpperLoc = { wingX,  0.0, gunZ };
    Vector3 leftLowerLoc = { -wingX, -wingY, gunZ };
    Vector3 rightLowerLoc = { wingX, -wingY, gunZ };
    leftUpperLoc = Vector3RotateByQuaternion(leftUpperLoc, orientation);
    rightUpperLoc = Vector3RotateByQuaternion(rightUpperLoc, orientation);
    leftLowerLoc = Vector3RotateByQuaternion(leftLowerLoc, orientation);
    rightLowerLoc = Vector3RotateByQuaternion(rightLowerLoc, orientation);
    Vector3 t1_pos = Vector3Add(pos, leftUpperLoc);
    Vector3 t2_pos = Vector3Add(pos, rightUpperLoc);
    Vector3 t3_pos = Vector3Add(pos, leftLowerLoc);
    Vector3 t4_pos = Vector3Add(pos, rightLowerLoc);
    shot t1 = { orientation, t1_pos, team };
    shot t2 = { orientation, t2_pos, team };
    shot t3 = { orientation, t3_pos, team };
    shot t4 = { orientation, t4_pos, team };
    shots.emplace_back(t1);
    shots.emplace_back(t2);
    shots.emplace_back(t3);
    shots.emplace_back(t4);
}

void UpdateEnemies(vector<Bot>& enemies, const vector<Bot>& allies, vector<shot>& shots, const Player& player, vector<Explosion>& explosions, int step, int& playerTargetID, float vel) {
    float dt = GetFrameTime();

    for (int i = 0; i < (int)enemies.size(); ++i) {
        if (!enemies[i].alive) continue;

        // wybór celu
        bool needPick = false;

        if (enemies[i].targetID == TARGET_NONE) {
            needPick = true;
        }
        else if (enemies[i].targetID == TARGET_PLAYER) {
            if (Vector3Distance(enemies[i].pos, player.pos) < MIN_LOCK_DISTANCE)
                needPick = true;
        }
        else {
            int tid = enemies[i].targetID;

            if (tid < 0 || tid >= (int)allies.size() || !allies[tid].alive)
                needPick = true;
            else if (Vector3Distance(enemies[i].pos, allies[tid].pos) < MIN_LOCK_DISTANCE)
                needPick = true;
        }

        if (needPick)
            enemies[i].targetID = PickTargetForEnemy(enemies[i], player, allies);

        Vector3 targetPos = enemies[i].pos;
        Quaternion targetOrientation = QuaternionIdentity();
        bool hasTarget = false;

        if (enemies[i].targetID == TARGET_PLAYER) {
            targetPos = player.pos;
            targetOrientation = player.orientation;
            hasTarget = true;
        }
        else if (enemies[i].targetID >= 0 && enemies[i].targetID < (int)allies.size()) {
            targetPos = allies[enemies[i].targetID].pos;
            targetOrientation = allies[enemies[i].targetID].orientation;
            hasTarget = true;
        }

        // separation
        Vector3 separation = { 0.0f, 0.0f, 0.0f };

        for (int j = 0; j < (int)enemies.size(); ++j) {
            if (i == j) continue;
            if (!enemies[j].alive) continue;

            Vector3 toOther = Vector3Subtract(enemies[i].pos, enemies[j].pos);
            float dist = Vector3Length(toOther);

            if (dist > 0.001f && dist < SEPARATION_RANGE) {
                Vector3 dir = Vector3Scale(toOther, 1.0f / dist);
                float factor = (SEPARATION_RANGE - dist) / SEPARATION_RANGE;
                separation = Vector3Add(separation, Vector3Scale(dir, factor * SEPARATION_STRENGTH));
            }
        }

        for (int j = 0; j < (int)allies.size(); ++j) {
            if (!allies[j].alive) continue;

            Vector3 toOther = Vector3Subtract(enemies[i].pos, allies[j].pos);
            float dist = Vector3Length(toOther);

            if (dist > 0.001f && dist < SEPARATION_RANGE) {
                Vector3 dir = Vector3Scale(toOther, 1.0f / dist);
                float factor = (SEPARATION_RANGE - dist) / SEPARATION_RANGE;
                separation = Vector3Add(separation, Vector3Scale(dir, factor * (SEPARATION_STRENGTH * 0.8f)));
            }
        }

        if (hasTarget) {
            Vector3 toTarget = Vector3Subtract(targetPos, enemies[i].pos);
            float distToTarget = Vector3Length(toTarget);

            // strzelanie z wyprzedzeniem
            float bulletSpeed = 240.0f;

            Vector3 targetForward = { 0.0f, 0.0f, 1.0f };
            targetForward = Vector3RotateByQuaternion(targetForward, targetOrientation);

            Vector3 targetVelocity = Vector3Scale(targetForward, 30.0f);
            float timeToTarget = distToTarget / bulletSpeed;

            Vector3 predictedPos = Vector3Add(targetPos, Vector3Scale(targetVelocity, timeToTarget));
            Vector3 shootDir = Vector3Subtract(predictedPos, enemies[i].pos);

            if (Vector3Length(shootDir) > 0.001f) {
                shootDir = Vector3Normalize(shootDir);

                Matrix shootLookAt = MatrixLookAt(enemies[i].pos, Vector3Add(enemies[i].pos, shootDir), Vector3{ 0.0f, 1.0f, 0.0f });
                shootLookAt = MatrixInvert(shootLookAt);

                Quaternion shootOrientation = QuaternionFromMatrix(shootLookAt);

                if (GetRandomValue(0, 100) > 99)
                    Shot(shots, enemies[i].pos, shootOrientation, shot_e);
            }

            if (distToTarget < TARGET_STOP_DISTANCE) {
                if (Vector3Length(separation) > 0.001f) {
                    Vector3 move = Vector3Normalize(separation);
                    enemies[i].pos = Vector3Add(enemies[i].pos, Vector3Scale(move, vel * 0.4f * dt));

                    Vector3 lookPos = Vector3Add(enemies[i].pos, move);
                    Matrix lookAt = MatrixLookAt(enemies[i].pos, lookPos, Vector3{ 0,1,0 });
                    lookAt = MatrixInvert(lookAt);

                    Quaternion desired = QuaternionFromMatrix(lookAt);
                    enemies[i].orientation = QuaternionSlerp(enemies[i].orientation, desired, fminf(1.0f, ENEMY_ROT_SPEED * dt));
                    enemies[i].orientation = QuaternionNormalize(enemies[i].orientation);
                }

                continue;
            }

            Vector3 desiredDir = Vector3Normalize(toTarget);

            Vector3 sepDir = separation;
            if (Vector3Length(sepDir) > 0.001f)
                sepDir = Vector3Normalize(sepDir);

            Vector3 combined = Vector3Add(desiredDir, Vector3Scale(sepDir, SEPARATION_WEIGHT));

            if (Vector3Length(combined) < 0.001f)
                combined = desiredDir;

            combined = Vector3Normalize(combined);

            // ograniczenie bocznego skrętu
            combined.x *= 0.35f;
            combined = Vector3Normalize(combined);

            Vector3 lookPos = Vector3Add(enemies[i].pos, combined);
            Matrix lookAt = MatrixLookAt(enemies[i].pos, lookPos, Vector3{ 0,1,0 });
            lookAt = MatrixInvert(lookAt);

            Quaternion desired = QuaternionFromMatrix(lookAt);

            enemies[i].orientation = QuaternionSlerp(enemies[i].orientation, desired, fminf(1.0f, ENEMY_ROT_SPEED * dt));
            enemies[i].orientation = QuaternionNormalize(enemies[i].orientation);

            Vector3 forward = { 0.0f, 0.0f, -1.0f };
            forward = Vector3RotateByQuaternion(forward, enemies[i].orientation);

            Vector3 moveVec = Vector3Scale(forward, vel);
            moveVec = Vector3Add(moveVec, Vector3Scale(separation, vel * 0.03f));

            enemies[i].pos = Vector3Add(enemies[i].pos, Vector3Scale(moveVec, dt));
        }
        else {
            Vector3 forward = { 0.0f, 0.0f, -1.0f };
            forward = Vector3RotateByQuaternion(forward, enemies[i].orientation);

            Vector3 move = Vector3Add(Vector3Scale(forward, vel * 0.3f), Vector3Scale(separation, 0.5f));

            if (Vector3Length(move) > 0.001f) {
                enemies[i].pos = Vector3Add(enemies[i].pos, Vector3Scale(Vector3Normalize(move), vel * 0.3f * dt));

                Vector3 lookPos = Vector3Add(enemies[i].pos, move);
                Matrix lookAt = MatrixLookAt(enemies[i].pos, lookPos, Vector3{ 0,1,0 });
                lookAt = MatrixInvert(lookAt);

                Quaternion desired = QuaternionFromMatrix(lookAt);
                enemies[i].orientation = QuaternionSlerp(enemies[i].orientation, desired, fminf(1.0f, ENEMY_ROT_SPEED * dt));
                enemies[i].orientation = QuaternionNormalize(enemies[i].orientation);
            }
        }

        // trafienia strzałami
        for (int j = 0; j < (int)shots.size(); ++j) {
            if (shots[j].team == shot_e) continue;

            float dist = Vector3Distance(shots[j].pos, enemies[i].pos);

            if (dist < enemies[i].radius) {
                enemies[i].HP -= 200;

                if (enemies[i].HP <= 0) {
                    enemies[i].alive = false;

                    shots.erase(shots.begin() + j);

                    Explosion t = { enemies[i].pos, step };
                    explosions.push_back(t);

                    if (playerTargetID == i)
                        playerTargetID = -1;

                    enemies.erase(enemies.begin() + i);
                    --i;
                    break;
                }
                else {
                    shots.erase(shots.begin() + j);
                    --j;
                }
            }
        }
    }
}


void UpdateAllies(vector<Bot>& allies, const vector<Bot>& enemies, vector<shot>& shots, vector<Explosion>& explosions, int step, float vel) {
    float dt = GetFrameTime();
    Quaternion modelCorrection = QuaternionFromAxisAngle(Vector3{ 0.0f, 1.0f, 0.0f }, MODEL_CORRECTION_Y_ANGLE);

    for (int i = 0; i < (int)allies.size(); ++i) {
        if (!allies[i].alive) continue;

        int bestId = TARGET_NONE;
        float bestDist = 1e30f;

        for (int e = 0; e < (int)enemies.size(); ++e) {
            if (!enemies[e].alive) continue;

            float d = Vector3Distance(allies[i].pos, enemies[e].pos);

            if (d < MIN_LOCK_DISTANCE) continue;

            if (d < bestDist) {
                bestDist = d;
                bestId = e;
            }
        }

        allies[i].targetID = bestId;

        Vector3 separation = { 0,0,0 };

        for (int j = 0; j < (int)allies.size(); ++j) {
            if (i == j) continue;
            if (!allies[j].alive) continue;

            Vector3 toOther = Vector3Subtract(allies[i].pos, allies[j].pos);
            float dist = Vector3Length(toOther);

            if (dist > 0.001f && dist < SEPARATION_RANGE) {
                Vector3 dir = Vector3Scale(toOther, 1.0f / dist);
                float factor = (SEPARATION_RANGE - dist) / SEPARATION_RANGE;
                separation = Vector3Add(separation, Vector3Scale(dir, factor * SEPARATION_STRENGTH));
            }
        }

        for (int j = 0; j < (int)enemies.size(); ++j) {
            if (!enemies[j].alive) continue;

            Vector3 toOther = Vector3Subtract(allies[i].pos, enemies[j].pos);
            float dist = Vector3Length(toOther);

            if (dist > 0.001f && dist < SEPARATION_RANGE) {
                Vector3 dir = Vector3Scale(toOther, 1.0f / dist);
                float factor = (SEPARATION_RANGE - dist) / SEPARATION_RANGE;
                separation = Vector3Add(separation, Vector3Scale(dir, factor * (SEPARATION_STRENGTH * 0.6f)));
            }
        }

        if (allies[i].targetID == TARGET_NONE) {
            if (Vector3Length(separation) > 0.001f) {
                Vector3 move = Vector3Scale(Vector3Normalize(separation), vel * 0.4f * dt);
                allies[i].pos = Vector3Add(allies[i].pos, move);

                Matrix lookAt = MatrixLookAt(allies[i].pos, Vector3Add(allies[i].pos, move), Vector3{ 0,1,0 });
                lookAt = MatrixInvert(lookAt);

                Quaternion desired = QuaternionFromMatrix(lookAt);
                desired = QuaternionMultiply(desired, modelCorrection);

                allies[i].orientation = QuaternionSlerp(allies[i].orientation, desired, fminf(1.0f, ALLY_ROT_SPEED * dt));
                allies[i].orientation = QuaternionNormalize(allies[i].orientation);
            }

            continue;
        }

        int targetID = allies[i].targetID;

        if (targetID < 0 || targetID >= (int)enemies.size() || !enemies[targetID].alive) {
            continue;
        }

        Vector3 targetPos = enemies[targetID].pos;
        Vector3 toTarget = Vector3Subtract(targetPos, allies[i].pos);
        float distToTarget = Vector3Length(toTarget);

        // strzelanie do aktualnej pozycji przeciwnika
        if (GetRandomValue(0, 100) > 98) {
            Vector3 shootDir = Vector3Subtract(enemies[targetID].pos, allies[i].pos);

            if (Vector3Length(shootDir) > 0.001f) {
                shootDir = Vector3Normalize(shootDir);

                Matrix shootLookAt = MatrixLookAt(allies[i].pos, Vector3Add(allies[i].pos, shootDir), Vector3{ 0.0f, 1.0f, 0.0f });
                shootLookAt = MatrixInvert(shootLookAt);

                Quaternion shootOrientation = QuaternionFromMatrix(shootLookAt);

                Shot(shots, allies[i].pos, shootOrientation, shot_a);
            }
        }

        if (distToTarget < TARGET_STOP_DISTANCE) {
            if (Vector3Length(separation) > 0.001f) {
                Vector3 move = Vector3Scale(Vector3Normalize(separation), vel * 0.4f * dt);
                allies[i].pos = Vector3Add(allies[i].pos, move);

                Matrix lookAt = MatrixLookAt(allies[i].pos, Vector3Add(allies[i].pos, move), Vector3{ 0,1,0 });
                lookAt = MatrixInvert(lookAt);

                Quaternion desired = QuaternionFromMatrix(lookAt);
                desired = QuaternionMultiply(desired, modelCorrection);

                allies[i].orientation = QuaternionSlerp(allies[i].orientation, desired, fminf(1.0f, ALLY_ROT_SPEED * dt));
                allies[i].orientation = QuaternionNormalize(allies[i].orientation);
            }

            continue;
        }

        Vector3 desiredDir = Vector3Normalize(toTarget);

        Vector3 sepDir = separation;

        if (Vector3Length(sepDir) > 0.001f)
            sepDir = Vector3Normalize(sepDir);

        Vector3 combined = Vector3Add(desiredDir, Vector3Scale(sepDir, SEPARATION_WEIGHT));

        if (Vector3Length(combined) < 0.001f)
            combined = desiredDir;

        combined = Vector3Normalize(combined);

        // ograniczenie bocznego skrętu
        combined.x *= 0.35f;
        combined = Vector3Normalize(combined);

        Vector3 lookPos = Vector3Add(allies[i].pos, combined);
        Matrix lookAt = MatrixLookAt(allies[i].pos, lookPos, Vector3{ 0,1,0 });
        lookAt = MatrixInvert(lookAt);

        Quaternion desired = QuaternionFromMatrix(lookAt);
        desired = QuaternionMultiply(desired, modelCorrection);

        allies[i].orientation = QuaternionSlerp(allies[i].orientation, desired, fminf(1.0f, ALLY_ROT_SPEED * dt));
        allies[i].orientation = QuaternionNormalize(allies[i].orientation);

        Vector3 forward = { 0.0f, 0.0f, 1.0f };
        forward = Vector3RotateByQuaternion(forward, allies[i].orientation);

        Vector3 moveVec = Vector3Scale(forward, vel);
        moveVec = Vector3Add(moveVec, Vector3Scale(separation, vel * 0.03f));

        allies[i].pos = Vector3Add(allies[i].pos, Vector3Scale(moveVec, dt));

        // reakcja na strzały
        for (int j = 0; j < (int)shots.size(); ++j) {
            if (shots[j].team == shot_a) continue;

            float dist = Vector3Distance(shots[j].pos, allies[i].pos);

            if (dist < allies[i].radius) {
                allies[i].HP -= 200;

                if (allies[i].HP <= 0) {
                    allies[i].alive = false;
                    shots.erase(shots.begin() + j);

                    Explosion t = { allies[i].pos, step };
                    explosions.push_back(t);
                    allies.erase(allies.begin() + i);
                    break;
                }
                else {
                    shots.erase(shots.begin() + j);
                    --j;
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

void DrawEnemies(vector<Bot> enemies, Model model, float scale, Shader shader, int targetID) {
    for (int i = 0; i < enemies.size(); i++) {
        if (!enemies[i].alive) continue;

        Vector3 axis;
        float angle;
        QuaternionToAxisAngle(enemies[i].orientation, &axis, &angle);
        angle *= RAD2DEG;

        DrawModelEx(model, enemies[i].pos, axis, angle, Vector3{scale, scale, scale}, WHITE);
        //DrawSphere(e.pos, e.radius, GREEN);
        if (i == targetID) {
            EndShaderMode();
            DrawCubeWires(enemies[i].pos, enemies[i].radius * 2, enemies[i].radius * 2, enemies[i].radius * 2, RED);
            BeginShaderMode(shader);
        }
    }
}
void DrawAllies(vector<Bot> allies, Model model, float scale, Shader shader) {
    for (int i = 0; i < allies.size(); i++) {
        if (!allies[i].alive) continue;

        Vector3 axis;
        float angle;
        QuaternionToAxisAngle(allies[i].orientation, &axis, &angle);
        angle *= RAD2DEG;

        DrawModelEx(model, allies[i].pos, axis, angle, Vector3{ scale, scale, scale }, WHITE);
        //DrawSphere(e.pos, e.radius, GREEN);
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
    #pragma region Init
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

    float a = 30.0f;
    float speed = a;
    float BotSpeed = a;
    float ShotVel = 8 * a;

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
    vector<Bot> enemies;
    vector<Bot> allies;
    vector<Explosion> explosions;
    int NoE = 10; // number of enemies
    int NoA = 10; // number of allies
    float radius = 30.0f;

    for (int i = 0; i < NoE; i++) {
        Bot t;
        t.pos = { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200), (float)GetRandomValue(300, 800) };
        t.orientation = QuaternionIdentity();
        t.alive = true;
        t.radius = radius;
        t.HP = 1000;
        t.targetID = -1;
        enemies.push_back(t);
    }
    for (int i = 0; i < NoA; i++) {
        Bot t;
        t.pos = { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200), (float)GetRandomValue(300, 800) };
        t.orientation = QuaternionIdentity();
        t.alive = true;
        t.radius = radius;
        t.HP = 1000;
        t.targetID = -1;
        allies.push_back(t);
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

    bool turbo = false;
    float ToT = 3.0f; // Time of Turbo

    bool assetsLoaded = false;
    RenderTexture2D menuTarget = LoadRenderTexture(screenSize.x, screenSize.y);
    bool out = false;
    Light light;

    player.HP = 10000.0f;
    #pragma endregion

    while (!WindowShouldClose()) {

        if (isAudioReady) {
            if (GameState >= 0 && GameState <= 3 && step > 1) {
                UpdateMusicStream(musicIntro);
            }
            else if (GameState == 4) {
                UpdateMusicStream(music01);
            }
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

                if (isAudioReady) {
                    StopMusicStream(musicIntro);
                    PlayMusicStream(music01);
                }
            }
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);

            const char* menuText = "STAR WARS";
            float fontSize = 300.0f;
            float spacing = 20.0f;
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

            const char* menuText = "STARFiGHTERS";
            float fontSize = 300.0f;
            float spacing = 20.0f;
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

                    if (isAudioReady) {
                        StopMusicStream(musicIntro);
                        PlayMusicStream(music01);
                    }

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
                targetID = targetLockingAlliesPlayer(player, enemies);
                if (toggleLock) targetID = -1;
                toggleLock = !toggleLock;
            }

            targetYaw = 0.0f; // reset automatycznego yaw każdej klatki, zapobiega akumulacji
            if (toggleLock && targetID != -1) {
                targetRoll += 1.5f * t2 * (GetWorldToScreen(enemies[targetID].pos, camera).x - screenSize.x / 2.0f) / screenSize.x;
                targetPitch = 1.0f * t2 * (GetWorldToScreen(enemies[targetID].pos, camera).y - screenSize.y / 2.0f) / screenSize.y;
                float screenXNorm = (GetWorldToScreen(enemies[targetID].pos, camera).x - screenSize.x / 2.0f) / screenSize.x;
                if (fabs(screenXNorm) < 0.1f) {
                    targetYaw = -1.5f * t2 * screenXNorm;
                    // clamp yaw na bezpieczny zakres (dostosuj, jeśli potrzebujesz)
                    const float MAX_AUTO_YAW = 10.0f;
                    if (targetYaw > MAX_AUTO_YAW) targetYaw = MAX_AUTO_YAW;
                    if (targetYaw < -MAX_AUTO_YAW) targetYaw = -MAX_AUTO_YAW;
                }
                else {
                    targetYaw = 0.0f;
                }

                player.targetOrientation = { targetPitch, targetYaw, targetRoll };
            }
            else {
                // bez locka nie ma auto-yaw
                player.targetOrientation = { targetPitch, 0.0f, targetRoll };
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

            if (ToT >= 0.0f && turbo) {
                ToT -= dt;
                a = 3;
            }
            else
                a = 1;
            if (IsKeyPressed(KEY_LEFT_ALT)) {
                turbo = !turbo;
                if (!turbo) ToT = 3.0f;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ToR <= 0.0f) {
                if (magazine >= 4) {
                    Shot(shots, player.pos, player.orientation, shot_p);
                    if (isAudioReady) {
                        PlaySound(laserChannels[currentChannel]);
                        currentChannel++;
                        if (currentChannel >= MAX_CHANNELS) currentChannel = 0;
                    }
                    magazine -= 4;
                }
            }

            UpdateFlight(player, a * speed, shots, 15.0f);
            UpdateShots(shots, ShotVel);
            UpdateEnemies(enemies, allies, shots, player, explosions, step, targetID, speed);
            UpdateAllies(allies, enemies, shots, explosions, step, speed);

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
            DrawEnemies(enemies, TIE, scale, lightingShader, targetID);
            DrawAllies(allies, xWing, scale, lightingShader);

            EndShaderMode();
            rlDisableDepthTest();
            rlDisableDepthMask();
            DrawShots(shots, player);
            rlEnableDepthMask();
            rlEnableDepthTest();
            BeginShaderMode(lightingShader);

            if (camState == 3) DrawModelEx(xWing, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);
            else if (camState == 2) DrawModelEx(xWing, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);
            else if (camState == 1) DrawModelEx(xWingFPV, player.pos, rotationAxis, rotationAngle, Vector3{ scale, scale, scale }, WHITE);

            EndMode3D();

            EndShaderMode();
            DrawText(TextFormat("Enemies left: %.1f", (float)enemies.size()), 10, 70, 20, WHITE);
            DrawText(TextFormat("Allies left: %.1f", (float)allies.size()), 10, 90, 20, WHITE);
            DrawText(TextFormat("Time of turbo left: %.1f", ToT), 10, 110, 20, WHITE);
            DrawText(TextFormat("HP: %.1f", player.HP), 10, 130, 20, WHITE);
            if (camState != 2) Targeting(screenSize);
            BeginShaderMode(lightingShader);

            EndDrawing();
            step++;

            /*if (enemies.size() <= 0) {
                GameState = 5;
            }
            if (player.HP <= 0) {
                GameState = 6;
            }*/
        }
    }

    #pragma region Unload
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
    #pragma endregion
    return 0;
}