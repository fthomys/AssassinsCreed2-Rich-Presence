#include "process_dump.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "log.h"

void WriteProcessDump(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    using MiniDumpWriteDump_t = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
    auto writeDump = (MiniDumpWriteDump_t)GetProcAddress(LoadLibraryW(L"dbghelp.dll"), "MiniDumpWriteDump");
    constexpr int kMiniDumpWithFullMemory = 0x00000002;
    BOOL ok = writeDump && writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, kMiniDumpWithFullMemory,
                                     nullptr, nullptr, nullptr);
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    CloseHandle(file);
    Log("Process dump %s: %s (%lld MB)", ToUtf8(path.c_str()).c_str(), ok ? "ok" : "FAILED",
        size.QuadPart / (1024 * 1024));
}
