#pragma once
#include "gamestate.h"

void saveGame(const GameState& gs, const char* path = "save.dat");
bool loadGame(GameState& gs, const char* path = "save.dat");