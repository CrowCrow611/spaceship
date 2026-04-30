#include "ls32asm.h"
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace LS32 {
// string helpers
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b-a+1);
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> toks;
    std::istringstream ss(s);
    std::string tok;
    while (ss >> tok) {
        // strip trailing commas
        if (!tok.empty() && tok.back() == ',')
            tok.pop_back();
        if (!tok.empty())
            toks.push_back(tok);
    }
    return toks;
}

// number parsing
// supports 42 0xFF 0b1010 -5

static bool parseNum(const std::string& s, i32& out) {
    if (s.empty()) return false;
    try {
        if (s.size() > 2 && s[0] == '0' && (s[1]=='x'||s[1]=='X'))
            out = (i32)std::stoul(s.substr(2), nullptr, 16);
        else if (s.size() > 2 && s[0] == '0' && (s[1]=='b'||s[1]=='B'))
            out = (i32)std::stoul(s.substr(2), nullptr, 2);
        else
            out = std::stoi(s);
        return true;
    } catch (...) { return false; }
}

//register parsing
// R0-R31, F0-F31, and aliases
static bool parseReg(const std::string& s, u8& idx, bool& isFloat) {
    std::string u = toUpper(s);
    isFloat = false;

    // integer aliases
    if (u == "ZERO") { idx = 0;  return true; }
    if (u == "RV")   { idx = 1;  return true; }
    if (u == "A0")   { idx = 2;  return true; }
    if (u == "A1")   { idx = 3;  return true; }
    if (u == "A2")   { idx = 4;  return true; }
    if (u == "A3")   { idx = 5;  return true; }
    if (u == "LR")   { idx = 28; return true; }
    if (u == "SP")   { idx = 29; return true; }
    if (u == "FP")   { idx = 30; return true; }

    if (u.size() >= 2 && u[0] == 'R') {
        try { idx = (u8)std::stoul(u.substr(1)); return idx < 32; }
        catch (...) {}
    }
    if (u.size() >= 2 && u[0] == 'F') {
        isFloat = true;
        try { idx = (u8)std::stoul(u.substr(1)); return idx < 32; }
        catch (...) {}
    }
    return false;
}

// byte emmiters
static void emitA(std::vector<u8>& out, Op op) {
    out.push_back((u8)op);
}

static void emitB(std::vector<u8>& out, Op op,
                  u8 rd, u8 ra, u8 rb, u16 func = 0) {
    // word = op | (rd<<8) | (ra<<13) | (rb<<18) | (func<<23)
    u32 w = (u32)op
          | ((u32)rd   <<  8)
          | ((u32)ra   << 13)
          | ((u32)rb   << 18)
          | ((u32)func << 23);
    out.push_back((w >>  0) & 0xFF);
    out.push_back((w >>  8) & 0xFF);
    out.push_back((w >> 16) & 0xFF);
    out.push_back((w >> 24) & 0xFF);
}

static void emitC(std::vector<u8>& out, Op op,
                  u8 rd, u8 ra, i32 imm) {
    // header word = op | (rd<<8) | (ra<<13)
    u32 h = (u32)op | ((u32)rd << 8) | ((u32)ra << 13);
    out.push_back((h >>  0) & 0xFF);
    out.push_back((h >>  8) & 0xFF);
    out.push_back((h >> 16) & 0xFF);
    out.push_back((h >> 24) & 0xFF);
    // immediate (little-endian 32-bit)
    u32 ui = (u32)imm;
    out.push_back((ui >>  0) & 0xFF);
    out.push_back((ui >>  8) & 0xFF);
    out.push_back((ui >> 16) & 0xFF);
    out.push_back((ui >> 24) & 0xFF);
}

static void emitD(std::vector<u8>& out, Op op,
                  u8 rd, u8 ra, i32 offset) {
    // same layout as C — header + 32-bit offset
    u32 h = (u32)op | ((u32)rd << 8) | ((u32)ra << 13);
    out.push_back((h >>  0) & 0xFF);
    out.push_back((h >>  8) & 0xFF);
    out.push_back((h >> 16) & 0xFF);
    out.push_back((h >> 24) & 0xFF);
    u32 uo = (u32)offset;
    out.push_back((uo >>  0) & 0xFF);
    out.push_back((uo >>  8) & 0xFF);
    out.push_back((uo >> 16) & 0xFF);
    out.push_back((uo >> 24) & 0xFF);
}

//parse memory opperand [Ra+off] or [Ra]

static bool parseMem(const std::string& tok, u8& reg, i32& off,
                     bool& isFloat) {
    // strip [ and ]
    std::string s = tok;
    if (s.empty() || s.front() != '[' || s.back() != ']') return false;
    s = s.substr(1, s.size()-2);

    // split on + or -
    size_t plus  = s.find('+');
    size_t minus = s.find('-');
    size_t sep   = std::string::npos;
    bool   neg   = false;

    if (plus  != std::string::npos) { sep = plus;  neg = false; }
    if (minus != std::string::npos) { sep = minus; neg = true;  }

    std::string regPart = sep == std::string::npos ? s : trim(s.substr(0, sep));
    std::string offPart = sep == std::string::npos ? "0" : trim(s.substr(sep+1));

    if (!parseReg(regPart, reg, isFloat)) return false;
    i32 rawOff = 0;
    if (!parseNum(offPart, rawOff)) rawOff = 0;
    off = neg ? -rawOff : rawOff;
    return true;
}

// label resolution

struct LabelUse {
    u32 patchOffset; // byte offset in binary to patch (the imm field)
    int sourceLine;
    std::string name;
};

// assembler pass

AsmResult assemble(const std::string& source) {
    AsmResult result;
    std::vector<u8>& out = result.binary;

    std::map<std::string, u32> labels; // label name → byte offset
    std::vector<LabelUse>      uses;   // forward references to patch

    std::istringstream ss(source);
    std::string rawLine;
    int lineNum = 0;

    auto err = [&](const std::string& msg) -> AsmResult {
        result.ok = false;
        result.error = {lineNum, msg};
        return result;
    };

    while (std::getline(ss, rawLine)) {
        lineNum++;

        // strip comment
        auto ci = rawLine.find(';');
        if (ci != std::string::npos) rawLine = rawLine.substr(0, ci);
        std::string line = trim(rawLine);
        if (line.empty()) continue;

        // label definition: "main:" or ".loop:"
        if (line.back() == ':') {
            std::string lbl = toUpper(trim(line.substr(0, line.size()-1)));
            if (lbl.empty()) return err("empty label");
            labels[lbl] = (u32)out.size() + PROG_START;
            continue;
        }

        auto toks = split(line);
        if (toks.empty()) continue;
        std::string mnem = toUpper(toks[0]);

        // ── resolve immediate or label ────────────────────────
        auto getImm = [&](const std::string& s, i32& imm) -> bool {
            std::string u = toUpper(s);
            auto it = labels.find(u);
            if (it != labels.end()) { imm = (i32)it->second; return true; }
            // forward reference — patch later
            if (!parseNum(s, imm)) {
                // assume it's a forward label reference
                uses.push_back({(u32)out.size() + 4, lineNum, u});
                imm = 0; // placeholder
                return true;
            }
            return true;
        };

        auto getReg = [&](const std::string& s, u8& idx) -> bool {
            bool f; return parseReg(s, idx, f);
        };

        auto getFReg = [&](const std::string& s, u8& idx) -> bool {
            bool f;
            if (!parseReg(s, idx, f)) return false;
            return f;
        };

        // need at least N tokens
        auto need = [&](int n) -> bool {
            if ((int)toks.size() < n+1) {
                result.error = {lineNum, mnem + " needs " + std::to_string(n) + " operands"};
                return false;
            }
            return true;
        };

        // ── Format A ─────────────────────────────────────────
        if      (mnem == "NOP")  { emitA(out, Op::OP_NOP);  }
        else if (mnem == "HLT")  { emitA(out, Op::OP_HLT);  }
        else if (mnem == "RET")  { emitA(out, Op::OP_RET);  }
        else if (mnem == "RETL") { emitA(out, Op::OP_RETL); }
        else if (mnem == "SRET") { emitA(out, Op::OP_SRET); }

        // ── Format B — integer register ops ──────────────────
        else if (mnem == "MOV") {
            if (!need(2)) return result;
            u8 rd, ra;
            if (!getReg(toks[1],rd)||!getReg(toks[2],ra)) return err("bad register");
            emitB(out, Op::OP_MOV, rd, ra, 0);
        }
        else if (mnem=="ADD"||mnem=="SUB"||mnem=="MUL"||mnem=="DIV"||
                 mnem=="DIVU"||mnem=="REM"||mnem=="REMU"||mnem=="MAC"||
                 mnem=="AND"||mnem=="OR"||mnem=="XOR"||
                 mnem=="SHL"||mnem=="SHR"||mnem=="SAR"||
                 mnem=="ROL"||mnem=="ROR"||mnem=="CMP"||mnem=="CMPU") {
            if (!need(3)) return result;
            u8 rd, ra, rb;
            if (!getReg(toks[1],rd)||!getReg(toks[2],ra)||!getReg(toks[3],rb))
                return err("bad register in " + mnem);
            Op op = Op::OP_NOP;
            if      (mnem=="ADD")  op=Op::OP_ADD;
            else if (mnem=="SUB")  op=Op::OP_SUB;
            else if (mnem=="MUL")  op=Op::OP_MUL;
            else if (mnem=="DIV")  op=Op::OP_DIV;
            else if (mnem=="DIVU") op=Op::OP_DIVU;
            else if (mnem=="REM")  op=Op::OP_REM;
            else if (mnem=="REMU") op=Op::OP_REMU;
            else if (mnem=="MAC")  op=Op::OP_MAC;
            else if (mnem=="AND")  op=Op::OP_BAND;
            else if (mnem=="OR")   op=Op::OP_BOR;
            else if (mnem=="XOR")  op=Op::OP_BXOR;
            else if (mnem=="SHL")  op=Op::OP_SHL;
            else if (mnem=="SHR")  op=Op::OP_SHR;
            else if (mnem=="SAR")  op=Op::OP_SAR;
            else if (mnem=="ROL")  op=Op::OP_ROL;
            else if (mnem=="ROR")  op=Op::OP_ROR;
            else if (mnem=="CMP")  op=Op::OP_CMP;
            else if (mnem=="CMPU") op=Op::OP_CMPU;
            emitB(out, op, rd, ra, rb);
        }
        else if (mnem=="MULH"||mnem=="MULHU") {
            if (!need(3)) return result;
            u8 rd, ra, rb;
            if (!getReg(toks[1],rd)||!getReg(toks[2],ra)||!getReg(toks[3],rb))
                return err("bad register");
            emitB(out, mnem=="MULH" ? Op::OP_MULH : Op::OP_MULHU, rd, ra, rb);
        }
        else if (mnem=="NOT"||mnem=="NEG"||mnem=="CLZ"||
                 mnem=="CTZ"||mnem=="POPC"||mnem=="BSWAP") {
            if (!need(2)) return result;
            u8 rd, ra;
            if (!getReg(toks[1],rd)||!getReg(toks[2],ra)) return err("bad register");
            Op op = Op::OP_NOP;
            if      (mnem=="NOT")   op=Op::OP_BNOT;
            else if (mnem=="NEG")   op=Op::OP_NEG;
            else if (mnem=="CLZ")   op=Op::OP_CLZ;
            else if (mnem=="CTZ")   op=Op::OP_CTZ;
            else if (mnem=="POPC")  op=Op::OP_POPC;
            else if (mnem=="BSWAP") op=Op::OP_BSWAP;
            emitB(out, op, rd, ra, 0);
        }
        else if (mnem=="JMPR") {
            if (!need(1)) return result;
            u8 ra; if (!getReg(toks[1],ra)) return err("bad register");
            emitB(out, Op::OP_JMPR, 0, ra, 0);
        }
        else if (mnem=="CALLR") {
            if (!need(1)) return result;
            u8 ra; if (!getReg(toks[1],ra)) return err("bad register");
            emitB(out, Op::OP_CALLR, 0, ra, 0);
        }

        // ── Format B — float ops ──────────────────────────────
        else if (mnem=="FADD32"||mnem=="FSUB32"||mnem=="FMUL32"||
                 mnem=="FDIV32"||mnem=="FMIN32"||mnem=="FMAX32"||
                 mnem=="FCMP32"||mnem=="FADD64"||mnem=="FSUB64"||
                 mnem=="FMUL64"||mnem=="FDIV64"||mnem=="FMIN64"||
                 mnem=="FMAX64"||mnem=="FCMP64") {
            if (!need(3)) return result;
            u8 rd, ra, rb;
            if (!getFReg(toks[1],rd)||!getFReg(toks[2],ra)||!getFReg(toks[3],rb))
                return err("bad float register");
            Op op = Op::OP_NOP;
            if      (mnem=="FADD32") op=Op::OP_FADD32;
            else if (mnem=="FSUB32") op=Op::OP_FSUB32;
            else if (mnem=="FMUL32") op=Op::OP_FMUL32;
            else if (mnem=="FDIV32") op=Op::OP_FDIV32;
            else if (mnem=="FMIN32") op=Op::OP_FMIN32;
            else if (mnem=="FMAX32") op=Op::OP_FMAX32;
            else if (mnem=="FCMP32") op=Op::OP_FCMP32;
            else if (mnem=="FADD64") op=Op::OP_FADD64;
            else if (mnem=="FSUB64") op=Op::OP_FSUB64;
            else if (mnem=="FMUL64") op=Op::OP_FMUL64;
            else if (mnem=="FDIV64") op=Op::OP_FDIV64;
            else if (mnem=="FMIN64") op=Op::OP_FMIN64;
            else if (mnem=="FMAX64") op=Op::OP_FMAX64;
            else if (mnem=="FCMP64") op=Op::OP_FCMP64;
            emitB(out, op, rd, ra, rb);
        }
        else if (mnem=="FMOV32"||mnem=="FMOV64"||
                 mnem=="FNEG32"||mnem=="FABS32"||mnem=="FSQRT32"||
                 mnem=="FNEG64"||mnem=="FABS64"||mnem=="FSQRT64"||
                 mnem=="F32TO64"||mnem=="F64TO32") {
            if (!need(2)) return result;
            u8 rd, ra;
            if (!getFReg(toks[1],rd)||!getFReg(toks[2],ra)) return err("bad float register");
            Op op = Op::OP_NOP;
            if      (mnem=="FMOV32")  op=Op::OP_FMOV32;
            else if (mnem=="FMOV64")  op=Op::OP_FMOV64;
            else if (mnem=="FNEG32")  op=Op::OP_FNEG32;
            else if (mnem=="FABS32")  op=Op::OP_FABS32;
            else if (mnem=="FSQRT32") op=Op::OP_FSQRT32;
            else if (mnem=="FNEG64")  op=Op::OP_FNEG64;
            else if (mnem=="FABS64")  op=Op::OP_FABS64;
            else if (mnem=="FSQRT64") op=Op::OP_FSQRT64;
            else if (mnem=="F32TO64") op=Op::OP_F32TO64;
            else if (mnem=="F64TO32") op=Op::OP_F64TO32;
            emitB(out, op, rd, ra, 0);
        }
        else if (mnem=="FMADD32"||mnem=="FMADD64") {
            if (!need(4)) return result;
            u8 rd, ra, rb, rc;
            if (!getFReg(toks[1],rd)||!getFReg(toks[2],ra)||
                !getFReg(toks[3],rb)||!getFReg(toks[4],rc))
                return err("bad float register");
            emitB(out, mnem=="FMADD32" ? Op::OP_FMADD32 : Op::OP_FMADD64,
                  rd, ra, rb, rc);
        }
        else if (mnem=="FCVT32I"||mnem=="FCVT64I") {
            if (!need(2)) return result;
            u8 rd, ra; bool f;
            if (!parseReg(toks[1],rd,f)) return err("bad int register");
            if (!getFReg(toks[2],ra))    return err("bad float register");
            emitB(out, mnem=="FCVT32I" ? Op::OP_FCVT32I : Op::OP_FCVT64I,
                  rd, ra, 0);
        }
        else if (mnem=="FICVT32"||mnem=="FICVT64") {
            if (!need(2)) return result;
            u8 rd, ra; bool f;
            if (!getFReg(toks[1],rd))    return err("bad float register");
            if (!parseReg(toks[2],ra,f)) return err("bad int register");
            emitB(out, mnem=="FICVT32" ? Op::OP_FICVT32 : Op::OP_FICVT64,
                  rd, ra, 0);
        }

        // ── Format C — immediate ops ──────────────────────────
        else if (mnem=="MOVI") {
            if (!need(2)) return result;
            u8 rd; i32 imm;
            if (!getReg(toks[1],rd)) return err("bad register");
            if (!getImm(toks[2],imm)) return err("bad immediate");
            emitC(out, Op::OP_MOVI, rd, 0, imm);
        }
        else if (mnem=="ADDI"||mnem=="ANDI"||mnem=="ORI"||
                 mnem=="XORI"||mnem=="SHLI"||mnem=="SHRI"||mnem=="SARI") {
            if (!need(3)) return result;
            u8 rd, ra; i32 imm;
            if (!getReg(toks[1],rd)||!getReg(toks[2],ra)) return err("bad register");
            if (!getImm(toks[3],imm)) return err("bad immediate");
            Op op = Op::OP_NOP;
            if      (mnem=="ADDI") op=Op::OP_ADDI;
            else if (mnem=="ANDI") op=Op::OP_ANDI;
            else if (mnem=="ORI")  op=Op::OP_ORI;
            else if (mnem=="XORI") op=Op::OP_XORI;
            else if (mnem=="SHLI") op=Op::OP_SHLI;
            else if (mnem=="SHRI") op=Op::OP_SHRI;
            else if (mnem=="SARI") op=Op::OP_SARI;
            emitC(out, op, rd, ra, imm);
        }
        else if (mnem=="CMPI"||mnem=="CMPUI") {
            if (!need(2)) return result;
            u8 ra; i32 imm;
            if (!getReg(toks[1],ra)) return err("bad register");
            if (!getImm(toks[2],imm)) return err("bad immediate");
            emitC(out, mnem=="CMPI" ? Op::OP_CMPI : Op::OP_CMPUI,
                  0, ra, imm);
        }

        // ── Format C — jumps ──────────────────────────────────
        else if (mnem=="JMP"||mnem=="JZ"||mnem=="JNZ"||
                 mnem=="JG"||mnem=="JL"||mnem=="JGE"||mnem=="JLE"||
                 mnem=="JFZ"||mnem=="JFNZ"||mnem=="JFN"||
                 mnem=="JFI"||mnem=="JFU") {
            if (!need(1)) return result;
            i32 imm = 0;
            // check if it's a label
            std::string target = toUpper(toks[1]);
            auto it = labels.find(target);
            if (it != labels.end()) {
                imm = (i32)it->second;
            } else {
                // forward reference
                uses.push_back({(u32)out.size() + 4, lineNum, target});
                imm = 0;
            }
            Op op = Op::OP_NOP;
            if      (mnem=="JMP")  op=Op::OP_JMP;
            else if (mnem=="JZ")   op=Op::OP_JZ;
            else if (mnem=="JNZ")  op=Op::OP_JNZ;
            else if (mnem=="JG")   op=Op::OP_JG;
            else if (mnem=="JL")   op=Op::OP_JL;
            else if (mnem=="JGE")  op=Op::OP_JGE;
            else if (mnem=="JLE")  op=Op::OP_JLE;
            else if (mnem=="JFZ")  op=Op::OP_JFZ;
            else if (mnem=="JFNZ") op=Op::OP_JFNZ;
            else if (mnem=="JFN")  op=Op::OP_JFN;
            else if (mnem=="JFI")  op=Op::OP_JFI;
            else if (mnem=="JFU")  op=Op::OP_JFU;
            emitC(out, op, 0, 0, imm);
        }
        else if (mnem=="CALL") {
            if (!need(1)) return result;
            i32 imm = 0;
            std::string target = toUpper(toks[1]);
            auto it = labels.find(target);
            if (it != labels.end()) {
                imm = (i32)it->second;
            } else {
                uses.push_back({(u32)out.size() + 4, lineNum, target});
                imm = 0;
            }
            emitC(out, Op::OP_LCALL, 0, 0, imm);
        }
        else if (mnem=="INT") {
            if (!need(1)) return result;
            i32 vec;
            if (!parseNum(toks[1],vec)) return err("bad interrupt vector");
            emitC(out, Op::OP_SWINT, 0, 0, vec & 0xFF);
        }

        // ── Format D — memory ─────────────────────────────────
        else if (mnem=="LOAD"||mnem=="LOADB"||mnem=="LOADS"||mnem=="LOADH") {
            if (!need(2)) return result;
            u8 rd, ra; i32 off; bool f;
            if (!getReg(toks[1],rd)) return err("bad dest register");
            if (!parseMem(toks[2],ra,off,f)) return err("bad memory operand");
            Op op = mnem=="LOAD"  ? Op::OP_LOAD  :
                    mnem=="LOADB" ? Op::OP_LOADB :
                    mnem=="LOADS" ? Op::OP_LOADS : Op::OP_LOADH;
            emitD(out, op, rd, ra, off);
        }
        else if (mnem=="STORE"||mnem=="STOREB"||mnem=="STOREH") {
            if (!need(2)) return result;
            u8 rd, ra; i32 off; bool f;
            if (!parseMem(toks[1],ra,off,f)) return err("bad memory operand");
            if (!getReg(toks[2],rd)) return err("bad src register");
            Op op = mnem=="STORE"  ? Op::OP_STORE  :
                    mnem=="STOREB" ? Op::OP_STOREB : Op::OP_STOREH;
            emitD(out, op, rd, ra, off);
        }
        else if (mnem=="LOADF32"||mnem=="LOADF64") {
            if (!need(2)) return result;
            u8 rd, ra; i32 off; bool f;
            if (!getFReg(toks[1],rd)) return err("bad float register");
            if (!parseMem(toks[2],ra,off,f)) return err("bad memory operand");
            emitD(out, mnem=="LOADF32" ? Op::OP_LOADF32 : Op::OP_LOADF64,
                  rd, ra, off);
        }
        else if (mnem=="STORF32"||mnem=="STORF64") {
            if (!need(2)) return result;
            u8 rd, ra; i32 off; bool f;
            if (!parseMem(toks[1],ra,off,f)) return err("bad memory operand");
            if (!getFReg(toks[2],rd)) return err("bad float register");
            emitD(out, mnem=="STORF32" ? Op::OP_STORF32 : Op::OP_STORF64,
                  rd, ra, off);
        }

        else {
            return err("unknown mnemonic: " + mnem);
        }

        result.instrCount++;
    }

    // ── patch forward label references ────────────────────────
    for (auto& use : uses) {
        auto it = labels.find(use.name);
        if (it == labels.end()) {
            result.error = {use.sourceLine, "undefined label: " + use.name};
            return result;
        }
        u32 addr = it->second;
        u32 off  = use.patchOffset;
        if (off + 4 > out.size()) {
            result.error = {use.sourceLine, "patch offset out of range"};
            return result;
        }
        out[off+0] = (addr >>  0) & 0xFF;
        out[off+1] = (addr >>  8) & 0xFF;
        out[off+2] = (addr >> 16) & 0xFF;
        out[off+3] = (addr >> 24) & 0xFF;
    }

    result.ok = true;
    return result;
}

AsmResult assembleFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        AsmResult r;
        r.error = {-1, "cannot open file: " + path};
        return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return assemble(ss.str());
}

} // namespace LS32