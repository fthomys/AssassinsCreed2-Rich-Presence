#include "game_state.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cwchar>

#include "log.h"

namespace {

namespace Uplay {
constexpr uintptr_t kVersionCheck = 0x00414A55;
constexpr uint32_t kVersionCheckValue = 0x01CA6FC8;

constexpr uint32_t kVtBhvAssassin = 0x01E07898;
constexpr uint32_t kVtEntity = 0x01CB3720;
constexpr uint32_t kVtHuman = 0x01DE6B70;
constexpr uint32_t kVtCharacterAI = 0x01E0A738;
constexpr uint32_t kVtPlayerHealth = 0x01E0C1A8;
constexpr uint32_t kVtRechargeableContainer = 0x01DE3BA8;
constexpr uint32_t kVtMissionGroup = 0x01CB9C88;
constexpr uint32_t kVtMission = 0x01CB9EA8;
constexpr uint32_t kVtEntityGroup = 0x01CD9DB8;
constexpr uint32_t kVtDatabaseTrigger = 0x01E24B5C;
constexpr uint32_t kVtDatabaseEntry = 0x01E26444;

constexpr uint32_t kFirstSequenceGroupId = 0x10001858;
}

constexpr int kSequenceCount = 14;

constexpr uint32_t kObjectId = 0x04;
constexpr uint32_t kBhvEntity = 0x08;
constexpr uint32_t kBhvCharacterAI = 0x10;
constexpr uint32_t kBhvHuman = 0x18;
constexpr uint32_t kEntityPosition = 0x40;
constexpr uint32_t kEntityWorld = 0x6C;
constexpr uint32_t kEntityComponents = 0x74;
constexpr uint32_t kHumanInventory = 0x20;
constexpr uint32_t kContainerType = 0x08;
constexpr uint32_t kContainerValue = 0x10;
constexpr uint32_t kCharacterAIServices = 0x30;
constexpr uint32_t kHealthCurrent = 0x58;
constexpr uint32_t kHealthMax = 0x5C;
constexpr uint32_t kHealthPerBar = 0x64;
constexpr uint32_t kGroupState = 0x20;
constexpr uint32_t kGroupMissions = 0x50;
constexpr uint32_t kMissionState = 0x18;
constexpr uint32_t kTriggerEntry = 0x08;
constexpr uint32_t kTriggerObject = 0x10;
constexpr uint32_t kEntryCategory = 0x14;
constexpr uint32_t kEntryName = 0x44;

constexpr uint32_t kCategoryBuilding = 1;
constexpr int kMaxTriggers = 2048;
constexpr float kTemplateRadius = 1.0f;
constexpr float kLandmarkRadius = 150.0f;
constexpr float kLandmarkSwitchRatio = 0.8f;

constexpr uint32_t kInventoryTypeMoney = 1;
constexpr uint32_t kGroupInProgress = 2;
constexpr uint32_t kGroupCompleted = 3;
constexpr uint32_t kMissionActive = 5;
constexpr uint32_t kMissionCompleted = 6;

constexpr DWORD kRetryInterval = 5000;
constexpr DWORD kRescanInterval = 30000;

template <typename T>
bool Read(uintptr_t address, T& value) {
    if (address < 0x10000) return false;
    __try {
        value = *reinterpret_cast<const volatile T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint32_t VtableOf(uintptr_t object) {
    uint32_t vt = 0;
    Read(object, vt);
    return vt;
}

bool ReadArray(uintptr_t at, uint32_t& data, uint32_t& count) {
    uint32_t packed;
    if (!Read(at, data) || !Read(at + 4, packed)) return false;
    count = (packed >> 16) & 0x3FFF;
    return true;
}

uint32_t PlayerComponentCount(uintptr_t bhv) {
    uint32_t entity, ai, human, world;
    if (VtableOf(bhv) != Uplay::kVtBhvAssassin) return 0;
    if (!Read(bhv + kBhvEntity, entity) || VtableOf(entity) != Uplay::kVtEntity) return 0;
    if (!Read(bhv + kBhvCharacterAI, ai) || VtableOf(ai) != Uplay::kVtCharacterAI) return 0;
    if (!Read(bhv + kBhvHuman, human) || VtableOf(human) != Uplay::kVtHuman) return 0;
    if (!Read(entity + kEntityWorld, world) || !world) return 0;

    float x, y;
    if (!Read(entity + kEntityPosition, x) || !Read(entity + kEntityPosition + 4, y)) return 0;
    if (x * x + y * y < kTemplateRadius * kTemplateRadius) return 0;

    uint32_t components, count;
    if (!ReadArray(entity + kEntityComponents, components, count)) return 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t component;
        if (Read(components + i * 4, component) && component == bhv) return count;
    }
    return 0;
}

int SequenceOfGroup(uintptr_t group) {
    uint32_t id;
    if (VtableOf(group) != Uplay::kVtMissionGroup || !Read(group + kObjectId, id)) return -1;
    uint32_t index = id - Uplay::kFirstSequenceGroupId;
    return index < (uint32_t)kSequenceCount ? (int)index : -1;
}

struct ScanResult {
    uintptr_t player = 0;
    uint32_t playerComponents = 0;
    int playerCandidates = 0;
    uintptr_t groups[kSequenceCount] = {};
    uintptr_t triggers[kMaxTriggers] = {};
    int triggerCount = 0;
};

void ScanBlock(uintptr_t base, size_t size, ScanResult& result) {
    __try {
        auto words = reinterpret_cast<const volatile uint32_t*>(base);
        for (size_t i = 0, n = size / 4; i < n; i++) {
            uint32_t word = words[i];
            uintptr_t object = base + i * 4;
            if (word == Uplay::kVtBhvAssassin) {
                uint32_t components = PlayerComponentCount(object);
                if (!components) continue;
                result.playerCandidates++;
                if (components > result.playerComponents) {
                    result.player = object;
                    result.playerComponents = components;
                }
            } else if (word == Uplay::kVtMissionGroup) {
                int sequence = SequenceOfGroup(object);
                if (sequence >= 0) result.groups[sequence] = object;
            } else if (word == Uplay::kVtDatabaseTrigger && result.triggerCount < kMaxTriggers) {
                result.triggers[result.triggerCount++] = object;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ScanHeap(ScanResult& result) {
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = 0x10000;
    while (address < 0xFFFF0000 && VirtualQuery((void*)address, &mbi, sizeof(mbi))) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && mbi.Protect == PAGE_READWRITE)
            ScanBlock(base, mbi.RegionSize, result);
        uintptr_t next = base + mbi.RegionSize;
        if (next <= address) break;
        address = next;
    }
}

bool ReadFlorins(uintptr_t human, int& florins) {
    uint32_t inventory, data, count;
    if (!Read(human + kHumanInventory, inventory) || !ReadArray(inventory, data, count)) return false;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t container, type;
        if (!Read(data + i * 4, container) || VtableOf(container) != Uplay::kVtRechargeableContainer) continue;
        if (Read(container + kContainerType, type) && type == kInventoryTypeMoney)
            return Read(container + kContainerValue, florins);
    }
    return false;
}

bool ReadHealth(uintptr_t ai, GameState& state) {
    uint32_t data, count;
    if (!ReadArray(ai + kCharacterAIServices, data, count)) return false;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t service;
        if (!Read(data + i * 4, service) || VtableOf(service) != Uplay::kVtPlayerHealth) continue;
        int perBar = 0;
        if (!Read(service + kHealthCurrent, state.health) || !Read(service + kHealthMax, state.healthMax) ||
            !Read(service + kHealthPerBar, perBar) || perBar <= 0)
            return false;
        state.healthBars = (state.health + perBar - 1) / perBar;
        state.healthBarsMax = state.healthMax / perBar;
        return true;
    }
    return false;
}

bool ReadStats(uintptr_t player, GameState& state) {
    uint32_t entity = 0, ai = 0, human = 0;
    Read(player + kBhvEntity, entity);
    Read(player + kBhvCharacterAI, ai);
    Read(player + kBhvHuman, human);
    Read(entity + kEntityPosition, state.x);
    Read(entity + kEntityPosition + 4, state.y);
    Read(entity + kEntityPosition + 8, state.z);
    return ReadFlorins(human, state.florins) && ReadHealth(ai, state);
}

bool CurrentMission(uintptr_t group, int& index, uint32_t& missionState) {
    uint32_t data, count;
    if (!ReadArray(group + kGroupMissions, data, count)) return false;
    int firstOpen = -1;
    uint32_t firstOpenState = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t entry, mission, state;
        if (!Read(data + i * 4, entry) || !Read(entry, mission) || VtableOf(mission) != Uplay::kVtMission) return false;
        if (!Read(mission + kMissionState, state)) return false;
        if (state == kMissionActive) {
            index = (int)i;
            missionState = state;
            return true;
        }
        if (state != kMissionCompleted && firstOpen < 0) {
            firstOpen = (int)i;
            firstOpenState = state;
        }
    }
    if (firstOpen < 0) return false;
    index = firstOpen;
    missionState = firstOpenState;
    return true;
}

bool ReadProgress(const uintptr_t* groups, GameState& state) {
    for (int s = 0; s < kSequenceCount; s++) {
        uint32_t groupState;
        if (SequenceOfGroup(groups[s]) != s || !Read(groups[s] + kGroupState, groupState)) return false;
        if (groupState != kGroupInProgress) continue;

        int index;
        uint32_t missionState;
        if (!CurrentMission(groups[s], index, missionState)) return false;
        state.sequence = s + 1;
        state.memory = index > 0 ? index : 1;
        state.missionState = (int)missionState;
        state.inMission = missionState == kMissionActive;
        return true;
    }
    return false;
}

bool ResolveLandmark(uintptr_t trigger, uintptr_t& entry, float& x, float& y) {
    uint32_t handle, object, category;
    if (VtableOf(trigger) != Uplay::kVtDatabaseTrigger) return false;
    if (!Read(trigger + kTriggerEntry, handle) || !Read(handle, entry) || VtableOf(entry) != Uplay::kVtDatabaseEntry)
        return false;
    if (!Read(entry + kEntryCategory, category) || category != kCategoryBuilding) return false;
    if (!Read(trigger + kTriggerObject, handle) || !Read(handle, object)) return false;
    uint32_t vt = VtableOf(object);
    if (vt != Uplay::kVtEntity && vt != Uplay::kVtEntityGroup) return false;
    return Read(object + kEntityPosition, x) && Read(object + kEntityPosition + 4, y);
}

bool ReadEntryName(uintptr_t entry, wchar_t* name, size_t size) {
    uint32_t text;
    if (!Read(entry + kEntryName, text)) return false;
    for (size_t i = 0; i + 1 < size; i++) {
        if (!Read(text + i * 2, name[i])) return false;
        if (!name[i]) return i > 0;
    }
    name[size - 1] = 0;
    return true;
}

bool IsDistrictEntry(const wchar_t* name) {
    return wcsncmp(name, L"District of ", 12) == 0;
}

uintptr_t g_landmark;

uintptr_t FindLandmark(const ScanResult& objects, float px, float py) {
    uintptr_t best = 0, current = 0;
    float bestDistance = kLandmarkRadius * kLandmarkRadius;
    float currentDistance = bestDistance;
    wchar_t name[96];
    for (int i = 0; i < objects.triggerCount; i++) {
        uintptr_t entry;
        float x, y;
        if (!ResolveLandmark(objects.triggers[i], entry, x, y)) continue;
        float distance = (x - px) * (x - px) + (y - py) * (y - py);
        if (entry == g_landmark && distance <= currentDistance) {
            current = entry;
            currentDistance = distance;
        }
        if (distance >= bestDistance) continue;
        if (!ReadEntryName(entry, name, _countof(name)) || IsDistrictEntry(name)) continue;
        best = entry;
        bestDistance = distance;
    }
    float ratio = kLandmarkSwitchRatio * kLandmarkSwitchRatio;
    if (current && (!best || bestDistance > currentDistance * ratio)) return current;
    return best;
}

ScanResult g_objects;
DWORD g_lastScan;

bool CachedObjectsValid() {
    if (!g_objects.player || !PlayerComponentCount(g_objects.player)) return false;
    for (int s = 0; s < kSequenceCount; s++)
        if (SequenceOfGroup(g_objects.groups[s]) != s) return false;
    return true;
}

void Rescan() {
    DWORD start = GetTickCount();
    uintptr_t previous = g_objects.player;
    g_objects = ScanResult{};
    ScanHeap(g_objects);

    int groups = 0;
    for (uintptr_t g : g_objects.groups) groups += g != 0;
    if (g_objects.player != previous)
        Log("Heap scan took %lu ms: %d player candidates, picked %08X with %u components, %d/%d sequence groups, "
            "%d landmark triggers",
            GetTickCount() - start, g_objects.playerCandidates, (unsigned)g_objects.player,
            g_objects.playerComponents, groups, kSequenceCount, g_objects.triggerCount);
}

}

bool IsSupportedExecutable() {
    uint32_t value = 0;
    return Read(Uplay::kVersionCheck, value) && value == Uplay::kVersionCheckValue;
}

void InvalidateGameObjects() {
    g_lastScan = GetTickCount() - kRescanInterval;
}

void ReadGameState(GameState& state) {
    state = GameState{};
    DWORD now = GetTickCount();
    DWORD interval = CachedObjectsValid() ? kRescanInterval : kRetryInterval;
    if (now - g_lastScan >= interval) {
        g_lastScan = now;
        Rescan();
    }

    if (g_objects.player && PlayerComponentCount(g_objects.player))
        state.hasStats = ReadStats(g_objects.player, state);
    state.hasProgress = ReadProgress(g_objects.groups, state);

    g_landmark = state.hasStats ? FindLandmark(g_objects, state.x, state.y) : 0;
    wchar_t name[96];
    if (g_landmark && ReadEntryName(g_landmark, name, _countof(name))) state.landmark = ToUtf8(name);
}
