#include "region_detector.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "log.h"
#include "texts.h"

namespace {

constexpr double kDecay = 0.85;
constexpr double kSwitchThreshold = 512 * 1024;
constexpr double kSwitchRatio = 1.5;

struct Tracked {
    HANDLE handle;
    int region;
};

SRWLOCK g_lock = SRWLOCK_INIT;
Tracked g_tracked[256];
volatile LONG g_trackedCount;
volatile LONG g_bytesRead[32];
volatile LONG g_lastOpened = -1;

int FindTracked(HANDLE h) {
    for (LONG i = 0; i < g_trackedCount; i++)
        if (g_tracked[i].handle == h) return i;
    return -1;
}

void OnFileOpened(HANDLE h, const wchar_t* path) {
    const wchar_t* name = path;
    for (const wchar_t* p = path; *p; p++)
        if (*p == L'\\' || *p == L'/') name = p + 1;
    size_t len = wcslen(name);
    if (len < 6 || _wcsicmp(name + len - 6, L".forge") != 0 || IsSharedArchive(name)) return;

    int region = FindRegionByArchive(name);
    if (region < 0) {
        Log("Unknown archive opened: %s", ToUtf8(name).c_str());
        return;
    }

    AcquireSRWLockExclusive(&g_lock);
    if (FindTracked(h) < 0 && g_trackedCount < (LONG)_countof(g_tracked)) {
        g_tracked[g_trackedCount] = {h, region};
        g_trackedCount = g_trackedCount + 1;
    }
    ReleaseSRWLockExclusive(&g_lock);
    InterlockedExchange(&g_lastOpened, region);
    Log("Opened %s -> %s", ToUtf8(name).c_str(), RegionKey(region));
}

void OnFileRead(HANDLE h, DWORD bytes) {
    if (g_trackedCount == 0) return;
    int region = -1;
    AcquireSRWLockShared(&g_lock);
    int i = FindTracked(h);
    if (i >= 0) region = g_tracked[i].region;
    ReleaseSRWLockShared(&g_lock);
    if (region >= 0) InterlockedExchangeAdd(&g_bytesRead[region], (LONG)bytes);
}

void OnFileClosed(HANDLE h) {
    if (g_trackedCount == 0) return;
    AcquireSRWLockShared(&g_lock);
    bool found = FindTracked(h) >= 0;
    ReleaseSRWLockShared(&g_lock);
    if (!found) return;

    AcquireSRWLockExclusive(&g_lock);
    int i = FindTracked(h);
    if (i >= 0) {
        g_tracked[i] = g_tracked[g_trackedCount - 1];
        g_trackedCount = g_trackedCount - 1;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using ReadFile_t = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using ReadFileEx_t = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE);
using CloseHandle_t = BOOL(WINAPI*)(HANDLE);

CreateFileW_t oCreateFileW = CreateFileW;
CreateFileA_t oCreateFileA = CreateFileA;
ReadFile_t oReadFile = ReadFile;
ReadFileEx_t oReadFileEx = ReadFileEx;
CloseHandle_t oCloseHandle = CloseHandle;

HANDLE WINAPI HookCreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                              DWORD disposition, DWORD flags, HANDLE templ) {
    HANDLE h = oCreateFileW(name, access, share, sa, disposition, flags, templ);
    if (h != INVALID_HANDLE_VALUE && name) {
        DWORD err = GetLastError();
        OnFileOpened(h, name);
        SetLastError(err);
    }
    return h;
}

HANDLE WINAPI HookCreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                              DWORD disposition, DWORD flags, HANDLE templ) {
    HANDLE h = oCreateFileA(name, access, share, sa, disposition, flags, templ);
    if (h != INVALID_HANDLE_VALUE && name) {
        DWORD err = GetLastError();
        wchar_t wide[MAX_PATH * 2];
        if (MultiByteToWideChar(CP_ACP, 0, name, -1, wide, _countof(wide)) > 0) OnFileOpened(h, wide);
        SetLastError(err);
    }
    return h;
}

BOOL WINAPI HookReadFile(HANDLE h, LPVOID buf, DWORD size, LPDWORD read, LPOVERLAPPED ov) {
    BOOL ok = oReadFile(h, buf, size, read, ov);
    DWORD err = GetLastError();
    if (ok || err == ERROR_IO_PENDING) OnFileRead(h, size);
    SetLastError(err);
    return ok;
}

BOOL WINAPI HookReadFileEx(HANDLE h, LPVOID buf, DWORD size, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE cb) {
    BOOL ok = oReadFileEx(h, buf, size, ov, cb);
    DWORD err = GetLastError();
    if (ok) OnFileRead(h, size);
    SetLastError(err);
    return ok;
}

BOOL WINAPI HookCloseHandle(HANDLE h) {
    OnFileClosed(h);
    return oCloseHandle(h);
}

bool HookImport(HMODULE mod, const char* dll, const char* func, void* hook, void** original) {
    auto base = (uint8_t*)mod;
    auto nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return false;

    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; imp++) {
        if (_stricmp((const char*)(base + imp->Name), dll) != 0 || !imp->OriginalFirstThunk) continue;
        auto names = (IMAGE_THUNK_DATA*)(base + imp->OriginalFirstThunk);
        auto slots = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto byName = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((const char*)byName->Name, func) != 0) continue;

            DWORD oldProtect;
            if (!VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect)) return false;
            *original = (void*)slots->u1.Function;
            slots->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&slots->u1.Function, sizeof(void*), oldProtect, &oldProtect);
            return true;
        }
    }
    return false;
}

}

void InstallFileHooks() {
    HMODULE game = GetModuleHandleW(nullptr);
    struct {
        const char* name;
        void* hook;
        void** original;
    } hooks[] = {
        {"CreateFileW", (void*)HookCreateFileW, (void**)&oCreateFileW},
        {"CreateFileA", (void*)HookCreateFileA, (void**)&oCreateFileA},
        {"ReadFile", (void*)HookReadFile, (void**)&oReadFile},
        {"ReadFileEx", (void*)HookReadFileEx, (void**)&oReadFileEx},
        {"CloseHandle", (void*)HookCloseHandle, (void**)&oCloseHandle},
    };
    for (auto& h : hooks) {
        bool ok = HookImport(game, "KERNEL32.dll", h.name, h.hook, h.original);
        Log("Hook %s: %s", h.name, ok ? "ok" : "FAILED");
    }
}

int RegionDetector::Update() {
    int count = RegionCount();
    int best = -1;
    double total = 0;
    for (int r = 0; r < count; r++) {
        score_[r] = score_[r] * kDecay + InterlockedExchange(&g_bytesRead[r], 0);
        total += score_[r];
        if (best < 0 || score_[r] > score_[best]) best = r;
    }

    if (best >= 0 && best != current_ && score_[best] >= kSwitchThreshold &&
        (current_ < 0 || score_[best] > score_[current_] * kSwitchRatio)) {
        current_ = best;
        Log("Region -> %s (%.1f MB)", RegionKey(current_), score_[current_] / (1024 * 1024));
    } else if (current_ < 0 && total == 0 && g_lastOpened >= 0) {
        current_ = g_lastOpened;
        Log("Region -> %s (last opened)", RegionKey(current_));
    }
    return current_;
}

void RegionDetector::LogScores() const {
    std::string line;
    for (int r = 0; r < RegionCount(); r++) {
        if (score_[r] < 64 * 1024) continue;
        char buf[96];
        sprintf_s(buf, " %s=%.2fMB", RegionKey(r), score_[r] / (1024 * 1024));
        line += buf;
    }
    if (!line.empty()) Log("Scores:%s", line.c_str());
}
