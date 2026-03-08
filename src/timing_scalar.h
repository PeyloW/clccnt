#pragma once

#include "timing.h"

// Timing constants for non-pipelined CPUs (000/020). Each CPU model provides
// its own constant set; the shared TimingScalar logic uses these to compute cycles.
struct ScalarConst {
    // Simple switch constants
    int moveBase;
    int moveqCycles;
    int movepCycles[2]; // [bw, l]
    int linkCycles, unlkCycles;
    int exgCycles, swapCycles, extCycles;
    int braCycles, bsrCycles;
    int dbtCycles, dbccLoop, dbccFall;
    int rtsCycles, rteCycles, rtrCycles;
    int nopCycles, resetCycles, stopCycles;
    int trapCycles, trapvMax, illegalCycles;
    int moveUSPCycles;
    int cmpmCycles[2]; // [bw, l]
    int abcdMem, abcdReg;
    int chkBase;
    int defaultCycles;

    // EOR
    int eorDefault;
    int eorMem[2]; // [bw, l]
    int eorReg[2];

    // Immediate ALU
    int immDefault, immSRCCR;
    int immMem[2]; // [bw, l]
    int immReg[2];

    // ADDQ/SUBQ
    int quickDefault, quickAn;
    int quickMem[2]; // [bw, l]
    int quickReg[2];

    // Unary (NEG/NEGX/NOT/CLR)
    int unaryDefault;
    int unaryMem[2]; // [bw, l]
    int unaryReg[2];

    // MUL
    int mulBase, mulMax;

    // DIV
    int divuMin, divuMax;
    int divsMin, divsMax;

    // ADDX/SUBX
    int addxMem[2]; // [bw, l]
    int addxReg[2];

    // TST
    int tstBase;

    // TAS
    int tasMemBase, tasReg;

    // Move SR/CCR
    int moveToSRBase, moveToCCRBase;
    int moveFromSRMem, moveFromSRReg;

    // NBCD
    int nbcdMem, nbcdReg;

    // LEA/PEA/JMP/JSR mode tables
    // [ind, disp, index, absW, absL, pcDisp, pcIndex, default]
    int lea[8], pea[8], jmp[8], jsr[8];
};

// Base class for non-pipelined CPU timing (000/020). Uses EA fetch/store tables
// plus ScalarConst to compute instruction cycles. Subclasses override the pure
// virtual methods for instructions where 000 and 020 timing genuinely differs.
class TimingScalar : public TimingBase {
protected:
    const int (*_fetchTable)[2];
    const int (*_storeTable)[2];
    const ScalarConst _c;

    TimingScalar(const int (*fetch)[2], const int (*store)[2], ScalarConst c);

    // EA cost lookup
    int eaFetch(const EffectiveAddr& ea, std::optional<OpSize> sz) const;
    int eaStore(const EffectiveAddr& ea, std::optional<OpSize> sz) const;

    // Parameterized methods (non-virtual)
    int timeMove(const Instruction& inst) const;
    int timeModeSwitch(const Instruction& inst, const int table[8]) const;
    int timeEor(const Instruction& inst) const;
    int timeImmediate(const Instruction& inst) const;
    int timeQuick(const Instruction& inst) const;
    int timeUnary(const Instruction& inst) const;
    void timeMulu(const Instruction& inst, Timing& t) const;
    void timeMuls(const Instruction& inst, Timing& t) const;

    // Virtual methods for genuinely different logic
    virtual int timeMovem(const Instruction& inst) = 0;
    virtual int timeAlu(const Instruction& inst) = 0;
    virtual int timeAdda(const Instruction& inst) = 0;
    virtual int timeCmpa(const Instruction& inst) = 0;
    virtual void timeShift(const Instruction& inst, Timing& t) = 0;
    virtual int timeBtst(const Instruction& inst) = 0;
    virtual int timeBitOp(const Instruction& inst) = 0;
    virtual void timeScc(const Instruction& inst, Timing& t) = 0;
    virtual void timeBcc(const Instruction& inst, Timing& t) = 0;

public:
    Timing computeTime(const Instruction& inst) override;
};
