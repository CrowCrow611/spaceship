#pragma once
#include "ls32.h"
#include "ls32cpu.h"
#include <string>
#include <vector>

namespace LS32 {

struct DisResult {
    u32         addr;   // address of this instruction
    u32         size;   // bytes it takes up
    std::string hex;    // raw bytes as hex  e.g. "11 A1 00 00"
    std::string text;   // human readable    e.g. "ADD R6, R2, R3"
};

struct Disassembler {

    // disassemble one instruction at addr in cpu's memory
    // returns the result and advances addr by instruction size
    DisResult disOne(CPU& cpu, u32 addr);

    // disassemble count instructions starting at addr
    std::vector<DisResult> dis(CPU& cpu, u32 addr, u32 count);

    // disassemble and print to stdout — useful for debugging
    void dump(CPU& cpu, u32 addr, u32 count);

    // disassemble around PC — shows context window
    // prints (before) instructions before PC
    // and (after) instructions after PC
    void dumpAround(CPU& cpu, u32 before = 4, u32 after = 4);

    // dump all registers to stdout
    void dumpRegs(CPU& cpu);

    // dump a region of memory as hex + ascii (like xxd)
    void hexDump(CPU& cpu, u32 addr, u32 bytes);

public:
    std::string regName (u8 r);   // "R0" .. "R31", with aliases
    std::string fregName(u8 r);   // "F0" .. "F31"
    std::string immStr  (i32 imm); // decimal + hex side by side
    std::string flagsStr(u32 flags);
};

} // namespace LS32