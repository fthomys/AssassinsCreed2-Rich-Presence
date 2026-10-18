#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

#include "config.h"
#include "game_state.h"
#include "log.h"
#include "presence.h"
#include "process_dump.h"
#include "region_detector.h"

namespace {

std::wstring g_basePath;

void LogGameState(const GameState& state) {
    if (state.hasStats)
        Log("Florins %d, health %d/%d (%d/%d bars), position %.1f %.1f %.1f, landmark '%s'", state.florins,
            state.health, state.healthMax, state.healthBars, state.healthBarsMax, state.x, state.y, state.z,
            state.landmark.c_str());
    else
        Log("Player stats unavailable");
    if (state.hasProgress)
        Log("Sequence %d, memory %d, mission state %d (%s)", state.sequence, state.memory, state.missionState,
            state.inMission ? "in mission" : "exploring");
    else Log("Story progress unavailable");
}

DWORD WINAPI Worker(LPVOID) {
    PresenceStart();

    RegionDetector regions;
    GameState state;
    bool supported = false;
    bool versionChecked = false;
    bool dumpKeyDown = false;
    int dumps = 0;
    int region = -1;

    for (int tick = 1;; tick++) {
        Sleep(100);

        if (g_config.dumpProcess) {
            bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (down && !dumpKeyDown) WriteProcessDump(g_basePath + L"." + std::to_wstring(++dumps) + L".dmp");
            dumpKeyDown = down;
        }

        if (tick % 10 != 0) continue;

        int previousRegion = region;
        region = regions.Update();
        if (region != previousRegion) InvalidateGameObjects();

        if (!versionChecked && region >= 0) {
            versionChecked = true;
            supported = IsSupportedExecutable();
            Log("Game version %s", supported ? "supported, reading game state" : "unknown, game state disabled");
        }
        if (supported && (g_config.showStats || g_config.showProgress)) ReadGameState(state);

        if (g_config.debugLog && tick % 100 == 0) {
            regions.LogScores();
            LogGameState(state);
        }

        PresenceUpdate(region, state);
        PresencePoll();
    }
}

bool IsGameProcess() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* name = wcsrchr(path, L'\\');
    return _wcsicmp(name ? name + 1 : path, L"AssassinsCreedIIGame.exe") == 0;
}

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(module);
    if (!IsGameProcess()) return TRUE;

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(module, path, MAX_PATH);
    g_basePath = path;
    size_t dot = g_basePath.find_last_of(L'.');
    if (dot != std::wstring::npos) g_basePath.resize(dot);

    LoadConfig(g_basePath + L".ini");
    if (g_config.debugLog) LogOpen(g_basePath + L".log");
    Log("DiscordPresenceAC2 loaded, language %s", g_language == German ? "de" : "en");

    InstallFileHooks();
    if (HANDLE t = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr)) CloseHandle(t);
    return TRUE;
}
