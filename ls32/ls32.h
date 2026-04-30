#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

//word sizes
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

//architecture constants
namespace LS32 {
constexpr u32 MEM_SIZE = 0x20000000; //512mb
constexpr u32 IVEC_BASE = 0x00000000; //interrupt vector table
constexpr u32 IVEC_SIZE = 0x00000100; // 256 vectors x 4 bytes
constexpr u32 PROG_START = 0x00000100; // default PC on reset
constexpr u32 MMIO_BASE = 0x1E000000; // memory-mapped I/O
constexpr u32 MMIO_END = 0x1EFFFFFF;
constexpr u32 STACK_TOP = 0x1FFFFFFF; // SP starts here
constexpr u32 STACK_BASE      = 0x1F000000;

constexpr int NUM_IREGS       = 32; // R0–R31
constexpr int NUM_FREGS       = 32; // F0–F31

// special integer register indices
constexpr u8 REG_ZERO = 0;  // always 0
constexpr u8 REG_RV   = 1;  // return value
constexpr u8 REG_A0   = 2;  // arg 0
constexpr u8 REG_A1   = 3;  // arg 1
constexpr u8 REG_A2   = 4;  // arg 2
constexpr u8 REG_A3   = 5;  // arg 3
constexpr u8 REG_LR   = 28; // link register
constexpr u8 REG_SP   = 29; // stack pointer
constexpr u8 REG_FP   = 30; // frame pointer
constexpr u8 REG_FL   = 31; // FLAGS (read-only from software)

// flags bits (stored in R31 internally)
constexpr u32 FL_Z = (1u << 0); // Integer zero
constexpr u32 FL_N = (1u << 1); // Integer negative
constexpr u32 FL_C = (1u << 2); // Integer carry
constexpr u32 FL_V = (1u << 3); // Integer overflow
constexpr u32 FL_FZ = (1u << 4); // float zero
constexpr u32 FL_FN = (1u << 5); // float negative
constexpr u32 FL_FI = (1u << 6); // float infinity
constexpr u32 FL_FU = (1u << 7); // float NaN

// CPU Status
enum class CPUStatus : u8 {
    RUNNING = 0,
    HALTED, //hlt executed
    FAULT, //illegal opcode / bad memory access
    DIVZERO, //division by zero
};

 // instruction format, 
 // format A: 1 byte
 //Format B: 4byte
 // Format C: 8 byte
 // Format D: 8 bytes

enum class Fmt : u8 { A, B, C, D};


#ifdef NOP
#undef NOP
#endif
#ifdef HLT
#undef HLT
#endif
#ifdef INT
#undef INT
#endif
#ifdef DIV
#undef DIV
#endif
#ifdef REM
#undef REM
#endif
#ifdef AND
#undef AND
#endif
#ifdef OR
#undef OR
#endif
#ifdef NOT
#undef NOT
#endif
#ifdef XOR
#undef XOR
#endif
#ifdef CALL
#undef CALL
#endif
#ifdef MOV
#undef MOV
#endif
#ifdef CMP
#undef CMP
#endif

//opcodes
enum class Op : u8 {
    OP_NOP    = 0x00,
    OP_HLT    = 0x01,
    OP_RET    = 0x02,
    OP_RETL   = 0x03,
    OP_SRET   = 0x04,
    OP_MOV    = 0x10,
    OP_ADD    = 0x11,
    OP_SUB    = 0x12,
    OP_MUL    = 0x13,
    OP_MULH   = 0x14,
    OP_MULHU  = 0x15,
    OP_DIV    = 0x16,
    OP_DIVU   = 0x17,
    OP_REM    = 0x18,
    OP_REMU   = 0x19,
    OP_MAC    = 0x1A,
    OP_BAND   = 0x1B,
    OP_BOR    = 0x1C,
    OP_BXOR   = 0x1D,
    OP_BNOT   = 0x1E,
    OP_NEG    = 0x1F,
    OP_SHL    = 0x20,
    OP_SHR    = 0x21,
    OP_SAR    = 0x22,
    OP_ROL    = 0x23,
    OP_ROR    = 0x24,
    OP_CLZ    = 0x25,
    OP_CTZ    = 0x26,
    OP_POPC   = 0x27,
    OP_BSWAP  = 0x28,
    OP_CMP    = 0x29,
    OP_CMPU   = 0x2A,
    OP_JMPR   = 0x2B,
    OP_CALLR  = 0x2C,
    OP_FMOV32  = 0x30,
    OP_FADD32  = 0x31,
    OP_FSUB32  = 0x32,
    OP_FMUL32  = 0x33,
    OP_FDIV32  = 0x34,
    OP_FNEG32  = 0x35,
    OP_FABS32  = 0x36,
    OP_FSQRT32 = 0x37,
    OP_FMIN32  = 0x38,
    OP_FMAX32  = 0x39,
    OP_FMADD32 = 0x3A,
    OP_FCMP32  = 0x3B,
    OP_FCVT32I = 0x3C,
    OP_FICVT32 = 0x3D,
    OP_FMOV64  = 0x40,
    OP_FADD64  = 0x41,
    OP_FSUB64  = 0x42,
    OP_FMUL64  = 0x43,
    OP_FDIV64  = 0x44,
    OP_FNEG64  = 0x45,
    OP_FABS64  = 0x46,
    OP_FSQRT64 = 0x47,
    OP_FMIN64  = 0x48,
    OP_FMAX64  = 0x49,
    OP_FMADD64 = 0x4A,
    OP_FCMP64  = 0x4B,
    OP_FCVT64I = 0x4C,
    OP_FICVT64 = 0x4D,
    OP_F32TO64 = 0x4E,
    OP_F64TO32 = 0x4F,
    OP_MOVI    = 0x50,
    OP_ADDI    = 0x51,
    OP_ANDI    = 0x52,
    OP_ORI     = 0x53,
    OP_XORI    = 0x54,
    OP_SHLI    = 0x55,
    OP_SHRI    = 0x56,
    OP_SARI    = 0x57,
    OP_CMPI    = 0x58,
    OP_CMPUI   = 0x59,
    OP_JMP     = 0x60,
    OP_JZ      = 0x61,
    OP_JNZ     = 0x62,
    OP_JG      = 0x63,
    OP_JL      = 0x64,
    OP_JGE     = 0x65,
    OP_JLE     = 0x66,
    OP_JFZ     = 0x67,
    OP_JFNZ    = 0x68,
    OP_JFN     = 0x69,
    OP_JFI     = 0x6A,
    OP_JFU     = 0x6B,
    OP_LCALL   = 0x6C,
    OP_SWINT   = 0x6D,
    OP_LOAD    = 0x70,
    OP_LOADB   = 0x71,
    OP_LOADS   = 0x72,
    OP_LOADH   = 0x73,
    OP_STORE   = 0x74,
    OP_STOREB  = 0x75,
    OP_STOREH  = 0x76,
    OP_LOADF32 = 0x77,
    OP_LOADF64 = 0x78,
    OP_STORF32 = 0x79,
    OP_STORF64 = 0x7A,
    OP_ILLEGAL = 0xFF,
};

// decoded instructions
struct Instr {
    Op op = Op::OP_NOP;
    Fmt fmt = Fmt::A; 
    u8 rd =0; // dest int register
    u8 ra =0; // src int register A
    u8 rb=0; // src int register B
    u16 func=0; // extra field 
    i32 imm =0; // immediate / address / offset
};

using IOReadFn  = std::function<u32(u32 addr)>;
using IOWriteFn = std::function<void(u32 addr, u32 val)>;

// format helpers 
inline Fmt fmtOf(Op op) {
    u8 o = (u8)op;
    if (o <= 0x04) return Fmt::A;
    if (o <= 0x4F) return Fmt::B;
    if (o <= 0x6D) return Fmt::C;
    return Fmt::D;
}

//  instruction size in bytes 
inline u32 instrSize(Fmt f) {
    switch (f) {
    case Fmt::A: return 1;
    case Fmt::B: return 4;
    case Fmt::C: return 8;
    case Fmt::D: return 8;
    }
    return 1;
}

} // namespace LS32



  