#include "presence.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <ctime>
#include <map>
#include <string>

#include <discord_rpc.h>

#include "config.h"
#include "log.h"
#include "texts.h"

namespace {

constexpr DWORD kMinUpdateInterval = 15000;
const std::string kSeparator = " · ";

int64_t g_startTime;
volatile LONG g_resend;
bool g_started;
int g_lastRegion = -2;
DWORD g_lastSend;
std::string g_lastDetails, g_lastState, g_lastLargeText;

void HandleReady(const DiscordUser* user) {
    Log("Connected to Discord as %s", user->username);
    InterlockedExchange(&g_resend, 1);
}

void HandleDisconnected(int code, const char* message) {
    Log("Discord disconnected (%d): %s", code, message);
}

void HandleError(int code, const char* message) {
    Log("Discord error (%d): %s", code, message);
}

std::string FormatNumber(int value, Language language) {
    std::string digits = std::to_string(value < 0 ? -value : value);
    std::string out;
    int n = (int)digits.size();
    for (int i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) out += language == German ? '.' : ',';
        out += digits[i];
    }
    return value < 0 ? "-" + out : out;
}

using Values = std::map<std::string, std::string>;

bool ExpandSegment(std::string segment, const Values& values, std::string& out) {
    size_t pos = 0;
    while ((pos = segment.find('{', pos)) != std::string::npos) {
        size_t end = segment.find('}', pos);
        if (end == std::string::npos) break;
        auto it = values.find(segment.substr(pos + 1, end - pos - 1));
        if (it == values.end() || it->second.empty()) return false;
        segment.replace(pos, end - pos + 1, it->second);
        pos += it->second.size();
    }
    out = segment;
    return true;
}

std::string Expand(const std::string& format, const Values& values) {
    std::string result;
    size_t start = 0;
    while (start <= format.size()) {
        size_t end = format.find(kSeparator, start);
        if (end == std::string::npos) end = format.size();
        std::string segment;
        if (ExpandSegment(format.substr(start, end - start), values, segment) && !segment.empty())
            result += (result.empty() ? "" : kSeparator) + segment;
        start = end + kSeparator.size();
    }
    return result;
}

Values CollectValues(int region, const GameState& state) {
    const Text& text = RegionText(region, g_language);
    Values values = {{"region", text.name}, {"region_info", text.info}};
    if (g_config.showStats && state.hasStats) {
        values["florins"] = FormatNumber(state.florins, g_language);
        values["health"] = std::to_string(state.healthBars);
        values["health_max"] = std::to_string(state.healthBarsMax);
    }
    if (g_config.showProgress && state.hasProgress && state.inMission) {
        values["sequence"] = std::to_string(state.sequence);
        values["memory"] = std::to_string(state.memory);
    } else if (state.hasStats) {
        values["exploring"] = ExploringText(g_language);
        if (g_config.showLandmark) values["landmark"] = state.landmark;
    }
    return values;
}

}

void PresenceStart() {
    g_startTime = (int64_t)time(nullptr);
    if (g_config.applicationId.empty()) {
        Log("No ApplicationId set, Discord presence disabled");
        return;
    }
    DiscordEventHandlers handlers{};
    handlers.ready = HandleReady;
    handlers.disconnected = HandleDisconnected;
    handlers.errored = HandleError;
    Discord_Initialize(g_config.applicationId.c_str(), &handlers, 0, nullptr);
    g_started = true;
}

void PresenceUpdate(int region, const GameState& state) {
    if (!g_started) return;

    const LineFormats& formats = Formats(g_language);
    Values values = CollectValues(region, state);
    std::string details = Expand(formats.details, values);
    std::string stateLine = Expand(formats.state, values);
    std::string largeText = Expand(formats.largeText, values);
    if (largeText.empty()) largeText = g_config.largeText;

    bool resend = InterlockedExchange(&g_resend, 0) != 0;
    bool changed = details != g_lastDetails || stateLine != g_lastState || largeText != g_lastLargeText;
    if (!resend && !changed) return;
    if (!resend && region == g_lastRegion && GetTickCount() - g_lastSend < kMinUpdateInterval) return;

    DiscordRichPresence presence{};
    presence.details = details.c_str();
    presence.state = stateLine.c_str();
    if (g_config.showElapsedTime) presence.startTimestamp = g_startTime;
    presence.largeImageKey = g_config.largeImage.c_str();
    presence.largeImageText = largeText.c_str();
    Discord_UpdatePresence(&presence);

    g_lastRegion = region;
    g_lastSend = GetTickCount();
    g_lastDetails = details;
    g_lastState = stateLine;
    g_lastLargeText = largeText;
    Log("Presence: %s | %s | %s", details.c_str(), stateLine.c_str(), largeText.c_str());
}

void PresencePoll() {
    if (g_started) Discord_RunCallbacks();
}
