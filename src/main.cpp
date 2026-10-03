// By The_headphones
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include "mem.hpp"

static uintptr_t parseAddr(const std::string& s)
{
    return static_cast<uintptr_t>(std::stoull(s, nullptr, 0));
}

static std::string lastError()
{
    DWORD err = GetLastError();
    char buf[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, buf, sizeof(buf), nullptr);
    return std::string(buf);
}

static void printHelp()
{
    std::cout <<
        "Commands:\n"
        "  open <pid>                    open process by PID\n"
        "  read <addr> <type>            read value at address\n"
        "  write <addr> <type> <value>   write value to address\n"
        "  close                         release process handle\n"
        "  help                          show this\n"
        "  quit\n"
        "\n"
        "Types: i8  i16  i32  i64\n"
        "       u8  u16  u32  u64\n"
        "       f32  f64\n"
        "\n"
        "Addresses accept hex (0x...) or decimal.\n"
        "Run as Administrator for most processes.\n";
}

template<typename T>
static void doRead(const Process& proc, uintptr_t addr)
{
    T val{};
    if (proc.read(addr, val))
        std::cout << val << "\n";
    else
        std::cout << "read failed: " << lastError();
}

template<typename T>
static void doWrite(const Process& proc, uintptr_t addr, const std::string& valStr)
{
    T val{};
    std::istringstream(valStr) >> val;
    if (proc.write_force(addr, val))
        std::cout << "ok\n";
    else
        std::cout << "write failed: " << lastError();
}

static void dispatchRead(const Process& proc, uintptr_t addr, const std::string& type)
{
    if      (type == "i8")  doRead<int8_t>  (proc, addr);
    else if (type == "i16") doRead<int16_t> (proc, addr);
    else if (type == "i32") doRead<int32_t> (proc, addr);
    else if (type == "i64") doRead<int64_t> (proc, addr);
    else if (type == "u8")  doRead<uint8_t> (proc, addr);
    else if (type == "u16") doRead<uint16_t>(proc, addr);
    else if (type == "u32") doRead<uint32_t>(proc, addr);
    else if (type == "u64") doRead<uint64_t>(proc, addr);
    else if (type == "f32") doRead<float>   (proc, addr);
    else if (type == "f64") doRead<double>  (proc, addr);
    else std::cout << "unknown type\n";
}

static void dispatchWrite(const Process& proc, uintptr_t addr,
                          const std::string& type, const std::string& val)
{
    if      (type == "i8")  doWrite<int8_t>  (proc, addr, val);
    else if (type == "i16") doWrite<int16_t> (proc, addr, val);
    else if (type == "i32") doWrite<int32_t> (proc, addr, val);
    else if (type == "i64") doWrite<int64_t> (proc, addr, val);
    else if (type == "u8")  doWrite<uint8_t> (proc, addr, val);
    else if (type == "u16") doWrite<uint16_t>(proc, addr, val);
    else if (type == "u32") doWrite<uint32_t>(proc, addr, val);
    else if (type == "u64") doWrite<uint64_t>(proc, addr, val);
    else if (type == "f32") doWrite<float>   (proc, addr, val);
    else if (type == "f64") doWrite<double>  (proc, addr, val);
    else std::cout << "unknown type\n";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "memtool -- type 'help' for commands\n";

    Process proc;
    std::string line;

    while (true)
    {
        std::cout << (proc.valid()
            ? "[pid:" + std::to_string(proc.pid) + "] > "
            : "> ");
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "quit" || cmd == "exit")
            break;

        if (cmd == "help")
        {
            printHelp();
        }
        else if (cmd == "open")
        {
            DWORD pid = 0;
            ss >> pid;
            if (!pid) { std::cout << "usage: open <pid>\n"; continue; }
            if (proc.open(pid))
                std::cout << "opened pid " << pid << "\n";
            else
                std::cout << "OpenProcess failed: " << lastError();
        }
        else if (cmd == "close")
        {
            proc.close();
            std::cout << "closed\n";
        }
        else if (cmd == "read")
        {
            if (!proc.valid()) { std::cout << "no process open\n"; continue; }
            std::string addrStr, type;
            ss >> addrStr >> type;
            if (addrStr.empty() || type.empty())
            { std::cout << "usage: read <addr> <type>\n"; continue; }
            try { dispatchRead(proc, parseAddr(addrStr), type); }
            catch (...) { std::cout << "bad address\n"; }
        }
        else if (cmd == "write")
        {
            if (!proc.valid()) { std::cout << "no process open\n"; continue; }
            std::string addrStr, type, val;
            ss >> addrStr >> type >> val;
            if (addrStr.empty() || type.empty() || val.empty())
            { std::cout << "usage: write <addr> <type> <value>\n"; continue; }
            try { dispatchWrite(proc, parseAddr(addrStr), type, val); }
            catch (...) { std::cout << "bad address\n"; }
        }
        else
        {
            std::cout << "unknown command (type 'help')\n";
        }
    }

    return 0;
}
