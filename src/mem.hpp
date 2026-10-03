// By The_headphones
#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

struct Process
{
    HANDLE handle = nullptr;
    DWORD  pid    = 0;

    bool open(DWORD targetPid)
    {
        close();
        handle = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE, targetPid);
        if (handle) pid = targetPid;
        return handle != nullptr;
    }

    void close()
    {
        if (handle) { CloseHandle(handle); handle = nullptr; pid = 0; }
    }

    bool valid() const { return handle != nullptr; }

    template<typename T>
    bool read(uintptr_t addr, T& out) const
    {
        SIZE_T n = 0;
        return ReadProcessMemory(handle,
            reinterpret_cast<LPCVOID>(addr), &out, sizeof(T), &n)
            && n == sizeof(T);
    }

    template<typename T>
    bool write(uintptr_t addr, const T& val) const
    {
        SIZE_T n = 0;
        return WriteProcessMemory(handle,
            reinterpret_cast<LPVOID>(addr), &val, sizeof(T), &n)
            && n == sizeof(T);
    }

    // Write with automatic VirtualProtect if page is read-only
    template<typename T>
    bool write_force(uintptr_t addr, const T& val) const
    {
        DWORD old = 0;
        VirtualProtectEx(handle, reinterpret_cast<LPVOID>(addr),
            sizeof(T), PAGE_EXECUTE_READWRITE, &old);
        bool ok = write(addr, val);
        VirtualProtectEx(handle, reinterpret_cast<LPVOID>(addr),
            sizeof(T), old, &old);
        return ok;
    }

    ~Process() { close(); }
};
