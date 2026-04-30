#include "ls32dis.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace LS32 {

// ── name helpers ─────────────────────────────────────────────

std::string Disassembler::regName(u8 r) {
    // show alias alongside number for special registers
    switch (r) {
    case REG_ZERO: return "R0(zero)";
    case REG_RV:   return "R1(rv)";
    case REG_A0:   return "R2(a0)";
    case REG_A1:   return "R3(a1)";
    case REG_A2:   return "R4(a2)";
    case REG_A3:   return "R5(a3)";
    case REG_LR:   return "R28(lr)";
    case REG_SP:   return "R29(sp)";
    case REG_FP:   return "R30(fp)";
    case REG_FL:   return "R31(fl)";
    default:
        return "R" + std::to_string(r);
    }
}

std::string Disassembler::fregName(u8 r) {
    switch (r) {
    case 0: return "F0(frv)";
    case 1: return "F1(fa0)";
    case 2: return "F2(fa1)";
    case 3: return "F3(fa2)";
    default:
        return "F" + std::to_string(r);
    }
}

std::string Disassembler::immStr(i32 imm) {
    std::ostringstream ss;
    ss << imm;
    if (imm != 0)
        ss << " (0x" << std::hex << std::uppercase << (u32)imm << ")";
    return ss.str();
}

std::string Disassembler::flagsStr(u32 f) {
    std::string s = "[";
    s += (f & FL_Z)  ? "Z"  : "-";
    s += (f & FL_N)  ? "N"  : "-";
    s += (f & FL_C)  ? "C"  : "-";
    s += (f & FL_V)  ? "V"  : "-";
    s += " ";
    s += (f & FL_FZ) ? "FZ" : "--";
    s += (f & FL_FN) ? "FN" : "--";
    s += (f & FL_FI) ? "FI" : "--";
    s += (f & FL_FU) ? "FU" : "--";
    s += "]";
    return s;
}

// ── opcode name table ─────────────────────────────────────────

static const char* opName(Op op) {
    switch (op) {
    case Op::OP_NOP:    return "NOP";
    case Op::OP_HLT:    return "HLT";
    case Op::OP_RET:    return "RET";
    case Op::OP_RETL:   return "RETL";
    case Op::OP_SRET:   return "SRET";
    case Op::OP_MOV:    return "MOV";
    case Op::OP_ADD:    return "ADD";
    case Op::OP_SUB:    return "SUB";
    case Op::OP_MUL:    return "MUL";
    case Op::OP_MULH:   return "MULH";
    case Op::OP_MULHU:  return "MULHU";
    case Op::OP_DIV:    return "DIV";
    case Op::OP_DIVU:   return "DIVU";
    case Op::OP_REM:    return "REM";
    case Op::OP_REMU:   return "REMU";
    case Op::OP_MAC:    return "MAC";
    case Op::OP_BAND:    return "AND";
    case Op::OP_BOR:     return "OR";
    case Op::OP_BXOR:    return "XOR";
    case Op::OP_BNOT:    return "NOT";
    case Op::OP_NEG:    return "NEG";
    case Op::OP_SHL:    return "SHL";
    case Op::OP_SHR:    return "SHR";
    case Op::OP_SAR:    return "SAR";
    case Op::OP_ROL:    return "ROL";
    case Op::OP_ROR:    return "ROR";
    case Op::OP_CLZ:    return "CLZ";
    case Op::OP_CTZ:    return "CTZ";
    case Op::OP_POPC:   return "POPC";
    case Op::OP_BSWAP:  return "BSWAP";
    case Op::OP_CMP:    return "CMP";
    case Op::OP_CMPU:   return "CMPU";
    case Op::OP_JMPR:   return "JMPR";
    case Op::OP_CALLR:  return "CALLR";
    case Op::OP_FMOV32: return "FMOV32";
    case Op::OP_FADD32: return "FADD32";
    case Op::OP_FSUB32: return "FSUB32";
    case Op::OP_FMUL32: return "FMUL32";
    case Op::OP_FDIV32: return "FDIV32";
    case Op::OP_FNEG32: return "FNEG32";
    case Op::OP_FABS32: return "FABS32";
    case Op::OP_FSQRT32:return "FSQRT32";
    case Op::OP_FMIN32: return "FMIN32";
    case Op::OP_FMAX32: return "FMAX32";
    case Op::OP_FMADD32:return "FMADD32";
    case Op::OP_FCMP32: return "FCMP32";
    case Op::OP_FCVT32I:return "FCVT32I";
    case Op::OP_FICVT32:return "FICVT32";
    case Op::OP_FMOV64: return "FMOV64";
    case Op::OP_FADD64: return "FADD64";
    case Op::OP_FSUB64: return "FSUB64";
    case Op::OP_FMUL64: return "FMUL64";
    case Op::OP_FDIV64: return "FDIV64";
    case Op::OP_FNEG64: return "FNEG64";
    case Op::OP_FABS64: return "FABS64";
    case Op::OP_FSQRT64:return "FSQRT64";
    case Op::OP_FMIN64: return "FMIN64";
    case Op::OP_FMAX64: return "FMAX64";
    case Op::OP_FMADD64:return "FMADD64";
    case Op::OP_FCMP64: return "FCMP64";
    case Op::OP_FCVT64I:return "FCVT64I";
    case Op::OP_FICVT64:return "FICVT64";
    case Op::OP_F32TO64:return "F32TO64";
    case Op::OP_F64TO32:return "F64TO32";
    case Op::OP_MOVI:   return "MOVI";
    case Op::OP_ADDI:   return "ADDI";
    case Op::OP_ANDI:   return "ANDI";
    case Op::OP_ORI:    return "ORI";
    case Op::OP_XORI:   return "XORI";
    case Op::OP_SHLI:   return "SHLI";
    case Op::OP_SHRI:   return "SHRI";
    case Op::OP_SARI:   return "SARI";
    case Op::OP_CMPI:   return "CMPI";
    case Op::OP_CMPUI:  return "CMPUI";
    case Op::OP_JMP:    return "JMP";
    case Op::OP_JZ:     return "JZ";
    case Op::OP_JNZ:    return "JNZ";
    case Op::OP_JG:     return "JG";
    case Op::OP_JL:     return "JL";
    case Op::OP_JGE:    return "JGE";
    case Op::OP_JLE:    return "JLE";
    case Op::OP_JFZ:    return "JFZ";
    case Op::OP_JFNZ:   return "JFNZ";
    case Op::OP_JFN:    return "JFN";
    case Op::OP_JFI:    return "JFI";
    case Op::OP_JFU:    return "JFU";
    case Op::OP_LCALL:   return "CALL";
    case Op::OP_SWINT:    return "INT";
    case Op::OP_LOAD:   return "LOAD";
    case Op::OP_LOADB:  return "LOADB";
    case Op::OP_LOADS:  return "LOADS";
    case Op::OP_LOADH:  return "LOADH";
    case Op::OP_STORE:  return "STORE";
    case Op::OP_STOREB: return "STOREB";
    case Op::OP_STOREH: return "STOREH";
    case Op::OP_LOADF32:return "LOADF32";
    case Op::OP_LOADF64:return "LOADF64";
    case Op::OP_STORF32:return "STORF32";
    case Op::OP_STORF64:return "STORF64";
    default:         return "???";
    }
}

// ── read raw bytes without executing ─────────────────────────
// We re-decode here without touching PC

static Instr peekInstr(CPU& cpu, u32 addr, u32& outSize) {
    Instr ins;
    u8 opByte = cpu.memRead8(addr);
    ins.op    = (Op)opByte;
    ins.fmt   = fmtOf(ins.op);
    outSize   = instrSize(ins.fmt);

    switch (ins.fmt) {
    case Fmt::A:
        break;

    case Fmt::B: {
        u32 w  = cpu.memRead32(addr);
        ins.rd   = (w >> 19) & 0x1F;
        ins.ra   = (w >> 14) & 0x1F;
        ins.rb   = (w >>  9) & 0x1F;
        ins.func = (w >>  0) & 0x1FF;
        break;
    }
    case Fmt::C: {
        u32 h  = cpu.memRead32(addr);
        ins.rd  = (h >> 19) & 0x1F;
        ins.ra  = (h >> 14) & 0x1F;
        ins.imm = (i32)cpu.memRead32(addr + 4);
        break;
    }
    case Fmt::D: {
        u32 h    = cpu.memRead32(addr);
        ins.rd   = (h >> 19) & 0x1F;
        ins.ra   = (h >> 14) & 0x1F;
        i16 o14  = (i16)((h & 0x3FFF) << 2) >> 2;
        i32 ext  = (i32)cpu.memRead32(addr + 4);
        ins.imm  = ext != 0 ? ext : (i32)o14;
        break;
    }
    }
    return ins;
}

// ── format one instruction as text ───────────────────────────

static std::string fmtInstr(const Instr& ins, Disassembler& d) {
    std::string n = opName(ins.op);

    auto R  = [&](u8 r) { return d.regName(r); };
    auto F  = [&](u8 r) { return d.fregName(r); };
    auto I  = [&](i32 i){ return d.immStr(i); };

    switch (ins.fmt) {
    case Fmt::A:
        return n;

    case Fmt::B: {
        // special cases
        switch (ins.op) {
        case Op::OP_BNOT: case Op::OP_NEG:
        case Op::OP_CLZ: case Op::OP_CTZ:
        case Op::OP_POPC: case Op::OP_BSWAP:
        case Op::OP_FNEG32: case Op::OP_FABS32: case Op::OP_FSQRT32:
        case Op::OP_FNEG64: case Op::OP_FABS64: case Op::OP_FSQRT64:
        case Op::OP_F32TO64: case Op::OP_F64TO32:
        case Op::OP_FCVT32I: case Op::OP_FCVT64I:
            return n + " " + R(ins.rd) + ", " + R(ins.ra);
        case Op::OP_FICVT32: case Op::OP_FICVT64:
            return n + " " + F(ins.rd) + ", " + R(ins.ra);
        case Op::OP_FMOV32: case Op::OP_FMOV64:
            return n + " " + F(ins.rd) + ", " + F(ins.ra);
        case Op::OP_FMADD32: case Op::OP_FMADD64:
            return n + " " + F(ins.rd) + ", " + F(ins.ra)
                     + ", " + F(ins.rb)
                     + ", " + F(ins.func % NUM_FREGS);
        case Op::OP_CMP: case Op::OP_CMPU:
            return n + " " + R(ins.ra) + ", " + R(ins.rb);
        case Op::OP_JMPR:
            return n + " " + R(ins.ra);
        case Op::OP_CALLR:
            return n + " " + R(ins.ra);
        // float binary ops
        case Op::OP_FADD32: case Op::OP_FSUB32:
        case Op::OP_FMUL32: case Op::OP_FDIV32:
        case Op::OP_FMIN32: case Op::OP_FMAX32:
        case Op::OP_FCMP32:
        case Op::OP_FADD64: case Op::OP_FSUB64:
        case Op::OP_FMUL64: case Op::OP_FDIV64:
        case Op::OP_FMIN64: case Op::OP_FMAX64:
        case Op::OP_FCMP64:
            return n + " " + F(ins.rd) + ", "
                     + F(ins.ra) + ", " + F(ins.rb);
        default:
            // standard: Rd, Ra, Rb
            return n + " " + R(ins.rd) + ", "
                     + R(ins.ra) + ", " + R(ins.rb);
        }
    }
    case Fmt::C: {
        switch (ins.op) {
        // jumps/calls — just an address
        case Op::OP_JMP: case Op::OP_JZ:  case Op::OP_JNZ:
        case Op::OP_JG:  case Op::OP_JL:  case Op::OP_JGE:
        case Op::OP_JLE: case Op::OP_JFZ: case Op::OP_JFNZ:
        case Op::OP_JFN: case Op::OP_JFI: case Op::OP_JFU:
        case Op::OP_LCALL:
            return n + " " + I(ins.imm);
        case Op::OP_SWINT:
            return n + " " + std::to_string(ins.imm & 0xFF);
        case Op::OP_CMPI: case Op::OP_CMPUI:
            return n + " " + R(ins.ra) + ", " + I(ins.imm);
        case Op::OP_MOVI:
            return n + " " + R(ins.rd) + ", " + I(ins.imm);
        default:
            // Rd, Ra, imm
            return n + " " + R(ins.rd) + ", "
                     + R(ins.ra) + ", " + I(ins.imm);
        }
    }
    case Fmt::D: {
        std::string offset = ins.imm == 0
            ? ""
            : (ins.imm > 0 ? "+" : "") + std::to_string(ins.imm);
        switch (ins.op) {
        case Op::OP_STORE: case Op::OP_STOREB:
        case Op::OP_STOREH:
            return n + " [" + R(ins.ra) + offset + "], " + R(ins.rd);
        case Op::OP_STORF32: case Op::OP_STORF64:
            return n + " [" + R(ins.ra) + offset + "], " + F(ins.rd);
        case Op::OP_LOADF32: case Op::OP_LOADF64:
            return n + " " + F(ins.rd) + ", [" + R(ins.ra) + offset + "]";
        default:
            return n + " " + R(ins.rd) + ", [" + R(ins.ra) + offset + "]";
        }
    }
    }
    return "???";
}

// ── hex bytes string ──────────────────────────────────────────

static std::string hexBytes(CPU& cpu, u32 addr, u32 size) {
    std::ostringstream ss;
    for (u32 i = 0; i < size; i++) {
        if (i) ss << " ";
        ss << std::hex << std::uppercase
           << std::setw(2) << std::setfill('0')
           << (int)cpu.memRead8(addr + i);
    }
    return ss.str();
}

// ── public API ────────────────────────────────────────────────

DisResult Disassembler::disOne(CPU& cpu, u32 addr) {
    DisResult r;
    r.addr = addr;
    Instr ins = peekInstr(cpu, addr, r.size);
    r.hex  = hexBytes(cpu, addr, r.size);
    r.text = fmtInstr(ins, *this);
    return r;
}

std::vector<DisResult> Disassembler::dis(CPU& cpu, u32 addr, u32 count) {
    std::vector<DisResult> out;
    out.reserve(count);
    for (u32 i = 0; i < count; i++) {
        auto r = disOne(cpu, addr);
        out.push_back(r);
        addr += r.size;
        if (addr >= MEM_SIZE) break;
    }
    return out;
}

void Disassembler::dump(CPU& cpu, u32 addr, u32 count) {
    auto results = dis(cpu, addr, count);
    for (auto& r : results) {
        std::cout
            << std::hex << std::uppercase
            << std::setw(8) << std::setfill('0') << r.addr
            << "  "
            << std::left << std::setw(20) << std::setfill(' ') << r.hex
            << "  "
            << r.text
            << "\n";
    }
}

void Disassembler::dumpAround(CPU& cpu, u32 before, u32 after) {
    // walk backwards to find start — tricky with variable width
    // strategy: disassemble from PROG_START up to PC, keep last N
    std::vector<u32> addrs;
    u32 cur = PROG_START;
    while (cur < cpu.PC && cur < MEM_SIZE) {
        addrs.push_back(cur);
        u32 sz;
        peekInstr(cpu, cur, sz);
        cur += sz;
    }

    // print (before) instructions before PC
    u32 startIdx = addrs.size() > before
        ? (u32)addrs.size() - before : 0;

    std::cout << "── disassembly around PC ──\n";
    for (u32 i = startIdx; i < addrs.size(); i++) {
        auto r = disOne(cpu, addrs[i]);
        std::cout
            << "  "
            << std::hex << std::setw(8) << std::setfill('0') << r.addr
            << "  " << std::left << std::setw(20) << r.hex
            << "  " << r.text << "\n";
    }

    // current PC marker
    auto cur2 = disOne(cpu, cpu.PC);
    std::cout
        << "> "
        << std::hex << std::setw(8) << std::setfill('0') << cur2.addr
        << "  " << std::left << std::setw(20) << cur2.hex
        << "  " << cur2.text << "   ← PC\n";

    // print (after) instructions after PC
    u32 nxt = cpu.PC + cur2.size;
    for (u32 i = 0; i < after && nxt < MEM_SIZE; i++) {
        auto r = disOne(cpu, nxt);
        std::cout
            << "  "
            << std::hex << std::setw(8) << std::setfill('0') << r.addr
            << "  " << std::left << std::setw(20) << r.hex
            << "  " << r.text << "\n";
        nxt += r.size;
    }
    std::cout << std::dec;
}

void Disassembler::dumpRegs(CPU& cpu) {
    std::cout << "── integer registers ──\n";
    for (int i = 0; i < NUM_IREGS; i++) {
        std::cout
            << "  " << std::left << std::setw(12) << regName(i)
            << " = " << std::dec << cpu.R[i]
            << "  (0x" << std::hex << std::uppercase
            << std::setw(8) << std::setfill('0') << cpu.R[i] << ")\n";
    }
    std::cout << "── float registers ──\n";
    for (int i = 0; i < NUM_FREGS; i++) {
        std::cout
            << "  " << std::left << std::setw(12) << fregName(i)
            << " = " << cpu.F[i] << "\n";
    }
    std::cout << "── status ──\n"
              << "  PC    = 0x" << std::hex << cpu.PC << "\n"
              << "  FLAGS = " << flagsStr(cpu.FLAGS) << "\n"
              << "  cycles= " << std::dec << cpu.cycles << "\n";
}

void Disassembler::hexDump(CPU& cpu, u32 addr, u32 bytes) {
    std::cout << "── hex dump 0x"
              << std::hex << addr << " .. 0x"
              << addr + bytes << " ──\n";
    for (u32 i = 0; i < bytes; i += 16) {
        std::cout << std::hex << std::setw(8)
                  << std::setfill('0') << addr + i << "  ";
        // hex bytes
        for (u32 j = 0; j < 16; j++) {
            if (i+j < bytes)
                std::cout << std::setw(2) << (int)cpu.memRead8(addr+i+j) << " ";
            else
                std::cout << "   ";
            if (j == 7) std::cout << " ";
        }
        // ascii
        std::cout << " |";
        for (u32 j = 0; j < 16 && i+j < bytes; j++) {
            u8 c = cpu.memRead8(addr+i+j);
            std::cout << (char)(c >= 32 && c < 127 ? c : '.');
        }
        std::cout << "|\n";
    }
    std::cout << std::dec;
}

} // namespace LS32