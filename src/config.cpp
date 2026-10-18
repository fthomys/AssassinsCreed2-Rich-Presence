#include "config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

#include "texts.h"

Config g_config;
Language g_language = English;

namespace {

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool Is(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

bool ReadTextFile(const std::wstring& path, std::string& data) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    fclose(f);
    if (data.compare(0, 3, "\xEF\xBB\xBF") == 0) data.erase(0, 3);
    return true;
}

void ApplySetting(const std::string& key, const std::string& value) {
    if (Is(key, "ApplicationId")) g_config.applicationId = value;
    else if (Is(key, "Language")) g_config.language = value;
    else if (Is(key, "LargeImage")) g_config.largeImage = value;
    else if (Is(key, "LargeText")) g_config.largeText = value;
    else if (Is(key, "ShowElapsedTime")) g_config.showElapsedTime = value != "0";
    else if (Is(key, "ShowStats")) g_config.showStats = value != "0";
    else if (Is(key, "ShowProgress")) g_config.showProgress = value != "0";
    else if (Is(key, "ShowLandmark")) g_config.showLandmark = value != "0";
    else if (Is(key, "DebugLog")) g_config.debugLog = value != "0";
    else if (Is(key, "DumpProcess")) g_config.dumpProcess = value != "0";
}

Language ResolveLanguage() {
    if (Is(g_config.language, "de")) return German;
    if (Is(g_config.language, "en")) return English;
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN ? German : English;
}

}

void LoadConfig(const std::wstring& path) {
    std::string data;
    if (ReadTextFile(path, data)) {
        std::string section;
        size_t pos = 0;
        while (pos < data.size()) {
            size_t end = data.find('\n', pos);
            if (end == std::string::npos) end = data.size();
            std::string line = Trim(data.substr(pos, end - pos));
            pos = end + 1;
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line[0] == '[') {
                section = Trim(line.substr(1, line.find(']') - 1));
                continue;
            }
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));

            if (Is(section, "DiscordPresenceAC2")) ApplySetting(key, value);
            else if (Is(section, "Text_DE") && !value.empty()) OverrideText(German, key, value);
            else if (Is(section, "Text_EN") && !value.empty()) OverrideText(English, key, value);
        }
    }
    g_language = ResolveLanguage();
}
