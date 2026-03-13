#pragma once

#include "types.h"
#include <bit>
#include <optional>

constexpr bool isCondBranch(Mnemonic m) {
    switch (m) {
    case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
    case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
    case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
    case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
    case Mnemonic::bgt: case Mnemonic::ble:
        return true;
    default:
        return false;
    }
}

constexpr bool isBranch(Mnemonic m) {
    return m == Mnemonic::bra || isCondBranch(m);
}

constexpr bool isDbcc(Mnemonic m) {
    switch (m) {
    case Mnemonic::dbt: case Mnemonic::dbf:
    case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
    case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
    case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
    case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
    case Mnemonic::dbgt: case Mnemonic::dble:
        return true;
    default:
        return false;
    }
}

constexpr bool isScc(Mnemonic m) {
    switch (m) {
    case Mnemonic::st: case Mnemonic::sf:
    case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
    case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
    case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
    case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
    case Mnemonic::sgt: case Mnemonic::sle:
        return true;
    default:
        return false;
    }
}

constexpr bool isShift(Mnemonic m) {
    switch (m) {
    case Mnemonic::asl: case Mnemonic::asr:
    case Mnemonic::lsl: case Mnemonic::lsr:
    case Mnemonic::rol: case Mnemonic::ror:
    case Mnemonic::roxl: case Mnemonic::roxr:
        return true;
    default:
        return false;
    }
}

constexpr bool isExit(Mnemonic m) {
    return m == Mnemonic::rts || m == Mnemonic::rte || m == Mnemonic::rtr;
}

// True for instructions that end a basic block (branches, jumps, returns).
constexpr bool isBlockTerminator(Mnemonic m) {
    return isBranch(m) || isDbcc(m) || m == Mnemonic::jmp || isExit(m);
}

constexpr bool isMemoryMode(AddrMode m) {
    switch (m) {
    case AddrMode::ind: case AddrMode::postInc: case AddrMode::preDec:
    case AddrMode::disp: case AddrMode::index:
    case AddrMode::absW: case AddrMode::absL:
    case AddrMode::pcDisp: case AddrMode::pcIndex:
        return true;
    default:
        return false;
    }
}

constexpr bool isRegDirect(AddrMode m) {
    return m == AddrMode::dn || m == AddrMode::an;
}

// True for modes with displacement or address extension words in the
// instruction stream (used by 040/060 for pipeline interlock costs).
constexpr bool hasExtWord(AddrMode m) {
    switch (m) {
    case AddrMode::disp: case AddrMode::index:
    case AddrMode::absW: case AddrMode::absL:
    case AddrMode::pcDisp: case AddrMode::pcIndex:
        return true;
    default:
        return false;
    }
}

constexpr bool isLong(std::optional<OpSize> sz) {
    return sz && *sz == OpSize::long_;
}

constexpr bool isMem(const EffectiveAddr& ea) {
    return isMemoryMode(ea.mode);
}

// Get the operand for single-operand instructions: parser puts it in src,
// but two-operand forms use dst. Prefer dst, fall back to src.
inline const EffectiveAddr* getOp(const Instruction& inst) {
    if (inst.dst) return &*inst.dst;
    if (inst.src) return &*inst.src;
    return nullptr;
}

// Maps addressing modes to indices 0-11 for EA timing table lookups.
constexpr int eaIndex(AddrMode m) {
    switch (m) {
    case AddrMode::dn:      return 0;
    case AddrMode::an:      return 1;
    case AddrMode::ind:     return 2;
    case AddrMode::postInc: return 3;
    case AddrMode::preDec:  return 4;
    case AddrMode::disp:    return 5;
    case AddrMode::index:   return 6;
    case AddrMode::absW:    return 7;
    case AddrMode::absL:    return 8;
    case AddrMode::pcDisp:  return 9;
    case AddrMode::pcIndex: return 10;
    case AddrMode::imm:     return 11;
    default:                return 0;
    }
}

constexpr int sizeIdx(std::optional<OpSize> sz) {
    return (sz && *sz == OpSize::long_) ? 1 : 0;
}

// Count 01/10 transitions for MULS immediate timing
constexpr int mulsBitTransitions(uint16_t v) {
    uint32_t ext = v | (v & 0x8000 ? 0xFFFF0000u : 0);
    return std::popcount(ext ^ (ext >> 1)) & 0x1F;
}

// Count registers in a MOVEM register list mask
inline int movemRegCount(const Instruction& inst) {
    if (inst.src && inst.src->mode == AddrMode::regList) {
        return std::popcount(inst.src->regMask);
    }
    if (inst.dst && inst.dst->mode == AddrMode::regList) {
        return std::popcount(inst.dst->regMask);
    }
    return 0;
}

// Unified register space: 0-7 = D0-D7, 8-15 = A0-A7
constexpr uint16_t regBit(int n) { return uint16_t(1 << n); }

// Extract immediate value as uint16_t for bit-counting helpers
constexpr uint16_t immWord(const EffectiveAddr& ea) {
    return static_cast<uint16_t>(ea.ref.value & 0xFFFF);
}
