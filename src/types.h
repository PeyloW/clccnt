#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Instruction operand size; long_ has trailing underscore to avoid keyword collision.
enum class OpSize { byte, word, long_ };

// MC68k addressing modes plus pseudo-modes for register lists and special registers.
enum class AddrMode {
    dn, an,
    ind, postInc, preDec,
    disp, index,
    absW, absL,
    pcDisp, pcIndex,
    imm,
    regList,
    sr, ccr, usp,
};

// MC68k instruction mnemonics. Includes auto-promoted variants (moveToSR, etc.)
// that the parser creates from plain MOVE when special register operands are detected.
enum class Mnemonic {
    // Data movement
    move, movea, moveq, movem, movep,
    lea, pea, exg, swap, link, unlk,
    // Arithmetic
    add, adda, addi, addq, addx,
    sub, suba, subi, subq, subx,
    mulu, muls, divu, divs,
    neg, negx, clr, ext, chk,
    cmp, cmpa, cmpi, cmpm, tst,
    // Logic
    and_, andi, or_, ori, eor, eori, not_,
    // Shift/Rotate
    asl, asr, lsl, lsr, rol, ror, roxl, roxr,
    // Bit manipulation
    btst, bset, bclr, bchg,
    // Bit field (020+)
    bfchg, bfclr, bfexts, bfextu, bfffo, bfins, bfset, bftst,
    // Branches
    bra, bsr,
    bhi, bls, bcc, bcs, bne, beq,
    bvc, bvs, bpl, bmi, bge, blt, bgt, ble,
    // DBcc
    dbt, dbf,
    dbhi, dbls, dbcc, dbcs, dbne, dbeq,
    dbvc, dbvs, dbpl, dbmi, dbge, dblt, dbgt, dble,
    // Scc
    st, sf,
    shi, sls, scc, scs, sne, seq,
    svc, svs, spl, smi, sge, slt, sgt, sle,
    // Jump
    jmp, jsr,
    // System
    rts, rte, rtr, nop, reset, stop,
    trap, trapv, illegal,
    tas,
    // Special moves (auto-promoted from Move)
    moveToSR, moveFromSR, moveToCCR, moveUSP,
    // BCD
    abcd, sbcd, nbcd,

    count_,
};

// Maps a Mnemonic enum value to its assembly-language string.
const char* mnemonicName(Mnemonic m);

// A numeric or symbolic reference used in operands (displacement, immediate, or label).
struct Ref {
    int64_t value = 0;
    std::string label;
    std::string toString() const {
        return label.empty() ? std::to_string(value) : label;
    }
};

// A parsed MC68k operand: addressing mode, register(s), displacement/immediate.
// The indexReg field uses a unified 0-15 space where 0-7 are Dn and 8-15 are An.
struct EffectiveAddr {
    AddrMode mode;
    int reg = 0;
    int indexReg = 0;  // 0-7 = Dn, 8-15 = An
    OpSize indexSize = OpSize::word;
    Ref ref;
    uint16_t regMask = 0;

    std::string toString() const;
    void validate() const;
};

// A parsed MC68k instruction with mnemonic, optional size, and up to two operands.
struct Instruction {
    Mnemonic mnemonic;
    std::optional<OpSize> size;
    std::optional<EffectiveAddr> src;
    std::optional<EffectiveAddr> dst;
    std::string toString() const;
    void validate() const;
};

// Two-channel timing result. For most instructions a == b. For branches,
// a = taken/loop cost and b = not-taken/fall-through cost, so a may exceed b.
struct Timing {
    int a = 0;
    int b = 0;
    int min() const { return a < b ? a : b; }
    int max() const { return a > b ? a : b; }
    Timing& operator+=(Timing rhs) { a += rhs.a; b += rhs.b; return *this; }
    Timing operator+(Timing rhs) const { return {a + rhs.a, b + rhs.b}; }
    Timing operator+(int v) const { return {a + v, b + v}; }
    Timing operator*(int v) const { return {a * v, b * v}; }
};
