// By The_headphones
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstring>

inline DWORD findPid(const char* exeName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32First(snap, &pe))
        do {
            if (_stricmp(pe.szExeFile, exeName) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));

    CloseHandle(snap);
    return pid;
}

inline uintptr_t getModuleBase(DWORD pid, const char* modName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32 me = { sizeof(me) };
    uintptr_t base = 0;
    if (Module32First(snap, &me))
        do {
            if (_stricmp(me.szModule, modName) == 0)
            { base = reinterpret_cast<uintptr_t>(me.modBaseAddr); break; }
        } while (Module32Next(snap, &me));

    CloseHandle(snap);
    return base;
}
