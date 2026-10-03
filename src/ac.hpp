// By The_headphones
#pragma once
#include <cstdint>
#include "mem.hpp"

// ── static offsets (from ac_client.exe base) ────────────────────────────────
namespace AC {
    constexpr uintptr_t LOCAL_PLAYER  = 0x0017E0A8; // ptr → entity
    constexpr uintptr_t ENTITY_LIST   = 0x0018AC04; // ptr → ptr[]
    constexpr uintptr_t PLAYER_COUNT  = 0x0018AC0C; // int
    constexpr uintptr_t FOV           = 0x0018A7CC; // int
}

// ── entity struct offsets ────────────────────────────────────────────────────
namespace Ent {
    constexpr uintptr_t HEAD_X       = 0x004;
    constexpr uintptr_t HEAD_Z       = 0x008;
    constexpr uintptr_t HEAD_Y       = 0x00C;
    constexpr uintptr_t POS_Z        = 0x028;
    constexpr uintptr_t POS_X        = 0x02C;
    constexpr uintptr_t POS_Y        = 0x030;
    constexpr uintptr_t CAM_X        = 0x034;
    constexpr uintptr_t CAM_Y        = 0x038;
    constexpr uintptr_t HEALTH       = 0x0EC;
    constexpr uintptr_t ARMOR        = 0x0F0;
    constexpr uintptr_t AMMO_PISTOL  = 0x12C;
    constexpr uintptr_t AMMO_CARBINE = 0x130;
    constexpr uintptr_t AMMO_SHOTGUN = 0x134;
    constexpr uintptr_t AMMO_SMG     = 0x138;
    constexpr uintptr_t AMMO_SNIPER  = 0x13C;
    constexpr uintptr_t AMMO_AR      = 0x140;
    constexpr uintptr_t AMMO_GRENADE = 0x144;
    constexpr uintptr_t FF_SHOTGUN   = 0x158; // fast fire (0 = normal)
    constexpr uintptr_t FF_SNIPER    = 0x160;
    constexpr uintptr_t FF_AR        = 0x164;
    constexpr uintptr_t AUTO_SHOOT   = 0x204;
    constexpr uintptr_t NAME         = 0x205; // char[32]
}

// ── helper: read a 32-bit in-game pointer ───────────────────────────────────
// AC is a 32-bit process; all its pointers are uint32_t even on a 64-bit host.
inline uintptr_t readPtr(const Process& p, uintptr_t addr)
{
    uint32_t v = 0;
    p.read(addr, v);
    return static_cast<uintptr_t>(v);
}

struct ACGame
{
    const Process& proc;
    uintptr_t      base = 0; // ac_client.exe base

    uintptr_t localPlayer() const { return readPtr(proc, base + AC::LOCAL_PLAYER); }

    // Read entity pointer from the entity list at slot i
    uintptr_t entity(int i) const
    {
        uintptr_t list = readPtr(proc, base + AC::ENTITY_LIST);
        return readPtr(proc, list + static_cast<uintptr_t>(i) * 4);
    }

    int playerCount() const
    {
        int v = 0; proc.read(base + AC::PLAYER_COUNT, v); return v;
    }

    int fov() const { int v = 0; proc.read(base + AC::FOV, v); return v; }
    void setFov(int v) { proc.write(base + AC::FOV, v); }

    // ── per-entity helpers ───────────────────────────────────────────────────
    int  health(uintptr_t ent) const { int v=0; proc.read(ent+Ent::HEALTH,v); return v; }
    int  armor (uintptr_t ent) const { int v=0; proc.read(ent+Ent::ARMOR ,v); return v; }
    void setHealth(uintptr_t ent, int v) { proc.write(ent+Ent::HEALTH, v); }
    void setArmor (uintptr_t ent, int v) { proc.write(ent+Ent::ARMOR , v); }

    float posX(uintptr_t ent) const { float v=0; proc.read(ent+Ent::POS_X,v); return v; }
    float posY(uintptr_t ent) const { float v=0; proc.read(ent+Ent::POS_Y,v); return v; }
    float posZ(uintptr_t ent) const { float v=0; proc.read(ent+Ent::POS_Z,v); return v; }

    float headX(uintptr_t ent) const { float v=0; proc.read(ent+Ent::HEAD_X,v); return v; }
    float headY(uintptr_t ent) const { float v=0; proc.read(ent+Ent::HEAD_Y,v); return v; }
    float headZ(uintptr_t ent) const { float v=0; proc.read(ent+Ent::HEAD_Z,v); return v; }

    int  ammo(uintptr_t ent, uintptr_t slot) const { int v=0; proc.read(ent+slot,v); return v; }
    void setAmmo(uintptr_t ent, uintptr_t slot, int v) { proc.write(ent+slot, v); }

    // fast fire: 0 = normal, non-zero = instant
    void setFastFire(uintptr_t ent, uintptr_t slot, int v) { proc.write(ent+slot, v); }
    void setAutoShoot(uintptr_t ent, int v) { proc.write(ent+Ent::AUTO_SHOOT, v); }

    std::string name(uintptr_t ent) const
    {
        char buf[33] = {};
        SIZE_T n = 0;
        ReadProcessMemory(proc.handle,
            reinterpret_cast<LPCVOID>(ent + Ent::NAME), buf, 32, &n);
        return std::string(buf);
    }
};
