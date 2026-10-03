// By The_headphones
#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <array>
#include <chrono>
#include <thread>
#include <optional>
#include "mem.hpp"
#include "process.hpp"
#include "ac.hpp"
#include "features.hpp"
#include "display.hpp"

// ── Globals ──────────────────────────────────────────────────────────────────

static Process  proc;
static uintptr_t base    = 0;
static bool     attached = false;
static Features fx;
static std::array<std::optional<Vec3>, 5> savedPos;

// ── Utilities ────────────────────────────────────────────────────────────────

static std::string lastError()
{
    char buf[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, GetLastError(), 0, buf, sizeof(buf), nullptr);
    for (auto& c : buf) if (c=='\r'||c=='\n') c=' ';
    return buf;
}

static bool tryAttach()
{
    DWORD pid = findPid("ac_client.exe");
    if (!pid) { std::cout << "ac_client.exe not found\n"; return false; }
    if (!proc.open(pid))
    { std::cout << "OpenProcess failed: " << lastError() << "\n"; return false; }
    base = getModuleBase(pid, "ac_client.exe");
    if (!base)
    { std::cout << "module base not found\n"; proc.close(); return false; }
    attached = true;
    std::cout << "attached  pid=" << pid
              << "  base=0x" << std::hex << base << std::dec << "\n";
    return true;
}

static void doDetach()
{
    fx.stopAll();
    proc.close();
    base = 0;
    attached = false;
    std::cout << "detached\n";
}

// Require the game to be attached and return a ready ACGame, or print an error.
#define NEED_AC(name) \
    if (!attached) { std::cout << "not attached — run 'attach'\n"; continue; } \
    ACGame name { proc, base };

// ── Help ─────────────────────────────────────────────────────────────────────

static void printHelp()
{
    std::cout << R"(
  ATTACHMENT
    attach                    find and open ac_client.exe
    detach                    stop all loops, close handle

  DISPLAY
    status                    local player stats + ammo
    players                   table of all players (with dist + HP bar)
    radar [scale]             ASCII top-down radar  (default scale=12)
    watch [ms]                live-refresh status+radar (any key to stop)

  SET (local player)
    set hp      <val>
    set armor   <val>
    set ammo    <gun|all> <val>    guns: ar smg sniper shotgun pistol grenade carbine
    set fov     <val>
    set ff      <gun> <on|off>     fast fire: ar sniper shotgun
    set autoshoot <on|off>
    set pos     <x> <y> <z>        teleport

  POSITION SLOTS  (0-4)
    savepos [slot]            save current position (default slot 0)
    loadpos [slot]            teleport to saved position
    slots                     list all saved positions

  LOOPS
    godmode  <on|off>         continuously write HP=200 Armor=200
    infammo  <on|off>         continuously top up all ammo
    freeze   <idx> <on|off>   lock an entity's position
    loops                     show active background loops

  COMBAT
    kill     <idx|all>        set health to 0

  MISC
    dump     [file]           write all player data to a file
    help
    quit
)";
}

// ── Watch mode ───────────────────────────────────────────────────────────────

static void doWatch(int intervalMs)
{
    consoleClear();
    hideCursor();
    std::cout << "\033[1;1H  [Watch — any key to stop]\n";

    while (!_kbhit())
    {
        consoleHome();
        std::cout << "  [Watch — any key to stop]\n";
        ACGame ac { proc, base };
        printStatus(ac);
        printRadar(ac);
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
    _getch();
    showCursor();
    consoleClear();
}

// ── Dump ─────────────────────────────────────────────────────────────────────

static void doDump(const ACGame& ac, const std::string& path)
{
    std::ofstream f(path);
    if (!f) { std::cout << "cannot open " << path << "\n"; return; }

    uintptr_t lp = ac.localPlayer();
    f << std::fixed << std::setprecision(3);
    f << "=== LocalPlayer ===\n";
    if (lp)
    {
        f << "Name:   " << ac.name(lp) << "\n"
          << "HP:     " << ac.health(lp) << "\n"
          << "Armor:  " << ac.armor(lp)  << "\n"
          << "Pos:    " << ac.posX(lp) << " " << ac.posY(lp) << " " << ac.posZ(lp) << "\n"
          << "Head:   " << ac.headX(lp) << " " << ac.headY(lp) << " " << ac.headZ(lp) << "\n"
          << "Cam:    " << ac.posX(lp) << " " << ac.posY(lp) << "\n"; // cam uses pos offsets
    }
    f << "\n=== Entities ===\n";
    int count = ac.playerCount();
    for (int i = 0; i < count; ++i)
    {
        uintptr_t ent = ac.entity(i);
        if (!ent) continue;
        Vec3 p = ac.pos(ent);
        f << "[" << i << "] " << ac.name(ent)
          << "  HP=" << ac.health(ent)
          << "  Armor=" << ac.armor(ent)
          << "  Pos=" << p.x << "," << p.y << "," << p.z
          << "  Dist=" << dist2d(lp ? ac.pos(lp) : Vec3{}, p) << "\n";
    }
    f << "\nFOV: " << ac.fov() << "\n";
    std::cout << "dumped to " << path << "\n";
}

// ── Ammo / fast-fire slot lookup ─────────────────────────────────────────────

static uintptr_t ammoSlot(const std::string& g)
{
    if (g=="ar")      return Ent::AMMO_AR;
    if (g=="smg")     return Ent::AMMO_SMG;
    if (g=="sniper")  return Ent::AMMO_SNIPER;
    if (g=="shotgun") return Ent::AMMO_SHOTGUN;
    if (g=="pistol")  return Ent::AMMO_PISTOL;
    if (g=="grenade") return Ent::AMMO_GRENADE;
    if (g=="carbine") return Ent::AMMO_CARBINE;
    return 0;
}

static uintptr_t ffSlot(const std::string& g)
{
    if (g=="ar")      return Ent::FF_AR;
    if (g=="sniper")  return Ent::FF_SNIPER;
    if (g=="shotgun") return Ent::FF_SHOTGUN;
    return 0;
}

// ── Main loop ────────────────────────────────────────────────────────────────

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    enableAnsi();
    std::cout << "AssaultCube tool  --  type 'help'\n";

    std::string line;
    while (true)
    {
        std::cout << (attached ? "[AC] > " : "> ");
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd; ss >> cmd;
        if (cmd.empty()) continue;
        if (cmd=="quit"||cmd=="exit") break;

        // ── no-attach commands ───────────────────────────────────────────────
        if (cmd == "help")  { printHelp(); continue; }
        if (cmd == "loops") {
            if (!attached) { std::cout << "not attached\n"; continue; }
            fx.printStatus(); continue;
        }
        if (cmd == "attach") { tryAttach(); continue; }
        if (cmd == "detach") { doDetach(); continue; }

        // ── require attachment ───────────────────────────────────────────────
        NEED_AC(ac)

        // ── display ──────────────────────────────────────────────────────────
        if (cmd == "status")
        {
            printStatus(ac);
        }
        else if (cmd == "players")
        {
            printPlayersTable(ac);
        }
        else if (cmd == "radar")
        {
            float scale = 12.0f;
            ss >> scale;
            printRadar(ac, scale);
        }
        else if (cmd == "watch")
        {
            int ms = 500;
            ss >> ms;
            ms = std::max(100, ms);
            doWatch(ms);
        }

        // ── loops ────────────────────────────────────────────────────────────
        else if (cmd == "godmode")
        {
            std::string onoff; ss >> onoff;
            if (onoff == "on") {
                if (fx.godmode.running()) { std::cout << "already on\n"; continue; }
                fx.godmode.start(&proc, base);
                std::cout << "godmode on\n";
            } else {
                fx.godmode.stop();
                std::cout << "godmode off\n";
            }
        }
        else if (cmd == "infammo")
        {
            std::string onoff; ss >> onoff;
            if (onoff == "on") {
                if (fx.infammo.running()) { std::cout << "already on\n"; continue; }
                fx.infammo.start(&proc, base);
                std::cout << "infammo on\n";
            } else {
                fx.infammo.stop();
                std::cout << "infammo off\n";
            }
        }
        else if (cmd == "freeze")
        {
            int idx; std::string onoff;
            ss >> idx >> onoff;
            if (idx < 0 || idx >= 8)
            { std::cout << "idx must be 0-7\n"; continue; }

            if (onoff == "on") {
                uintptr_t ent = ac.entity(idx);
                if (!ent) { std::cout << "entity " << idx << " not found\n"; continue; }
                Vec3 p = ac.pos(ent);
                fx.freeze[idx].stop();
                fx.freeze[idx].start(&proc, base, idx, p);
                std::cout << "freeze[" << idx << "] on at ("
                    << p.x << ", " << p.y << ", " << p.z << ")\n";
            } else {
                fx.freeze[idx].stop();
                std::cout << "freeze[" << idx << "] off\n";
            }
        }

        // ── combat ───────────────────────────────────────────────────────────
        else if (cmd == "kill")
        {
            std::string target; ss >> target;
            if (target == "all") {
                int count = ac.playerCount();
                uintptr_t lp = ac.localPlayer();
                for (int i = 0; i < count; ++i) {
                    uintptr_t ent = ac.entity(i);
                    if (!ent || ent == lp) continue;
                    ac.setHealth(ent, 0);
                }
                std::cout << "killed all enemies\n";
            } else {
                int idx = std::stoi(target);
                uintptr_t ent = ac.entity(idx);
                if (!ent) { std::cout << "entity not found\n"; continue; }
                ac.setHealth(ent, 0);
                std::cout << "killed [" << idx << "]\n";
            }
        }

        // ── position slots ───────────────────────────────────────────────────
        else if (cmd == "savepos")
        {
            int slot = 0; ss >> slot;
            slot = std::clamp(slot, 0, 4);
            uintptr_t lp = ac.localPlayer();
            if (!lp) { std::cout << "LocalPlayer null\n"; continue; }
            savedPos[slot] = ac.pos(lp);
            Vec3& p = *savedPos[slot];
            std::cout << "slot " << slot << " saved ("
                << p.x << ", " << p.y << ", " << p.z << ")\n";
        }
        else if (cmd == "loadpos")
        {
            int slot = 0; ss >> slot;
            slot = std::clamp(slot, 0, 4);
            if (!savedPos[slot]) { std::cout << "slot " << slot << " empty\n"; continue; }
            uintptr_t lp = ac.localPlayer();
            if (!lp) { std::cout << "LocalPlayer null\n"; continue; }
            ac.setPos(lp, *savedPos[slot]);
            Vec3& p = *savedPos[slot];
            std::cout << "teleported to slot " << slot
                << " (" << p.x << ", " << p.y << ", " << p.z << ")\n";
        }
        else if (cmd == "slots")
        {
            std::cout << std::fixed << std::setprecision(2);
            for (int i = 0; i < 5; ++i) {
                std::cout << "  slot " << i << ": ";
                if (savedPos[i])
                    std::cout << "(" << savedPos[i]->x << ", "
                              << savedPos[i]->y << ", " << savedPos[i]->z << ")\n";
                else
                    std::cout << "(empty)\n";
            }
        }

        // ── set ──────────────────────────────────────────────────────────────
        else if (cmd == "set")
        {
            uintptr_t lp = ac.localPlayer();
            if (!lp) { std::cout << "LocalPlayer null\n"; continue; }
            std::string sub; ss >> sub;

            if (sub == "hp") {
                int v; ss >> v;
                ac.setHealth(lp, v);
                std::cout << "hp = " << v << "\n";
            }
            else if (sub == "armor") {
                int v; ss >> v;
                ac.setArmor(lp, v);
                std::cout << "armor = " << v << "\n";
            }
            else if (sub == "fov") {
                int v; ss >> v;
                ac.setFov(v);
                std::cout << "fov = " << v << "\n";
            }
            else if (sub == "pos") {
                float x, y, z;
                ss >> x >> y >> z;
                ac.setPos(lp, { x, y, z });
                std::cout << "pos = (" << x << ", " << y << ", " << z << ")\n";
            }
            else if (sub == "ammo") {
                std::string gun; int v;
                ss >> gun >> v;
                if (gun == "all") {
                    for (uintptr_t s : { Ent::AMMO_AR, Ent::AMMO_SMG, Ent::AMMO_SNIPER,
                                         Ent::AMMO_SHOTGUN, Ent::AMMO_PISTOL,
                                         Ent::AMMO_GRENADE, Ent::AMMO_CARBINE })
                        ac.setAmmo(lp, s, v);
                    std::cout << "all ammo = " << v << "\n";
                } else {
                    uintptr_t s = ammoSlot(gun);
                    if (!s) { std::cout << "unknown gun\n"; continue; }
                    ac.setAmmo(lp, s, v);
                    std::cout << gun << " ammo = " << v << "\n";
                }
            }
            else if (sub == "ff") {
                std::string gun, onoff;
                ss >> gun >> onoff;
                uintptr_t s = ffSlot(gun);
                if (!s) { std::cout << "unknown gun (ar/sniper/shotgun)\n"; continue; }
                ac.setFastFire(lp, s, onoff == "on" ? 1 : 0);
                std::cout << "fastfire " << gun << " = " << onoff << "\n";
            }
            else if (sub == "autoshoot") {
                std::string onoff; ss >> onoff;
                ac.setAutoShoot(lp, onoff == "on" ? 1 : 0);
                std::cout << "autoshoot = " << onoff << "\n";
            }
            else {
                std::cout << "unknown set target\n";
            }
        }

        // ── dump ─────────────────────────────────────────────────────────────
        else if (cmd == "dump")
        {
            std::string path = "ac_dump.txt";
            ss >> path;
            doDump(ac, path);
        }

        else
        {
            std::cout << "unknown command (type 'help')\n";
        }
    }

    fx.stopAll();
    return 0;
}
