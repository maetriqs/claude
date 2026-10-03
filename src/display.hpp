// By The_headphones
#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "ac.hpp"

// ── Console helpers ──────────────────────────────────────────────────────────

inline void enableAnsi()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  m = 0;
    GetConsoleMode(h, &m);
    SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

inline void consoleClear() { std::cout << "\033[2J\033[H" << std::flush; }
inline void consoleHome()  { std::cout << "\033[H"         << std::flush; }

inline void hideCursor()
{
    CONSOLE_CURSOR_INFO ci { 1, FALSE };
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
}

inline void showCursor()
{
    CONSOLE_CURSOR_INFO ci { 1, TRUE };
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
}

// ── ASCII bars ───────────────────────────────────────────────────────────────

// Returns a string like "████████░░░░░░░░" scaled to width chars.
inline std::string bar(int val, int maxVal, int width = 20)
{
    if (maxVal <= 0) maxVal = 1;
    int filled = static_cast<int>(
        std::round(static_cast<double>(std::clamp(val, 0, maxVal)) / maxVal * width));
    return std::string(filled, '\xE2') + "\x96\x88" // UTF-8 U+2588 █
         + std::string(width - filled, '\xE2') + "\x96\x91"; // U+2591 ░
    // ↑ Won't compile cleanly that way; see below for proper UTF-8 approach
}

// Proper UTF-8 block bar using repeated strings
inline std::string blockBar(int val, int maxVal, int width = 20)
{
    if (maxVal <= 0) maxVal = 1;
    int filled = static_cast<int>(
        std::round(static_cast<double>(std::clamp(val, 0, maxVal)) / maxVal * width));
    std::string out;
    out.reserve(width * 3 + 4);
    for (int i = 0; i < width; ++i)
        out += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91"; // █ or ░
    return out;
}

// ── Status display ───────────────────────────────────────────────────────────

inline void printStatus(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();
    if (!lp) { std::cout << "  LocalPlayer null — join a match first\n"; return; }

    int hp    = ac.health(lp);
    int armor = ac.armor(lp);
    Vec3 p    = ac.pos(lp);

    std::cout << std::fixed << std::setprecision(2);
    std::cout
        << "\n  Player : " << ac.name(lp) << "\n\n"
        << "  HP     [" << blockBar(hp,    200) << "] " << std::setw(4) << hp    << " / 200\n"
        << "  Armor  [" << blockBar(armor, 200) << "] " << std::setw(4) << armor << " / 200\n"
        << "\n"
        << "  Pos    X=" << std::setw(9) << p.x
        <<         " Y=" << std::setw(9) << p.y
        <<         " Z=" << std::setw(9) << p.z << "\n"
        << "\n"
        << "  Ammo\n"
        << "    AR="      << std::setw(4) << ac.ammo(lp, Ent::AMMO_AR)
        << "  SMG="       << std::setw(4) << ac.ammo(lp, Ent::AMMO_SMG)
        << "  Sniper="    << std::setw(4) << ac.ammo(lp, Ent::AMMO_SNIPER)
        << "  Shotgun="   << std::setw(4) << ac.ammo(lp, Ent::AMMO_SHOTGUN) << "\n"
        << "    Pistol="  << std::setw(4) << ac.ammo(lp, Ent::AMMO_PISTOL)
        << "  Grenade="   << std::setw(4) << ac.ammo(lp, Ent::AMMO_GRENADE)
        << "  Carbine="   << std::setw(4) << ac.ammo(lp, Ent::AMMO_CARBINE) << "\n"
        << "\n"
        << "  Players: " << ac.playerCount()
        << "   FOV: "    << ac.fov() << "\n\n";
}

// ── Player table ─────────────────────────────────────────────────────────────

inline void printPlayersTable(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();
    Vec3 lpPos   = lp ? ac.pos(lp) : Vec3{};
    int  count   = ac.playerCount();

    std::cout << "\n  " << count << " player(s)\n\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout
        << "  " << std::left
        << std::setw(3)  << "IDX"
        << std::setw(17) << "  NAME"
        << std::setw(26) << "  HP-BAR"
        << std::setw(7)  << " HP"
        << std::setw(7)  << " ARM"
        << std::setw(9)  << " DIST"
        << "\n"
        << "  " << std::string(68, '-') << "\n";

    for (int i = 0; i < count; ++i)
    {
        uintptr_t ent = ac.entity(i);
        if (!ent) continue;

        int   hp   = ac.health(ent);
        int   arm  = ac.armor(ent);
        Vec3  ep   = ac.pos(ent);
        float d    = dist2d(lpPos, ep);
        std::string n = ac.name(ent);
        if (n.empty()) n = "(no name)";

        std::cout
            << "  [" << i << "] "
            << std::left << std::setw(14) << n
            << "  [" << blockBar(hp, 200, 10) << "]"
            << "  " << std::right << std::setw(3) << hp
            << " / " << std::setw(3) << arm
            << "  " << std::setw(6) << d << "u\n";
    }
    std::cout << "\n";
}

// ── ASCII radar ──────────────────────────────────────────────────────────────
// Top-down view using X (east) and Z (north) axes.
// Scale: one cell = RADAR_SCALE world units.

inline void printRadar(const ACGame& ac, float scale = 12.0f)
{
    constexpr int W = 41; // must be odd
    constexpr int H = 21; // must be odd
    constexpr int CX = W / 2;
    constexpr int CY = H / 2;

    char grid[H][W];
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            grid[r][c] = '.';

    uintptr_t lp    = ac.localPlayer();
    Vec3      lpPos = lp ? ac.pos(lp) : Vec3{};
    int       count = ac.playerCount();

    // Plot enemies
    for (int i = 0; i < count; ++i)
    {
        uintptr_t ent = ac.entity(i);
        if (!ent || ent == lp) continue;
        Vec3 ep = ac.pos(ent);
        int col = CX + static_cast<int>((ep.x - lpPos.x) / scale);
        int row = CY - static_cast<int>((ep.z - lpPos.z) / scale); // Z goes up on radar
        if (col >= 0 && col < W && row >= 0 && row < H)
        {
            int hp = ac.health(ent);
            grid[row][col] = (hp <= 0) ? 'x' : 'E';
        }
    }

    // Local player at center
    grid[CY][CX] = '@';

    // Print with border
    std::cout << "\n  +" << std::string(W, '-') << "+\n";
    for (int r = 0; r < H; ++r)
    {
        std::cout << "  |";
        for (int c = 0; c < W; ++c)
        {
            char ch = grid[r][c];
            if      (ch == '@') std::cout << "\033[92m@\033[0m";  // green
            else if (ch == 'E') std::cout << "\033[91mE\033[0m";  // red
            else if (ch == 'x') std::cout << "\033[90mx\033[0m";  // dark grey
            else                std::cout << '.';
        }
        std::cout << "|\n";
    }
    std::cout << "  +" << std::string(W, '-') << "+\n";
    std::cout << "  @ = you   E = enemy   x = dead   scale: 1 cell = "
              << scale << " units\n\n";
}
