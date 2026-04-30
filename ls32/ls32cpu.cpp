#include "ls32cpu.h"
#include <cmath>
#include <cstring>
#include <bit>
#include <sstream>
#include <iomanip>

namespace LS32 {

// helpers 

static std::string hexStr(u32 v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << v;
    return ss.str();
}

// page table memory

u8* CPU::pagePtr(u32 addr) {
    if (addr >= MEM_SIZE) {
        fault("address out of range: " + hexStr(addr));
        return nullptr;
    }
    u32 pageIdx = addr / PAGE_SIZE;
    if (!pages[pageIdx]) {
        pages[pageIdx] = new u8[PAGE_SIZE]();  // zero-initialised
    }
    return pages[pageIdx] + (addr % PAGE_SIZE);
}

CPU::~CPU() {
    for (auto* p : pages)
        delete[] p;
}

void CPU::reset() {
    for (auto* p : pages) { delete[] p; p = nullptr; }
    std::memset(R, 0, sizeof(R));
    std::memset(F, 0, sizeof(F));
    PC     = PROG_START;
    FLAGS  = 0;
    cycles = 0;
    status = CPUStatus::RUNNING;
    faultMsg.clear();
    R[REG_SP] = STACK_TOP;
}

// memory read/write

// MMIO region check
static bool isMMIO(u32 addr) {
    return addr >= MMIO_BASE && addr <= MMIO_END;
}

u8 CPU::memRead8(u32 addr) {
    if (isMMIO(addr)) {
        return onRead ? (u8)(onRead(addr) & 0xFF) : 0;
    }
    u8* p = pagePtr(addr);
    return p ? *p : 0;
}

u16 CPU::memRead16(u32 addr) {
    // little-endian: low byte first
    return (u16)memRead8(addr) | ((u16)memRead8(addr+1) << 8);
}

u32 CPU::memRead32(u32 addr) {
    if (isMMIO(addr)) {
        return onRead ? onRead(addr) : 0;
    }
    return  (u32)memRead8(addr)        |
           ((u32)memRead8(addr+1) << 8)  |
           ((u32)memRead8(addr+2) << 16) |
           ((u32)memRead8(addr+3) << 24);
}

f32 CPU::memReadF32(u32 addr) {
    u32 bits = memRead32(addr);
    f32 val;
    std::memcpy(&val, &bits, 4);
    return val;
}

f64 CPU::memReadF64(u32 addr) {
    u32 lo = memRead32(addr);
    u32 hi = memRead32(addr + 4);
    u64 bits = (u64)lo | ((u64)hi << 32);
    f64 val;
    std::memcpy(&val, &bits, 8);
    return val;
}

void CPU::memWrite8(u32 addr, u8 val) {
    if (isMMIO(addr)) {
        if (onWrite) onWrite(addr, val);
        return;
    }
    u8* p = pagePtr(addr);
    if (p) *p = val;
}

void CPU::memWrite16(u32 addr, u16 val) {
    memWrite8(addr,   val & 0xFF);
    memWrite8(addr+1, (val >> 8) & 0xFF);
}

void CPU::memWrite32(u32 addr, u32 val) {
    if (isMMIO(addr)) {
        if (onWrite) onWrite(addr, val);
        return;
    }
    memWrite8(addr,   val & 0xFF);
    memWrite8(addr+1, (val >> 8)  & 0xFF);
    memWrite8(addr+2, (val >> 16) & 0xFF);
    memWrite8(addr+3, (val >> 24) & 0xFF);
}

void CPU::memWriteF32(u32 addr, f32 val) {
    u32 bits;
    std::memcpy(&bits, &val, 4);
    memWrite32(addr, bits);
}

void CPU::memWriteF64(u32 addr, f64 val) {
    u64 bits;
    std::memcpy(&bits, &val, 8);
    memWrite32(addr,   (u32)(bits & 0xFFFFFFFF));
    memWrite32(addr+4, (u32)(bits >> 32));
}

void CPU::memWriteStr(u32 addr, const std::string& s) {
    for (char c : s) memWrite8(addr++, (u8)c);
    memWrite8(addr, 0); // null terminator
}

std::string CPU::memReadStr(u32 addr) {
    std::string s;
    u32 limit = 4096; // safety cap
    while (limit--) {
        u8 c = memRead8(addr++);
        if (c == 0) break;
        s += (char)c;
    }
    return s;
}

// load binary

void CPU::loadBinary(const u8* data, u32 size, u32 addr) {
    for (u32 i = 0; i < size; i++)
        memWrite8(addr + i, data[i]);
    PC = addr;
}

void CPU::loadInstrs(const std::vector<u8>& binary, u32 addr) {
    loadBinary(binary.data(), (u32)binary.size(), addr);
}

// stack 

void CPU::push32(u32 val) {
    R[REG_SP] -= 4;
    memWrite32(R[REG_SP], val);
}

u32 CPU::pop32() {
    u32 val = memRead32(R[REG_SP]);
    R[REG_SP] += 4;
    return val;
}

// ── flags ─────────────────────────────────────────────────────

void CPU::setIntFlags(i64 result) {
    FLAGS &= ~(FL_Z | FL_N | FL_C | FL_V);
    if ((result & 0xFFFFFFFF) == 0) FLAGS |= FL_Z;
    if (result < 0)                  FLAGS |= FL_N;
    if (result > 0xFFFFFFFF)         FLAGS |= FL_C;
    if (result > 0x7FFFFFFF || result < -(i64)0x80000000)
        FLAGS |= FL_V;
}

void CPU::clearIntFlags() {
    FLAGS &= ~(FL_Z | FL_N | FL_C | FL_V);
}

void CPU::setFloatFlags(f64 result) {
    FLAGS &= ~(FL_FZ | FL_FN | FL_FI | FL_FU);
    if (result == 0.0)          FLAGS |= FL_FZ;
    if (result < 0.0)           FLAGS |= FL_FN;
    if (std::isinf(result))     FLAGS |= FL_FI;
    if (std::isnan(result))     FLAGS |= FL_FU;
}

void CPU::clearFloatFlags() {
    FLAGS &= ~(FL_FZ | FL_FN | FL_FI | FL_FU);
}

void CPU::fault(const std::string& msg) {
    status   = CPUStatus::FAULT;
    faultMsg = msg + " (PC=" + hexStr(PC) + ")";
}

// ── fetch ─────────────────────────────────────────────────────

Instr CPU::fetch() {
    Instr ins;
    u8 opByte = memRead8(PC);
    ins.op  = (Op)opByte;
    ins.fmt = fmtOf(ins.op);

    switch (ins.fmt) {
    case Fmt::A:
        // [ op:8 ]
        PC += 1;
        break;

    case Fmt::B: {
    u32 word = memRead32(PC);
    // opcode is bits 7:0, so fields shift up from bit 8
    ins.rd   = (word >>  8) & 0x1F;
    ins.ra   = (word >> 13) & 0x1F;
    ins.rb   = (word >> 18) & 0x1F;
    ins.func = (word >> 23) & 0x1FF;
    PC += 4;
    break;
}
case Fmt::C: {
    u32 h   = memRead32(PC);
    ins.rd  = (h >>  8) & 0x1F;
    ins.ra  = (h >> 13) & 0x1F;
    ins.imm = (i32)memRead32(PC + 4);
    PC += 8;
    break;
}
case Fmt::D: {
    u32 h   = memRead32(PC);
    ins.rd  = (h >>  8) & 0x1F;
    ins.ra  = (h >> 13) & 0x1F;
    i16 o14 = (i16)((h >> 18) & 0x3FFF);
    i32 ext = (i32)memRead32(PC + 4);
    ins.imm = ext != 0 ? ext : (i32)o14;
    PC += 8;
    break;
}
    }

    return ins;
}

// execute

void CPU::execute(const Instr& ins) {
    // R0 is always zero — enforce before every instruction
    R[REG_ZERO] = 0;
    // FLAGS register is read-only from software
    // we store flags separately and only expose via R[REG_FL]
    R[REG_FL] = FLAGS;

    u8 rd = ins.rd % NUM_IREGS;
    u8 ra = ins.ra % NUM_IREGS;
    u8 rb = ins.rb % NUM_IREGS;

    switch (ins.op) {

    // Format A
    case Op::OP_NOP:  break;
    case Op::OP_HLT:  status = CPUStatus::HALTED; break;
    case Op::OP_RETL: PC = R[REG_LR]; break;
    case Op::OP_RET:  PC = pop32(); break;
    case Op::OP_SRET: PC = pop32(); break; // software interrupt return

    // ── Format B — integer ALU ────────────────────────────────
    case Op::OP_MOV:  R[rd] = R[ra]; break;

    case Op::OP_ADD: {
        i64 r = (i64)R[ra] + R[rb];
        setIntFlags(r); R[rd] = (u32)(r & 0xFFFFFFFF);
        break;
    }
    case Op::OP_SUB: {
    i64 r = (i64)R[ra] - R[rb];
    setIntFlags(r); R[rd] = (u32)(r & 0xFFFFFFFF);
    break;
    }
    case Op::OP_MUL: {
        i64 r = (i64)(i32)R[ra] * (i32)R[rb];
        R[rd] = (u32)(r & 0xFFFFFFFF);
        break;
    }
    case Op::OP_MULH: {
        i64 r = (i64)(i32)R[ra] * (i32)R[rb];
        R[rd] = (u32)((r >> 32) & 0xFFFFFFFF);
        break;
    }
    case Op::OP_MULHU: {
        u64 r = (u64)R[ra] * R[rb];
        R[rd] = (u32)((r >> 32) & 0xFFFFFFFF);
        break;
    }
    case Op::OP_DIV: {
        if (R[rb] == 0) { status = CPUStatus::DIVZERO;
                          faultMsg = "DIV by zero"; break; }
        R[rd] = (u32)((i32)R[ra] / (i32)R[rb]);
        break;
    }
    case Op::OP_DIVU: {
        if (R[rb] == 0) { status = CPUStatus::DIVZERO;
                          faultMsg = "DIVU by zero"; break; }
        R[rd] = R[ra] / R[rb];
        break;
    }
    case Op::OP_REM: {
        if (R[rb] == 0) { status = CPUStatus::DIVZERO;
                          faultMsg = "REM by zero"; break; }
        R[rd] = (u32)((i32)R[ra] % (i32)R[rb]);
        break;
    }
    case Op::OP_REMU: {
        if (R[rb] == 0) { status = CPUStatus::DIVZERO;
                          faultMsg = "REMU by zero"; break; }
        R[rd] = R[ra] % R[rb];
        break;
    }
    case Op::OP_MAC: {
        // Rd += Ra * Rb
        i64 r = (i64)R[rd] + (i64)(i32)R[ra] * (i32)R[rb];
        R[rd] = (u32)(r & 0xFFFFFFFF);
        break;
    }
    case Op::OP_BAND: R[rd] = R[ra] & R[rb]; setIntFlags(R[rd]); break;
    case Op::OP_BOR:  R[rd] = R[ra] | R[rb]; setIntFlags(R[rd]); break;
    case Op::OP_BXOR: R[rd] = R[ra] ^ R[rb]; setIntFlags(R[rd]); break;
    case Op::OP_BNOT: R[rd] = ~R[ra];         setIntFlags(R[rd]); break;
    case Op::OP_NEG: R[rd] = (u32)(-(i32)R[ra]); setIntFlags(R[rd]); break;

    case Op::OP_SHL: R[rd] = R[ra] << (R[rb] & 31); break;
    case Op::OP_SHR: R[rd] = R[ra] >> (R[rb] & 31); break;
    case Op::OP_SAR: R[rd] = (u32)((i32)R[ra] >> (R[rb] & 31)); break;
    
    case Op::OP_ROL: {
        u32 s = R[rb] & 31;
        R[rd] = s ? (R[ra] << s) | (R[ra] >> (32 - s)) : R[ra];
        break;
    }
    case Op::OP_ROR: {
        u32 s = R[rb] & 31;
        R[rd] = s ? (R[ra] >> s) | (R[ra] << (32 - s)) : R[ra];
        break;
    }

    case Op::OP_CLZ:  R[rd] = R[ra] ? __builtin_clz(R[ra])  : 32; break;
    case Op::OP_CTZ:  R[rd] = R[ra] ? __builtin_ctz(R[ra])  : 32; break;
    case Op::OP_POPC: R[rd] = __builtin_popcount(R[ra]); break;
    case Op::OP_BSWAP:R[rd] = __builtin_bswap32(R[ra]);  break;

    case Op::OP_CMP: {
        i64 r = (i64)(i32)R[ra] - (i32)R[rb];
        setIntFlags(r);
        break;
    }
    case Op::OP_CMPU: {
        // unsigned compare — use carry flag for borrow
        FLAGS &= ~(FL_Z | FL_N | FL_C);
        if (R[ra] == R[rb]) FLAGS |= FL_Z;
        if (R[ra] <  R[rb]) FLAGS |= FL_C; // borrow = "less than"
        if (R[ra] <  R[rb]) FLAGS |= FL_N;
        break;
    }

    // ── Format B — register jumps ─────────────────────────────
    case Op::OP_JMPR:  PC = R[ra]; break;
    case Op::OP_CALLR: R[REG_LR] = PC; PC = R[ra]; break;

    // ── Format B — float f32 ──────────────────────────────────
    case Op::OP_FMOV32:  F[rd] = (f32)F[ra]; break;
    case Op::OP_FADD32: { f32 r = (f32)F[ra] + (f32)F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FSUB32: { f32 r = (f32)F[ra] - (f32)F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FMUL32: { f32 r = (f32)F[ra] * (f32)F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FDIV32: { f32 r = (f32)F[ra] / (f32)F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FNEG32: F[rd] = -(f32)F[ra]; break;
    case Op::OP_FABS32: F[rd] = std::abs((f32)F[ra]); break;
    case Op::OP_FSQRT32:F[rd] = std::sqrt((f32)F[ra]); break;
    case Op::OP_FMIN32: F[rd] = std::min((f32)F[ra], (f32)F[rb]); break;
    case Op::OP_FMAX32: F[rd] = std::max((f32)F[ra], (f32)F[rb]); break;
    case Op::OP_FMADD32:{ // Fd = Fa*Fb + Fc, Fc index in func field
        u8 fc = ins.func % NUM_FREGS;
        f32 r = (f32)F[ra] * (f32)F[rb] + (f32)F[fc];
        F[rd] = r; setFloatFlags(r);
        break;
    }
    case Op::OP_FCMP32: { f32 r = (f32)F[ra] - (f32)F[rb]; setFloatFlags(r); break; }
    case Op::OP_FCVT32I: R[rd] = (u32)(i32)(f32)F[ra]; break;
    case Op::OP_FICVT32: F[rd] = (f32)(i32)R[ra]; break;

    // Format B — float f64
    case Op::OP_FMOV64:  F[rd] = F[ra]; break;
    case Op::OP_FADD64: { f64 r = F[ra] + F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FSUB64: { f64 r = F[ra] - F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FMUL64: { f64 r = F[ra] * F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FDIV64: { f64 r = F[ra] / F[rb]; F[rd]=r; setFloatFlags(r); break; }
    case Op::OP_FNEG64: F[rd] = -F[ra]; break;
    case Op::OP_FABS64: F[rd] = std::abs(F[ra]); break;
    case Op::OP_FSQRT64:F[rd] = std::sqrt(F[ra]); break;
    case Op::OP_FMIN64: F[rd] = std::min(F[ra], F[rb]); break;
    case Op::OP_FMAX64: F[rd] = std::max(F[ra], F[rb]); break;
    case Op::OP_FMADD64:{ 
        u8 fc = ins.func % NUM_FREGS;
        f64 r = F[ra] * F[rb] + F[fc];
        F[rd] = r; setFloatFlags(r);
        break;
    }
    case Op::OP_FCMP64: { f64 r = F[ra] - F[rb]; setFloatFlags(r); break; }
    case Op::OP_FCVT64I: R[rd] = (u32)(i32)F[ra]; break;
    case Op::OP_FICVT64: F[rd] = (f64)(i32)R[ra]; break;
    case Op::OP_F32TO64: F[rd] = (f64)(f32)F[ra]; break;
    case Op::OP_F64TO32: F[rd] = (f32)F[ra]; break;

    // Format C — immediate integer
    case Op::OP_MOVI: R[rd] = (u32)ins.imm; break;
    case Op::OP_ADDI: {
        i64 r = (i64)R[ra] + ins.imm;
        setIntFlags(r); R[rd] = (u32)(r & 0xFFFFFFFF);
        break;
    }
    case Op::OP_ANDI:  R[rd] = R[ra] & (u32)ins.imm; break;
    case Op::OP_ORI:   R[rd] = R[ra] | (u32)ins.imm; break;
    case Op::OP_XORI:  R[rd] = R[ra] ^ (u32)ins.imm; break;
    case Op::OP_SHLI:  R[rd] = R[ra] << (ins.imm & 31); break;
    case Op::OP_SHRI:  R[rd] = R[ra] >> (ins.imm & 31); break;
    case Op::OP_SARI:  R[rd] = (u32)((i32)R[ra] >> (ins.imm & 31)); break;
    case Op::OP_CMPI: {
        i64 r = (i64)(i32)R[ra] - ins.imm;
        setIntFlags(r);
        break;
    }
    case Op::OP_CMPUI: {
        FLAGS &= ~(FL_Z | FL_N | FL_C);
        u32 imm = (u32)ins.imm;
        if (R[ra] == imm) FLAGS |= FL_Z;
        if (R[ra] <  imm) FLAGS |= FL_C;
        if (R[ra] <  imm) FLAGS |= FL_N;
        break;
    }

    // Format C — jumps
    case Op::OP_JMP:  PC = (u32)ins.imm; break;
    case Op::OP_JZ:   if  (FLAGS & FL_Z)                      PC = (u32)ins.imm; break;
    case Op::OP_JNZ:  if (!(FLAGS & FL_Z))                    PC = (u32)ins.imm; break;
    case Op::OP_JG:   if (!(FLAGS & FL_Z) && !(FLAGS & FL_N)) PC = (u32)ins.imm; break;
    case Op::OP_JL:   if  (FLAGS & FL_N)                      PC = (u32)ins.imm; break;
    case Op::OP_JGE:  if (!(FLAGS & FL_N))                    PC = (u32)ins.imm; break;
    case Op::OP_JLE:  if  (FLAGS & FL_Z || FLAGS & FL_N)      PC = (u32)ins.imm; break;
    case Op::OP_JFZ:  if  (FLAGS & FL_FZ)                     PC = (u32)ins.imm; break;
    case Op::OP_JFNZ: if (!(FLAGS & FL_FZ))                   PC = (u32)ins.imm; break;
    case Op::OP_JFN:  if  (FLAGS & FL_FN)                     PC = (u32)ins.imm; break;
    case Op::OP_JFI:  if  (FLAGS & FL_FI)                     PC = (u32)ins.imm; break;
    case Op::OP_JFU:  if  (FLAGS & FL_FU)                     PC = (u32)ins.imm; break;

    case Op::OP_LCALL: push32(PC); PC = (u32)ins.imm; break;

    case Op::OP_SWINT: {
        // software interrupt — look up handler in vector table
        u32 vec  = (u32)ins.imm & 0xFF;
        u32 handler = memRead32(IVEC_BASE + vec * 4);
        if (handler != 0) {
            push32(PC);
            PC = handler;
        }
        break;
    }

    // Format D — memory
    case Op::OP_LOAD:    R[rd] = memRead32(R[ra] + ins.imm); break;
    case Op::OP_LOADB:   R[rd] = (u32)memRead8(R[ra] + ins.imm); break;
    case Op::OP_LOADS:   R[rd] = (u32)(i32)(i8)memRead8(R[ra] + ins.imm); break;
    case Op::OP_LOADH:   R[rd] = (u32)memRead16(R[ra] + ins.imm); break;
    case Op::OP_STORE:   memWrite32(R[ra] + ins.imm, R[rd]); break;
    case Op::OP_STOREB:  memWrite8 (R[ra] + ins.imm, R[rd] & 0xFF); break;
    case Op::OP_STOREH:  memWrite16(R[ra] + ins.imm, R[rd] & 0xFFFF); break;
    case Op::OP_LOADF32: F[rd] = memReadF32(R[ra] + ins.imm); break;
    case Op::OP_LOADF64: F[rd] = memReadF64(R[ra] + ins.imm); break;
    case Op::OP_STORF32: memWriteF32(R[ra] + ins.imm, (f32)F[rd]); break;
    case Op::OP_STORF64: memWriteF64(R[ra] + ins.imm, F[rd]); break;

    case Op::OP_ILLEGAL:
    default:
        fault("illegal opcode: " + hexStr((u8)ins.op));
        break;
    }

    // enforce R0 = 0 and FLAGS read-only after execution too
    R[REG_ZERO] = 0;
    R[REG_FL]   = FLAGS;
}

// step / run

bool CPU::step() {
    if (status != CPUStatus::RUNNING) return false;
    Instr ins = fetch();
    execute(ins);
    cycles++;
    return status == CPUStatus::RUNNING;
}

u32 CPU::run(u32 maxCycles) {
    u32 ran = 0;
    while (ran < maxCycles && status == CPUStatus::RUNNING) {
        Instr ins = fetch();
        execute(ins);
        cycles++;
        ran++;
    }
    return ran;
}

} // namespace LS32