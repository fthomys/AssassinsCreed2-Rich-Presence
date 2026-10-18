#pragma once

#include <string>

struct GameState {
    bool hasStats = false;
    int florins = 0;
    int health = 0;
    int healthMax = 0;
    int healthBars = 0;
    int healthBarsMax = 0;
    float x = 0, y = 0, z = 0;
    std::string landmark;

    bool hasProgress = false;
    bool inMission = false;
    int sequence = 0;
    int memory = 0;
    int missionState = 0;
};

bool IsSupportedExecutable();
void ReadGameState(GameState& state);
void InvalidateGameObjects();
