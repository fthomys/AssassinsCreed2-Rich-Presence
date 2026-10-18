#pragma once

#include <string>

enum Language { German = 0, English = 1 };

struct Config {
    std::string applicationId;
    std::string language = "auto";
    std::string largeImage = "ac2_logo";
    std::string largeText = "Assassin's Creed II";
    bool showElapsedTime = true;
    bool showStats = true;
    bool showProgress = true;
    bool showLandmark = true;
    bool debugLog = false;
    bool dumpProcess = false;
};

extern Config g_config;
extern Language g_language;

void LoadConfig(const std::wstring& path);
