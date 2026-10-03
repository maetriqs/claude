// By The_headphones
#pragma once
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <string>
#include <iostream>
#include "ac.hpp"

// Generic background loop — start(fn, ms) runs fn() every ms milliseconds.
struct LoopThread
{
    std::atomic<bool> active { false };
    std::thread       t;
    std::string       label;

    bool running() const { return active.load(); }

    void start(std::string name, std::function<void()> fn, int intervalMs = 100)
    {
        if (active) return;
        label  = std::move(name);
        active = true;
        t = std::thread([this, fn, intervalMs] {
            while (active) {
                fn();
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
        });
    }

    void stop()
    {
        active = false;
        if (t.joinable()) t.join();
    }

    ~LoopThread() { stop(); }
};

// ── Named feature loops ──────────────────────────────────────────────────────

struct GodmodeLoop : LoopThread
{
    void start(const Process* proc, uintptr_t base)
    {
        LoopThread::start("godmode", [proc, base] {
            ACGame ac { *proc, base };
            uintptr_t lp = ac.localPlayer();
            if (!lp) return;
            ac.setHealth(lp, 200);
            ac.setArmor (lp, 200);
        }, 80);
    }
};

struct InfAmmoLoop : LoopThread
{
    void start(const Process* proc, uintptr_t base)
    {
        LoopThread::start("infammo", [proc, base] {
            ACGame ac { *proc, base };
            uintptr_t lp = ac.localPlayer();
            if (!lp) return;
            for (uintptr_t s : { Ent::AMMO_AR, Ent::AMMO_SMG, Ent::AMMO_SNIPER,
                                  Ent::AMMO_SHOTGUN, Ent::AMMO_PISTOL,
                                  Ent::AMMO_GRENADE, Ent::AMMO_CARBINE })
                ac.setAmmo(lp, s, 999);
        }, 80);
    }
};

// Locks one entity at a captured position every 50ms.
struct FreezeLoop : LoopThread
{
    Vec3     frozenPos;
    int      targetIdx = -1;

    void start(const Process* proc, uintptr_t base, int idx, Vec3 capturedPos)
    {
        targetIdx  = idx;
        frozenPos  = capturedPos;
        LoopThread::start("freeze[" + std::to_string(idx) + "]",
        [proc, base, idx, capturedPos] {
            ACGame ac { *proc, base };
            uintptr_t ent = ac.entity(idx);
            if (!ent) return;
            ac.setPos(ent, capturedPos);
        }, 50);
    }
};

// ── Convenience: collection of all feature loops ─────────────────────────────
struct Features
{
    GodmodeLoop godmode;
    InfAmmoLoop infammo;
    // Multiple freeze slots (up to 8 enemies)
    FreezeLoop  freeze[8];

    void stopAll()
    {
        godmode.stop();
        infammo.stop();
        for (auto& f : freeze) f.stop();
    }

    void printStatus() const
    {
        auto flag = [](bool b) { return b ? "[ON] " : "[off]"; };
        std::cout << "  godmode   " << flag(godmode.running()) << "\n"
                  << "  infammo   " << flag(infammo.running()) << "\n";
        for (int i = 0; i < 8; ++i)
            if (freeze[i].running())
                std::cout << "  freeze[" << i << "] [ON]  pos=("
                    << freeze[i].frozenPos.x << ", "
                    << freeze[i].frozenPos.y << ", "
                    << freeze[i].frozenPos.z << ")\n";
    }
};
