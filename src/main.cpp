// By The_headphones
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include "mem.hpp"
#include "process.hpp"
#include "ac.hpp"

static std::string lastError()
{
    char buf[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, GetLastError(), 0, buf, sizeof(buf), nullptr);
    // trim newline
    for (auto& c : buf) if (c == '\r' || c == '\n') c = ' ';
    return buf;
}

static void printHelp()
{
    std::cout <<
        "\nCommands:\n"
        "  attach                attach to ac_client.exe\n"
        "  detach                release handle\n"
        "  status                print player/game state\n"
        "  players               list all players\n"
        "  set hp     <val>      set local player health\n"
        "  set armor  <val>      set local player armor\n"
        "  set ammo   <gun> <val> set ammo (ar/smg/sniper/shotgun/pistol/grenade/carbine/all)\n"
        "  set fov    <val>      set field of view\n"
        "  set ff     <gun> <on|off>  fast fire (ar/sniper/shotgun)\n"
        "  set autoshoot <on|off>\n"
        "  help\n"
        "  quit\n\n"
        "Run as Administrator.\n\n";
}

static uintptr_t ammoSlot(const std::string& gun)
{
    if (gun == "ar")      return Ent::AMMO_AR;
    if (gun == "smg")     return Ent::AMMO_SMG;
    if (gun == "sniper")  return Ent::AMMO_SNIPER;
    if (gun == "shotgun") return Ent::AMMO_SHOTGUN;
    if (gun == "pistol")  return Ent::AMMO_PISTOL;
    if (gun == "grenade") return Ent::AMMO_GRENADE;
    if (gun == "carbine") return Ent::AMMO_CARBINE;
    return 0;
}

static uintptr_t ffSlot(const std::string& gun)
{
    if (gun == "ar")      return Ent::FF_AR;
    if (gun == "sniper")  return Ent::FF_SNIPER;
    if (gun == "shotgun") return Ent::FF_SHOTGUN;
    return 0;
}

static void printStatus(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();
    if (!lp) { std::cout << "LocalPlayer pointer is null — is the game in a match?\n"; return; }

    std::cout << std::fixed << std::setprecision(2);
    std::cout
        << "\n=== Local Player  [" << ac.name(lp) << "] ===\n"
        << "  HP:     " << ac.health(lp) << "\n"
        << "  Armor:  " << ac.armor(lp)  << "\n"
        << "  Pos:    X=" << ac.posX(lp) << "  Y=" << ac.posY(lp) << "  Z=" << ac.posZ(lp) << "\n"
        << "  Head:   X=" << ac.headX(lp)<< "  Y=" << ac.headY(lp)<< "  Z=" << ac.headZ(lp)<< "\n"
        << "\n=== Ammo ===\n"
        << "  AR="      << ac.ammo(lp, Ent::AMMO_AR)
        << "  SMG="     << ac.ammo(lp, Ent::AMMO_SMG)
        << "  Sniper="  << ac.ammo(lp, Ent::AMMO_SNIPER)
        << "  Shotgun=" << ac.ammo(lp, Ent::AMMO_SHOTGUN)
        << "\n"
        << "  Pistol="  << ac.ammo(lp, Ent::AMMO_PISTOL)
        << "  Grenade=" << ac.ammo(lp, Ent::AMMO_GRENADE)
        << "  Carbine=" << ac.ammo(lp, Ent::AMMO_CARBINE)
        << "\n"
        << "\n=== Server ===\n"
        << "  Players: " << ac.playerCount() << "\n"
        << "  FOV:     " << ac.fov() << "\n\n";
}

static void printPlayers(const ACGame& ac)
{
    int count = ac.playerCount();
    std::cout << "\n" << count << " player(s):\n";
    std::cout << std::fixed << std::setprecision(1);
    for (int i = 0; i < count; ++i)
    {
        uintptr_t ent = ac.entity(i);
        if (!ent) continue;
        std::cout
            << "  [" << i << "] " << std::setw(16) << std::left << ac.name(ent)
            << "  HP=" << std::setw(4) << ac.health(ent)
            << "  Armor=" << std::setw(4) << ac.armor(ent)
            << "  Pos=(" << ac.posX(ent) << ", " << ac.posY(ent) << ", " << ac.posZ(ent) << ")\n";
    }
    std::cout << "\n";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "AssaultCube tool  --  type 'help' for commands\n";

    Process proc;
    uintptr_t base = 0;
    bool attached = false;

    auto tryAttach = [&]() -> bool {
        DWORD pid = findPid("ac_client.exe");
        if (!pid) { std::cout << "ac_client.exe not found\n"; return false; }
        if (!proc.open(pid)) { std::cout << "OpenProcess failed: " << lastError() << "\n"; return false; }
        base = getModuleBase(pid, "ac_client.exe");
        if (!base) { std::cout << "couldn't read module base\n"; proc.close(); return false; }
        std::cout << "attached  pid=" << pid
                  << "  base=0x" << std::hex << base << std::dec << "\n";
        attached = true;
        return true;
    };

    std::string line;
    while (true)
    {
        std::cout << (attached ? "[AC] > " : "> ");
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd.empty()) continue;
        if (cmd == "quit" || cmd == "exit") break;
        if (cmd == "help") { printHelp(); continue; }

        if (cmd == "attach") { tryAttach(); continue; }

        if (cmd == "detach")
        {
            proc.close(); base = 0; attached = false;
            std::cout << "detached\n";
            continue;
        }

        if (!attached) { std::cout << "not attached (run 'attach')\n"; continue; }

        ACGame ac{ proc, base };

        if (cmd == "status")
        {
            printStatus(ac);
        }
        else if (cmd == "players")
        {
            printPlayers(ac);
        }
        else if (cmd == "set")
        {
            uintptr_t lp = ac.localPlayer();
            if (!lp) { std::cout << "LocalPlayer is null\n"; continue; }

            std::string sub; ss >> sub;

            if (sub == "hp")
            {
                int v; ss >> v;
                ac.setHealth(lp, v);
                std::cout << "hp = " << v << "\n";
            }
            else if (sub == "armor")
            {
                int v; ss >> v;
                ac.setArmor(lp, v);
                std::cout << "armor = " << v << "\n";
            }
            else if (sub == "fov")
            {
                int v; ss >> v;
                ac.setFov(v);
                std::cout << "fov = " << v << "\n";
            }
            else if (sub == "ammo")
            {
                std::string gun; int v; ss >> gun >> v;
                if (gun == "all")
                {
                    for (uintptr_t s : { Ent::AMMO_AR, Ent::AMMO_SMG, Ent::AMMO_SNIPER,
                                         Ent::AMMO_SHOTGUN, Ent::AMMO_PISTOL,
                                         Ent::AMMO_GRENADE, Ent::AMMO_CARBINE })
                        ac.setAmmo(lp, s, v);
                    std::cout << "all ammo = " << v << "\n";
                }
                else
                {
                    uintptr_t slot = ammoSlot(gun);
                    if (!slot) { std::cout << "unknown gun\n"; continue; }
                    ac.setAmmo(lp, slot, v);
                    std::cout << gun << " ammo = " << v << "\n";
                }
            }
            else if (sub == "ff")
            {
                std::string gun, onoff; ss >> gun >> onoff;
                uintptr_t slot = ffSlot(gun);
                if (!slot) { std::cout << "unknown gun (ar/sniper/shotgun)\n"; continue; }
                int v = (onoff == "on") ? 1 : 0;
                ac.setFastFire(lp, slot, v);
                std::cout << "fast fire " << gun << " = " << onoff << "\n";
            }
            else if (sub == "autoshoot")
            {
                std::string onoff; ss >> onoff;
                ac.setAutoShoot(lp, (onoff == "on") ? 1 : 0);
                std::cout << "autoshoot = " << onoff << "\n";
            }
            else
            {
                std::cout << "unknown set target (hp/armor/ammo/fov/ff/autoshoot)\n";
            }
        }
        else
        {
            std::cout << "unknown command\n";
        }
    }

    return 0;
}
