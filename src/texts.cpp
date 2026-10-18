#include "texts.h"

#include <cstring>
#include <cwchar>
#include <vector>

namespace {

struct Region {
    const wchar_t* archive;
    const char* key;
    Text text[2];
};

const char* kEzio = "Ezio Auditore da Firenze";
const char* kDesmond = "Desmond Miles, 2012";

std::vector<Region> g_regions = {
    {L"DataPC_TitleScreen.forge", "TitleScreen", {{"Im Hauptmenü", ""}, {"In the main menu", ""}}},
    {L"DataPC_WhiteRoom.forge", "WhiteRoom", {{"Im Animus", ""}, {"In the Animus", ""}}},
    {L"DataPC_Abstergo.forge", "Abstergo", {{"Abstergo Industries", kDesmond}, {"Abstergo Industries", kDesmond}}},
    {L"DataPC_Hideout.forge", "Hideout", {{"Im Versteck der Assassinen", kDesmond}, {"In the Assassin hideout", kDesmond}}},
    {L"DataPC_Firenze.forge", "Firenze", {{"In Florenz", kEzio}, {"In Florence", kEzio}}},
    {L"DataPC_Toscana.forge", "Toscana", {{"In der Toskana", kEzio}, {"In Tuscany", kEzio}}},
    {L"DataPC_Villa.forge", "Villa", {{"In Monteriggioni", kEzio}, {"In Monteriggioni", kEzio}}},
    {L"DataPC_Venezia.forge", "Venezia", {{"In Venedig", kEzio}, {"In Venice", kEzio}}},
    {L"DataPC_Roma.forge", "Roma", {{"In Rom", kEzio}, {"In Rome", kEzio}}},
    {L"DataPC_Wetlands.forge", "Wetlands", {{"In der Romagna (Forlì)", kEzio}, {"In Romagna (Forlì)", kEzio}}},
    {L"DataPC_Mountains.forge", "Mountains", {{"In den Apenninen", kEzio}, {"In the Apennines", kEzio}}},
    {L"DataPC_Altair_Memory.forge", "Altair_Memory", {{"In Altaïrs Erinnerung", "Altaïr Ibn-La'Ahad"}, {"In Altaïr's memory", "Altaïr Ibn-La'Ahad"}}},
    {L"DataPC_LGS01_Duomo.forge", "LGS01_Duomo", {{"Gruft: Santa Maria del Fiore", "Florenz"}, {"Tomb: Santa Maria del Fiore", "Florence"}}},
    {L"DataPC_LGS02_Palazzo_Medic.forge", "LGS02_Palazzo_Medic", {{"Palazzo Medici", "Florenz"}, {"Palazzo Medici", "Florence"}}},
    {L"DataPC_LGS03_Gimignano.forge", "LGS03_Gimignano", {{"Gruft: Torre Grossa", "San Gimignano"}, {"Tomb: Torre Grossa", "San Gimignano"}}},
    {L"DataPC_LGS04_Novella.forge", "LGS04_Novella", {{"Gruft: Santa Maria Novella", "Florenz"}, {"Tomb: Santa Maria Novella", "Florence"}}},
    {L"DataPC_LGS05_Forli_Citadel.forge", "LGS05_Forli_Citadel", {{"Gruft: Rocca di Ravaldino", "Forlì"}, {"Tomb: Rocca di Ravaldino", "Forlì"}}},
    {L"DataPC_LGS06_Frari.forge", "LGS06_Frari", {{"Santa Maria Gloriosa dei Frari", "Venedig"}, {"Santa Maria Gloriosa dei Frari", "Venice"}}},
    {L"DataPC_LGS07_San_Marco.forge", "LGS07_San_Marco", {{"Gruft: Markusdom", "Venedig"}, {"Tomb: St. Mark's Basilica", "Venice"}}},
    {L"DataPC_LGS08_ArsenalShipya.forge", "LGS08_ArsenalShipya", {{"Arsenale di Venezia", "Venedig"}, {"Arsenale di Venezia", "Venice"}}},
    {L"DataPC_LGS09_Monteregionni.forge", "LGS09_Monteregionni", {{"Gruft der Familie Auditore", "Monteriggioni"}, {"Auditore family crypt", "Monteriggioni"}}},
    {L"DataPC_LGS10_Visitazione.forge", "LGS10_Visitazione", {{"Gruft: Santa Maria della Visitazione", "Venedig"}, {"Tomb: Santa Maria della Visitazione", "Venice"}}},
};

Text g_idle[2] = {{"Im Animus", ""}, {"In the Animus", ""}};

LineFormats g_formats[2] = {
    {"{region} · Sequenz {sequence} · Nahe {landmark}",
     "Erinnerung {memory} · {exploring} · {florins} ƒ · ❤ {health}/{health_max}", "{region_info}"},
    {"{region} · Sequence {sequence} · Near {landmark}",
     "Memory {memory} · {exploring} · {florins} ƒ · ❤ {health}/{health_max}", "{region_info}"},
};

std::string g_exploring[2] = {"Freies Erkunden", "Free roaming"};

const wchar_t* kSharedArchives[] = {L"DataPC.forge", L"DataPC_extra.forge", L"DataPC_extra_dlc.forge"};

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

Text ParseText(const std::string& value) {
    size_t bar = value.find('|');
    return {Trim(value.substr(0, bar)), bar == std::string::npos ? "" : Trim(value.substr(bar + 1))};
}

}

int RegionCount() {
    return (int)g_regions.size();
}

int FindRegionByArchive(const wchar_t* fileName) {
    for (size_t i = 0; i < g_regions.size(); i++)
        if (_wcsicmp(fileName, g_regions[i].archive) == 0) return (int)i;
    return -1;
}

bool IsSharedArchive(const wchar_t* fileName) {
    for (const wchar_t* shared : kSharedArchives)
        if (_wcsicmp(fileName, shared) == 0) return true;
    return false;
}

const char* RegionKey(int region) {
    return region >= 0 ? g_regions[region].key : "Idle";
}

const Text& RegionText(int region, Language language) {
    return region >= 0 ? g_regions[region].text[language] : g_idle[language];
}

const LineFormats& Formats(Language language) {
    return g_formats[language];
}

const std::string& ExploringText(Language language) {
    return g_exploring[language];
}

void OverrideText(Language language, const std::string& key, const std::string& value) {
    LineFormats& formats = g_formats[language];
    if (_stricmp(key.c_str(), "Details") == 0) formats.details = value;
    else if (_stricmp(key.c_str(), "State") == 0) formats.state = value;
    else if (_stricmp(key.c_str(), "LargeText") == 0) formats.largeText = value;
    else if (_stricmp(key.c_str(), "Exploring") == 0) g_exploring[language] = value;
    else if (_stricmp(key.c_str(), "Idle") == 0) g_idle[language] = ParseText(value);
    else
        for (Region& r : g_regions)
            if (_stricmp(key.c_str(), r.key) == 0) r.text[language] = ParseText(value);
}
