#include "savegame.h"
#include <fstream>
#include <iostream>
#include <cstdio>

void saveGame(const GameState& gs, const char* path) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "Save failed: could not open " << path << "\n";
        return;
    }
    uint32_t version =2;
    f.write((char*)&version, sizeof(uint32_t));
    f.write((char*)&gs.fuel, sizeof(float));
    f.write((char*)&gs.speed, sizeof(float));
    f.write((char*)&gs.posX, sizeof(float));
    f.write((char*)&gs.posY, sizeof(float));
    f.write((char*)&gs.camPos.x, sizeof(float));
    f.write((char*)&gs.camPos.y, sizeof(float));
    f.write((char*)&gs.camPos.z, sizeof(float));
    f.write((char*)&gs.chairAngle,  sizeof(float));
    std::cout << "Game saved.\n";
}

bool loadGame(GameState& gs, const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "No save file found.\n";
        return false;
    }
    uint32_t version = 0;
    f.read((char*)&version, sizeof(uint32_t));
    if (version != 2) {
        std::cerr << "Save file version mismatch — deleting old save.\n";
        f.close();
        std::remove(path);
        return false;
    }

    f.read((char*)&gs.fuel,       sizeof(float));
    f.read((char*)&gs.speed,      sizeof(float));
    f.read((char*)&gs.posX,       sizeof(float));
    f.read((char*)&gs.posY,       sizeof(float));
    f.read((char*)&gs.camPos.x,   sizeof(float));
    f.read((char*)&gs.camPos.y,   sizeof(float));
    f.read((char*)&gs.camPos.z,   sizeof(float));
    gs.chairTargetAngle = gs.chairAngle;
    gs.status = "OK";
    std::cout << "Game loaded.\n";
    return true;
}