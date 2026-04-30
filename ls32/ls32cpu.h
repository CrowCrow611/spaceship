#pragma once
#include "ls32.h"
#include <vector>
#include <string>

namespace LS32 {

struct CPU {
    // ── registers ─────────────────────────────────────────────
    u32 R[NUM_IREGS] = {};   // integer registers R0–R31
    f64 F[NUM_FREGS] = {};   // float registers   F0–F31
    u32 PC = PROG_START;     // program counter
    u32 FLAGS = 0;           // condition flags

    // ── memory ────────────────────────────────────────────────
    // 512MB is too big to allocate fully — we use a page table.
    // Memory is split into 4KB pages, only allocated on first access.
    // This lets programs use sparse address spaces cheaply.
    static constexpr u32 PAGE_SIZE  = 4096;
    static constexpr u32 PAGE_COUNT = MEM_SIZE / PAGE_SIZE; // 131072 pages
    u8* pages[PAGE_COUNT] = {};  // null = not yet allocated

    // ── I/O ───────────────────────────────────────────────────
    IOReadFn  onRead;    // called when CPU reads  MMIO region
    IOWriteFn onWrite;   // called when CPU writes MMIO region

    // ── status ────────────────────────────────────────────────
    CPUStatus status = CPUStatus::RUNNING;
    u64       cycles = 0;       // total instructions executed
    std::string faultMsg;       // set on FAULT or DIVZERO

    // ── API ───────────────────────────────────────────────────

    // reset all registers and status, free all pages
    void reset();

    // load raw binary into memory at addr, then set PC = addr
    void loadBinary(const u8* data, u32 size, u32 addr = PROG_START);

    // load a vector of already-encoded instructions at addr
    void loadInstrs(const std::vector<u8>& binary, u32 addr = PROG_START);

    // run exactly one instruction, return false if halted/faulted
    bool step();

    // run up to N instructions, stops early if halted/faulted
    // returns number of instructions actually executed
    u32 run(u32 maxCycles);

    // memory access — these go through the page table
    // and trigger MMIO callbacks for the I/O region
    u8  memRead8 (u32 addr);
    u16 memRead16(u32 addr);
    u32 memRead32(u32 addr);
    f32 memReadF32(u32 addr);
    f64 memReadF64(u32 addr);

    void memWrite8 (u32 addr, u8  val);
    void memWrite16(u32 addr, u16 val);
    void memWrite32(u32 addr, u32 val);
    void memWriteF32(u32 addr, f32 val);
    void memWriteF64(u32 addr, f64 val);

    // write a null-terminated string into memory at addr
    void memWriteStr(u32 addr, const std::string& s);

    // read a null-terminated string from memory at addr
    std::string memReadStr(u32 addr);

    // destructor — frees all allocated pages
    ~CPU();

private:
    // fetch + decode the instruction at PC, advance PC
    Instr fetch();

    // execute a decoded instruction
    void  execute(const Instr& ins);

    // stack helpers
    void push32(u32 val);
    u32  pop32();

    // flag helpers
    void setIntFlags(i64 result);  // sets Z N C V
    void setFloatFlags(f64 result); // sets FZ FN FI FU
    void clearIntFlags();
    void clearFloatFlags();

    // page allocator — returns pointer to byte in page table
    // allocates the page if it doesn't exist yet
    u8* pagePtr(u32 addr);

    // fault helper — sets status and message, stops execution
    void fault(const std::string& msg);
};

} // namespace LS32