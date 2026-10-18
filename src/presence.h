#pragma once

#include "game_state.h"

void PresenceStart();
void PresenceUpdate(int region, const GameState& state);
void PresencePoll();
