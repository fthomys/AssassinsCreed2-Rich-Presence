#pragma once

#include <string>

#include "config.h"

struct Text {
    std::string name;
    std::string info;
};

struct LineFormats {
    std::string details;
    std::string state;
    std::string largeText;
};

int RegionCount();
int FindRegionByArchive(const wchar_t* fileName);
bool IsSharedArchive(const wchar_t* fileName);
const char* RegionKey(int region);
const Text& RegionText(int region, Language language);
const LineFormats& Formats(Language language);
const std::string& ExploringText(Language language);
void OverrideText(Language language, const std::string& key, const std::string& value);
