#pragma once
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include "renderer.h"
#include "gamestate.h"
#include "../ls32/ls32cpu.h"
#include "../ls32/ls32dis.h"
struct Terminal {
    std::vector<std::string> output;  // lines of output text
    std::string currentInput;          // what player is typing
    std::string pendingCmd;            // set on Enter, consumed by main
    bool active = false;
    int maxLines = 24;
    void init(GameState* gs = nullptr);
    void handleChar(unsigned int c);   // printable character input
    void handleKey(int key);           // special keys (Enter, Backspace)
    void execute(const std::string& cmd, GameState& gs);
    void draw(Renderer& r, float dt);
    void resize(int w, int h);
    LS32::CPU cpu;
    LS32::Disassembler dis;
    bool cpuLoaded = false;
    GameState* gsPtr = nullptr;

private:
    void print(const std::string& line);
    float blinkTimer = 0.0f;
};