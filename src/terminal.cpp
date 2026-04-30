#include "../lilshit/ls32asm.h"
#include <glad/glad.h>
#include "terminal.h"
#include <algorithm>
#include <string>
#include <cmath>

#include "savegame.h"

static std::string fmt(float v, int d =4) {
    std::string s = std::to_string(std::abs(v));
    return (v < 0 ? "-" : "") + s.substr(0, s.find('.') + d);
}

//colors (change if u want)
static const float GP_BRIGHT[] = {0.15f, 1.00f, 0.25f}; // bright green phosphor
static const float GP_DIM[]    = {0.05f, 0.45f, 0.10f}; // dim green
static const float GP_INPUT[]  = {0.35f, 1.00f, 0.45f}; // input line green

void Terminal::print(const std::string& line) {
    output.push_back(line);
    if ((int)output.size() > maxLines)
        output.erase(output.begin());
}

void Terminal::init(GameState* gs) {
    gsPtr = gs;
    output.clear();
    cpu.onRead = [this](u32 addr) -> u32 {
        u32 port = addr - LS32::MMIO_BASE;
        if (!gsPtr) return 0;
        switch (port) {
        case 0x00: return (u32)gsPtr->fuel;
        case 0x01: return (u32)(gsPtr->speed * 1000);
        case 0x02: return (u32)gsPtr->posX;
        case 0x03: return (u32)gsPtr->posY;
        default: return 0;
        }
    };
    cpu.onWrite = [this](u32 addr, u32 val) {
        u32 port = addr - LS32::MMIO_BASE;
        if (!gsPtr) return;
        switch (port) {
            case 0x01: gsPtr->speed = val / 1000.0f; break;
            case 0x10: {
                //port 0x10 = print string from memory val
                std::string s = cpu.memReadStr(val);
                print(s);
                break;
            }
            case 0x11: {
                //port 0x11 = print integer from memory val directly
                print(std::to_string(val));
                break;
            }
            default: break;
            }
    };
    cpu.reset();

    currentInput = "";
    pendingCmd   = "";
    active       = false;
    print("SHIPBOARD COMPUTER");
    print("URSLA SYSTEMS");
    print("----------------------------");
    print("TYPE 'help' FOR COMMANDS");
    print("");
}

void Terminal::handleChar(unsigned int c) {
    if (!active) return;
    if (c >= 32 && c < 127)
        currentInput += (char)c;
}

void Terminal::handleKey(int key) {
    if (!active) return;
    if (key == GLFW_KEY_ENTER) {
        if (!currentInput.empty()) {
            print("> " + currentInput);
            pendingCmd   = currentInput;
            currentInput = "";
        }
    }
    if (key == GLFW_KEY_BACKSPACE && !currentInput.empty())
        currentInput.pop_back();
}

void Terminal::execute(const std::string& cmd, GameState& gs) {
    std::string c = cmd;
    std::transform(c.begin(), c.end(), c.begin(), ::tolower);

    if (c == "help") {
        print("AVAILABLE COMMANDS:");
        print("  scan        scan nearby space");
        print("  fuel        check fuel level");
        print("  status      ship status report");
        print("  travel      engage engines");
        print("  stop        cut engines");
        print("  coords      show position");
        print("  clear       clear terminal");
        print("  cpu         CPU status");
        print("  dis         disassemble at PC");
        print("  regs        dump all registers");
        print("  run         run until HLT/fault");
        print("  step        execute one instruction");
        print("  cpureset    reset the CPU");
    }
    else if (c == "fuel") {
        print("FUEL LEVEL: " + std::to_string((int)gs.fuel) + "%");
        if      (gs.fuel < 10)  print("!! CRITICAL: FUEL NEARLY EMPTY");
        else if (gs.fuel < 30)  print("! WARNING: FUEL LOW");
        else                    print("FUEL LEVELS NOMINAL");
    }
    else if (c == "status") {
        print("--- SHIP STATUS ---");
        print("STATUS : " + gs.status);
        print("SPEED  : " + fmt(gs.speed));
        print("FUEL   : " + std::to_string((int)gs.fuel) + "%");
        print("ENGINES: " + std::string(gs.speed > 0 ? "ACTIVE" : "IDLE"));
    }
    else if (c == "coords") {
        print("POSITION REPORT:");
        print("  X: " + std::to_string(gs.posX));
        print("  Y: " + std::to_string(gs.posY));
    }
    else if (c == "scan") {
        print("SCANNING SECTOR...");
        print("NO CONTACTS DETECTED");
        print("ANOMALIES : NONE");
        print("SIGNALS   : NONE");
        print("SECTOR    : VOID SPACE");
    }
    else if (c == "travel") {
        gs.speed  = 1.0f;
        gs.starSpeed = 1.0f;
        gs.status = "OK";
        print("ENGINES ENGAGED");
        print("SPEED: 1.0 | FUEL DRAIN ACTIVE");
    }
    else if (c == "stop") {
        gs.speed     = 0.0f;
        gs.starSpeed = 0.0f;
        print("ENGINES OFFLINE");
        print("SHIP STATIONARY");
    }
    else if (c == "save") {
        saveGame(gs);
        print("GAME SAVED.");
    }
    else if (c == "load") {
        if (loadGame(gs)) print("GAME LOADED.");
        else print("NO SAVE FILE FOUND.");
    }
    else if (c == "clear") {
        output.clear();
    }
    else if (c == "cpu") {
        print("── LS32 CPU STATUS ──");
        char pcbuf[32];
        snprintf(pcbuf, sizeof(pcbuf), "PC     : 0x%08X", cpu.PC);
        print(pcbuf);
        print("CYCLES : " + std::to_string(cpu.cycles));
        std::string st;
        switch(cpu.status) {
        case LS32::CPUStatus::RUNNING: st="RUNNING"; break;
        case LS32::CPUStatus::HALTED:  st="HALTED";  break;
        case LS32::CPUStatus::FAULT:   st="FAULT";   break;
        case LS32::CPUStatus::DIVZERO: st="DIV/ZERO";break;
        }
        print("STATUS : " + st);
        if (!cpu.faultMsg.empty())
            print("FAULT  : " + cpu.faultMsg);
    }
    else if (c == "regs") {
        print("── INTEGER REGISTERS ──");
        for (int i = 0; i < LS32::NUM_IREGS; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "  R%-2d = %11d  (0x%08X)",
                     i, (i32)cpu.R[i], cpu.R[i]);
            print(buf);
        }
        print("── FLOAT REGISTERS ──");
        for (int i = 0; i < LS32::NUM_FREGS; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "  F%-2d = %f", i, cpu.F[i]);
            print(buf);
        }
    }
    else if (c == "dis") {
        auto results = dis.dis(cpu, cpu.PC, 10);
        print("── DISASSEMBLY @ PC ──");
        for (auto& r : results) {
            char buf[80];
            snprintf(buf, sizeof(buf), "  %08X  %-20s  %s",
                     r.addr, r.hex.c_str(), r.text.c_str());
            print(buf);
        }
    }
    else if (c == "cpureset") {
        cpu.reset();
        cpuLoaded = false;
        print("CPU RESET.");
    }
    else if (c == "run") {
        if (!cpuLoaded) {
            print("NO PROGRAM LOADED.");
        } else {
            u32 ran = cpu.run(100000);
            print("RAN " + std::to_string(ran) + " INSTRUCTIONS.");
            std::string st;
            switch(cpu.status) {
            case LS32::CPUStatus::RUNNING: st="RUNNING"; break;
            case LS32::CPUStatus::HALTED:  st="HALTED";  break;
            case LS32::CPUStatus::FAULT:   st="FAULT";   break;
            case LS32::CPUStatus::DIVZERO: st="DIV/ZERO";break;
            }
            print("STATUS: " + st);
            if (!cpu.faultMsg.empty())
                print("FAULT: " + cpu.faultMsg);
        }
    }
    else if (c == "hexdump") {
        print("── MEMORY @ 0x100 ──");
        for (u32 i = 0; i < 25; i++) {
            char buf[32];
            u8 byte = cpu.memRead8(0x100 + i);
            snprintf(buf, sizeof(buf), "  [%03X] = 0x%02X", 0x100 + i, byte);
            print(buf);
        }
    }
    else if (c == "step") {
    if (!cpuLoaded) {
        print("NO PROGRAM LOADED.");
    } else {
        auto r = dis.disOne(cpu, cpu.PC);
        cpu.step();
        print("EXEC: " + r.text);
        char buf[32];
        snprintf(buf, sizeof(buf), "PC -> 0x%08X", cpu.PC);
        print(buf);
    }
}
    
    else if (c == "loadtest") {
    // hand-encoded test program:
    // MOVI R6, 42       — Format C: op=0x50, rd=6, ra=0, imm=42
    // MOVI R7, 58       — Format C: op=0x50, rd=7, ra=0, imm=58
    // ADD  R1, R6, R7   — Format B: op=0x11, rd=1, ra=6, rb=7
    // HLT               — Format A: op=0x01

    std::vector<u8> prog;

    // MOVI R6, 42
    // fetch reads: op=byte[0], rd=(word>>8)&0x1F=byte[1] bits4:0
    prog.push_back(0x50); // op = MOVI
    prog.push_back(0x06); // rd = 6
    prog.push_back(0x00); // ra = 0, upper bits = 0
    prog.push_back(0x00); // padding
    prog.push_back(42);   // imm low byte
    prog.push_back(0x00);
    prog.push_back(0x00);
    prog.push_back(0x00);

    // MOVI R7, 58
    prog.push_back(0x50); // op = MOVI
    prog.push_back(0x07); // rd = 7
    prog.push_back(0x00);
    prog.push_back(0x00);
    prog.push_back(58);   // imm low byte
    prog.push_back(0x00);
    prog.push_back(0x00);
    prog.push_back(0x00);

    // ADD R1, R6, R7
    // fetch reads: rd=(word>>8)&0x1F, ra=(word>>13)&0x1F, rb=(word>>18)&0x1F
    // word = op | (1<<8) | (6<<13) | (7<<18)
    //      = 0x11 | 0x100 | 0xC000 | 0x1C0000
    //      = 0x001CC111
    // ADD R1, R6, R7
    u32 addWord = 0x11u | (1u << 8) | (6u << 13) | (7u << 18);
    prog.push_back((addWord >>  0) & 0xFF);
    prog.push_back((addWord >>  8) & 0xFF);
    prog.push_back((addWord >> 16) & 0xFF);
    prog.push_back((addWord >> 24) & 0xFF);

    // HLT
    prog.push_back(0x01);

    cpu.reset();
    cpu.loadInstrs(prog);
    cpuLoaded = true;
    print("TEST PROGRAM LOADED.");
    print("3 INSTRUCTIONS + HLT");
    print("EXPECTED: R1 = 100 (42 + 58)");
    print("TYPE 'step' 4 TIMES THEN 'regs'");
    }

    else if (c == "r1") {
    i32 signed_val = (i32)cpu.R[1];
    char buf[64];
    snprintf(buf, sizeof(buf), "R1 = %d  (unsigned: %u  hex: 0x%08X)",
             signed_val, cpu.R[1], cpu.R[1]);
    print(buf);
    }


    else if (c.substr(0, 5) == "load ") {
    std::string path = cmd.substr(5);
    auto r = LS32::assembleFile(path);
    if (!r.ok) {
        print("ASM ERROR line " + std::to_string(r.error.line)
              + ": " + r.error.msg);
    } else {
        cpu.reset();
        cpu.loadInstrs(r.binary);
        cpuLoaded = true;
        print("LOADED " + std::to_string(r.instrCount)
              + " INSTRUCTIONS (" + std::to_string(r.binary.size())
              + " bytes)");
    }
}
else if (c.substr(0, 4) == "asm ") {
    std::string src = cmd.substr(4);
    auto r = LS32::assemble(src);
    if (!r.ok) {
        print("ASM ERROR: " + r.error.msg);
    } else {
        cpu.reset();
        cpu.loadInstrs(r.binary);
        cpuLoaded = true;
        print("OK — " + std::to_string(r.instrCount) + " instruction(s)");
    }
}

    else {
        print("UNKNOWN: " + cmd);
        print("TYPE 'help' FOR COMMANDS");
    }
}


void Terminal::draw(Renderer& r, float dt) {
    if (!active) return;

    float W   = (float)r.W;
    float H   = (float)r.H;
    float pad = 50.0f;
    float lineH = 26.0f;
    maxLines = (int)((H - pad * 2 - 120.0f) / lineH);

    //FULLSCREEN DARK GREEN BG
    r.rect(0, 0, W, H, 0.01f, 0.04f, 0.01f, 0.97f);

    //crt scanline
    for (float y = 0; y < H; y += 4)
        r.rect(0, y, W, 1.5f, 0.0f, 0.0f, 0.0f, 0.12f);

    //outer border
    r.rect(pad-4, H-pad-4,  W-pad*2+8, 4,    GP_DIM[0],GP_DIM[1],GP_DIM[2],1); // top
    r.rect(pad-4, pad,      W-pad*2+8, 4,    GP_DIM[0],GP_DIM[1],GP_DIM[2],1); // bottom
    r.rect(pad-4, pad,      4, H-pad*2,      GP_DIM[0],GP_DIM[1],GP_DIM[2],1); // left
    r.rect(W-pad, pad,      4, H-pad*2,      GP_DIM[0],GP_DIM[1],GP_DIM[2],1); // right

    //header
    r.text("SHIPBOARD TERMINAL", pad+12, H-pad-28, 1.5f, GP_BRIGHT[0],GP_BRIGHT[1],GP_BRIGHT[2]);
    r.rect(pad, H-pad-40, W-pad*2, 2, GP_DIM[0],GP_DIM[1],GP_DIM[2], 1.0f);

    //outer lines
    float startY = H - pad - 72.0f;
    int n = (int)output.size();
    for (int i = 0; i < n; i++) {
    float y = startY - (float)(n - 1 - i) * lineH;
        if (y < pad + 60) break;
        r.text(output[i], pad+12, y, 1.0f, GP_BRIGHT[0],GP_BRIGHT[1],GP_BRIGHT[2]);
    }

    //input seperator
    r.rect(pad, pad+36, W-pad*2, 2, GP_DIM[0],GP_DIM[1],GP_DIM[2], 1.0f);

    //input line with blinking cursor
    blinkTimer += dt;
    std::string inputLine = "> " + currentInput;
    if (fmod(blinkTimer, 1.0f) < 0.5f) inputLine +="_";
    r.text(inputLine, pad+12, pad+28, 1.0f, GP_INPUT[0],GP_INPUT[1],GP_INPUT[2]);

    //esc hint
    r.text("[ESC] CLOSE", W-160, pad+28, 0.9f, GP_DIM[0],GP_DIM[1],GP_DIM[2]);
}

void Terminal::resize(int w, int h) {
    maxLines = (int)((h - 50*2 - 120.0f) / 26.0f);
}