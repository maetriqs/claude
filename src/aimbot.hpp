// By The_headphones
#pragma once
#include <atomic>
#include <cmath>
#include "ac.hpp"
#include "features.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct AimbotConfig
{
    std::atomic<bool>  enabled  { false };
    std::atomic<int>   fovDeg   { 60 };  // cone of consideration, degrees
    std::atomic<int>   smoothPct{ 0 };   // 0 = instant, 99 = very slow
    std::atomic<bool>  aimHead  { true };// true = head, false = body centre
};

struct AimbotLoop : LoopThread
{
    AimbotConfig* cfg = nullptr;

    void start(const Process* proc, uintptr_t base, AimbotConfig* config)
    {
        cfg = config;
        LoopThread::start("aimbot", [proc, base, config] {

            if (!config->enabled) return;

            ACGame ac { *proc, base };
            uintptr_t lp = ac.localPlayer();
            if (!lp) return;

            // Current camera angles (stored as floats in AC)
            float camX = 0, camY = 0;
            proc->read(lp + 0x34, camX); // yaw
            proc->read(lp + 0x38, camY); // pitch

            // Our head position as origin
            float ox = ac.headX(lp), oy = ac.headY(lp), oz = ac.headZ(lp);

            float bestFov = static_cast<float>(config->fovDeg.load());
            float bestYaw = camX, bestPitch = camY;
            bool  found   = false;

            int count = ac.playerCount();
            for (int i = 0; i < count; ++i)
            {
                uintptr_t ent = ac.entity(i);
                if (!ent || ent == lp) continue;
                if (ac.health(ent) <= 0) continue;

                // Target position
                float tx, ty, tz;
                if (config->aimHead) {
                    tx = ac.headX(ent);
                    ty = ac.headY(ent);
                    tz = ac.headZ(ent);
                } else {
                    tx = ac.posX(ent);
                    ty = ac.posY(ent);
                    tz = ac.posZ(ent);
                }

                float dx = tx - ox;
                float dy = ty - oy;
                float dz = tz - oz;
                float distH = std::sqrt(dx*dx + dz*dz);

                float toYaw   = std::atan2(dx, dz)   * (180.0f / static_cast<float>(M_PI));
                float toPitch = std::atan2(dy, distH) * (180.0f / static_cast<float>(M_PI));

                // Normalise yaw delta to [-180, 180]
                float dYaw = toYaw - camX;
                while (dYaw >  180.0f) dYaw -= 360.0f;
                while (dYaw < -180.0f) dYaw += 360.0f;

                float dPitch = toPitch - camY;
                float fovAngle = std::sqrt(dYaw*dYaw + dPitch*dPitch);

                if (fovAngle < bestFov) {
                    bestFov   = fovAngle;
                    bestYaw   = toYaw;
                    bestPitch = toPitch;
                    found     = true;
                }
            }

            if (!found) return;

            // Apply smoothing: 0% = instant snap, 99% = barely moves per tick
            float t = 1.0f - (config->smoothPct.load() / 100.0f);
            t = std::max(0.01f, t);

            // Normalise target yaw relative to current before lerping
            float dY = bestYaw - camX;
            while (dY >  180.0f) dY -= 360.0f;
            while (dY < -180.0f) dY += 360.0f;
            float writeYaw   = camX   + dY                 * t;
            float writePitch = camY   + (bestPitch - camY) * t;

            proc->write(lp + 0x34, writeYaw);
            proc->write(lp + 0x38, writePitch);

        }, 10); // 100 Hz tick
    }
};
