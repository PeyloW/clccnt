#include "timing.h"
#include "m68k.h"
#include "error.h"
#include <bit>

namespace {

// MC68000 EA fetch timing: cycles to read an operand [word/byte, long] per mode.
constexpr int eaFetchTable[12][2] = {
    { 0,  0}, // Dn
    { 0,  0}, // An
    { 4,  8}, // (An)
    { 4,  8}, // (An)+
    { 6, 10}, // -(An)
    { 8, 12}, // d(An)
    {10, 14}, // d(An,Xn)
    { 8, 12}, // xxx.W
    {12, 16}, // xxx.L
    { 8, 12}, // d(PC)
    {10, 14}, // d(PC,Xn)
    { 4,  8}, // #imm
};

// MC68000 EA store timing: cycles to write an operand [word/byte, long] per mode.
constexpr int eaStoreTable[12][2] = {
    { 0,  0}, // Dn
    { 0,  0}, // An
    { 4,  8}, // (An)
    { 4,  8}, // (An)+
    { 4,  8}, // -(An)
    { 8, 12}, // d(An)
    {10, 14}, // d(An,Xn)
    { 8, 12}, // xxx.W
    {12, 16}, // xxx.L
    { 0,  0}, // d(PC)
    { 0,  0}, // d(PC,Xn)
    { 0,  0}, // #imm
};

int eaFetch(const EffectiveAddr& ea, std::optional<OpSize> sz) {
    return eaFetchTable[eaIndex(ea.mode)][sizeIdx(sz)];
}

int eaStore(const EffectiveAddr& ea, std::optional<OpSize> sz) {
    return eaStoreTable[eaIndex(ea.mode)][sizeIdx(sz)];
}

// MC68000 timing constants (4-cycle bus, 16-bit bus width).
// Simple switch constants
constexpr int moveBase = 4;
constexpr int moveqCycles = 4;
constexpr int movepCycles[2] = {16, 24};
constexpr int linkCycles = 16, unlkCycles = 12;
constexpr int exgCycles = 6, swapCycles = 4, extCycles = 4;
constexpr int braCycles = 10, bsrCycles = 18;
constexpr int dbtCycles = 12, dbccLoop = 10, dbccFall = 14;
constexpr int rtsCycles = 16, rteCycles = 20, rtrCycles = 20;
constexpr int nopCycles = 4, resetCycles = 132, stopCycles = 4;
constexpr int trapCycles = 34, trapvMax = 34, illegalCycles = 34;
constexpr int moveUSPCycles = 4;
constexpr int cmpmCycles[2] = {12, 20};
constexpr int abcdMem = 18, abcdReg = 6;
constexpr int chkBase = 10;
constexpr int defaultCycles = 4;

// EOR
constexpr int eorMem[2] = {8, 12}, eorReg[2] = {4, 8};

// Immediate ALU
constexpr int immSRCCR = 20;
constexpr int immMem[2] = {12, 20}, immReg[2] = {8, 16};

// ADDQ/SUBQ
constexpr int quickAn = 8;
constexpr int quickMem[2] = {8, 12}, quickReg[2] = {4, 8};

// Unary (NEG/NEGX/NOT/CLR)
constexpr int unaryMem[2] = {8, 12}, unaryReg[2] = {4, 6};

// MUL
constexpr int mulBase = 38, mulMax = 70;

// DIV
constexpr int divuMin = 76, divuMax = 140;
constexpr int divsMin = 120, divsMax = 158;

// ADDX/SUBX
constexpr int addxMem[2] = {18, 30}, addxReg[2] = {4, 8};

// TST
constexpr int tstBase = 4;

// TAS
constexpr int tasMemBase = 10, tasReg = 4;

// Move SR/CCR
constexpr int moveToSRBase = 12, moveToCCRBase = 12;
constexpr int moveFromSRMem = 8, moveFromSRReg = 6;

// NBCD
constexpr int nbcdMem = 8, nbcdReg = 6;

// LEA/PEA/JMP/JSR mode tables
// [ind, disp, index, absW, absL, pcDisp, pcIndex, default]
constexpr int leaTable[8] = {4, 8, 12, 8, 12, 8, 12, 4};
constexpr int peaTable[8] = {12, 16, 20, 16, 20, 16, 20, 12};
constexpr int jmpTable[8] = {8, 10, 14, 10, 12, 10, 14, 8};
constexpr int jsrTable[8] = {16, 18, 22, 18, 20, 18, 22, 16};

// Looks up timing from a mode-indexed table for LEA/PEA/JMP/JSR.
int timeModeSwitch(const Instruction& inst, const int table[8]) {
    if (!inst.src) return table[7];
    switch (inst.src->mode) {
    case AddrMode::ind:     return table[0];
    case AddrMode::disp:    return table[1];
    case AddrMode::index:   return table[2];
    case AddrMode::absW:    return table[3];
    case AddrMode::absL:    return table[4];
    case AddrMode::pcDisp:  return table[5];
    case AddrMode::pcIndex: return table[6];
    default:                return table[7];
    }
}

int timeMove(const Instruction& inst) {
    if (!inst.src || !inst.dst) return moveBase;
    return moveBase + eaFetch(*inst.src, inst.size) +
                      eaStore(*inst.dst, inst.size);
}

int timeEor(const Instruction& inst) {
    if (!inst.dst) return 4;
    if (isMem(*inst.dst)) {
        return eorMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return eorReg[sizeIdx(inst.size)];
}

int timeImmediate(const Instruction& inst) {
    if (!inst.dst) return 8;
    if (inst.dst->mode == AddrMode::sr || inst.dst->mode == AddrMode::ccr) {
        return immSRCCR;
    }
    if (isMem(*inst.dst)) {
        return immMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return immReg[sizeIdx(inst.size)];
}

int timeQuick(const Instruction& inst) {
    if (!inst.dst) return 4;
    if (inst.dst->mode == AddrMode::an) return quickAn;
    if (isMem(*inst.dst)) {
        return quickMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return quickReg[sizeIdx(inst.size)];
}

int timeUnary(const Instruction& inst) {
    auto op = getOp(inst);
    if (!op) return 4;
    if (isMem(*op)) {
        return unaryMem[sizeIdx(inst.size)] + eaFetch(*op, inst.size);
    }
    return unaryReg[sizeIdx(inst.size)];
}

void timeMulu(const Instruction& inst, Timing& t) {
    int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
    if (inst.src && inst.src->mode == AddrMode::imm) {
        int bits = std::popcount(immWord(*inst.src));
        t.a = t.b = mulBase + 2 * bits + ea;
    } else {
        t.a = mulBase + ea;
        t.b = mulMax + ea;
    }
}

void timeMuls(const Instruction& inst, Timing& t) {
    int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
    if (inst.src && inst.src->mode == AddrMode::imm) {
        int transitions = mulsBitTransitions(immWord(*inst.src));
        t.a = t.b = mulBase + 2 * transitions + ea;
    } else {
        t.a = mulBase + ea;
        t.b = mulMax + ea;
    }
}

// MC68000 timing: non-pipelined, 4-cycle bus, 16-bit data bus.
class Timing000 : public TimingBase {
public:
    const char* name() const override { return "MC68000"; }

protected:
    Timing computeTime(const Instruction& inst) override {
        Timing t;

        switch (inst.mnemonic) {
        // --- Data movement ---
        case Mnemonic::move:
        case Mnemonic::movea:
            t.a = t.b = timeMove(inst);
            break;

        case Mnemonic::moveq:
            t.a = t.b = moveqCycles;
            break;

        case Mnemonic::movem:
            t.a = t.b = timeMovem(inst);
            break;

        case Mnemonic::movep:
            t.a = t.b = movepCycles[sizeIdx(inst.size)];
            break;

        case Mnemonic::lea:
            t.a = t.b = timeModeSwitch(inst, leaTable);
            break;

        case Mnemonic::pea:
            t.a = t.b = timeModeSwitch(inst, peaTable);
            break;

        case Mnemonic::exg:
            t.a = t.b = exgCycles;
            break;

        case Mnemonic::swap:
            t.a = t.b = swapCycles;
            break;

        case Mnemonic::link:
            t.a = t.b = linkCycles;
            break;

        case Mnemonic::unlk:
            t.a = t.b = unlkCycles;
            break;

        // --- Arithmetic ---
        case Mnemonic::add: case Mnemonic::sub:
        case Mnemonic::and_: case Mnemonic::or_:
        case Mnemonic::cmp:
            if (inst.src && inst.src->mode == AddrMode::imm) {
                t.a = t.b = timeImmediate(inst);
            } else {
                t.a = t.b = timeAlu(inst);
            }
            break;

        case Mnemonic::adda: case Mnemonic::suba:
            t.a = t.b = timeAdda(inst);
            break;

        case Mnemonic::cmpa:
            t.a = t.b = timeCmpa(inst);
            break;

        case Mnemonic::eor:
            if (inst.src && inst.src->mode == AddrMode::imm) {
                t.a = t.b = timeImmediate(inst);
            } else {
                t.a = t.b = timeEor(inst);
            }
            break;

        case Mnemonic::addi: case Mnemonic::subi:
        case Mnemonic::cmpi: case Mnemonic::andi:
        case Mnemonic::ori: case Mnemonic::eori:
            t.a = t.b = timeImmediate(inst);
            break;

        case Mnemonic::addq: case Mnemonic::subq:
            t.a = t.b = timeQuick(inst);
            break;

        case Mnemonic::addx: case Mnemonic::subx:
            if (inst.src && inst.src->mode == AddrMode::preDec) {
                t.a = t.b = addxMem[sizeIdx(inst.size)];
            } else {
                t.a = t.b = addxReg[sizeIdx(inst.size)];
            }
            break;

        // --- Multiply/Divide ---
        case Mnemonic::mulu:
            timeMulu(inst, t);
            break;

        case Mnemonic::muls:
            timeMuls(inst, t);
            break;

        case Mnemonic::divu: {
            int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
            t.a = divuMin + ea;
            t.b = divuMax + ea;
            break;
        }
        case Mnemonic::divs: {
            int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
            t.a = divsMin + ea;
            t.b = divsMax + ea;
            break;
        }

        // --- Unary ---
        case Mnemonic::neg: case Mnemonic::negx:
        case Mnemonic::not_: case Mnemonic::clr:
            t.a = t.b = timeUnary(inst);
            break;

        case Mnemonic::ext:
            t.a = t.b = extCycles;
            break;

        case Mnemonic::tst: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = tstBase + eaFetch(*op, inst.size);
            } else {
                t.a = t.b = tstBase;
            }
            break;
        }

        case Mnemonic::chk:
            if (inst.src) {
                t.a = t.b = chkBase + eaFetch(*inst.src, OpSize::word);
            } else {
                t.a = t.b = chkBase;
            }
            break;

        // --- Shift/Rotate ---
        case Mnemonic::asl: case Mnemonic::asr:
        case Mnemonic::lsl: case Mnemonic::lsr:
        case Mnemonic::rol: case Mnemonic::ror:
        case Mnemonic::roxl: case Mnemonic::roxr:
            timeShift(inst, t);
            break;

        // --- Bit manipulation ---
        case Mnemonic::btst:
            t.a = t.b = timeBtst(inst);
            break;

        case Mnemonic::bset: case Mnemonic::bclr: case Mnemonic::bchg:
            t.a = t.b = timeBitOp(inst);
            break;

        // --- Branch ---
        case Mnemonic::bra:
            t.a = t.b = braCycles;
            break;

        case Mnemonic::bsr:
            t.a = t.b = bsrCycles;
            break;

        case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
        case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
        case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
        case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
        case Mnemonic::bgt: case Mnemonic::ble:
            timeBcc(inst, t);
            break;

        // --- DBcc ---
        case Mnemonic::dbt:
            t.a = t.b = dbtCycles;
            break;

        case Mnemonic::dbf:
        case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
        case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
        case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
        case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
        case Mnemonic::dbgt: case Mnemonic::dble:
            t.a = dbccLoop;
            t.b = dbccFall;
            break;

        // --- Scc ---
        case Mnemonic::st: case Mnemonic::sf:
        case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
        case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
        case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
        case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
        case Mnemonic::sgt: case Mnemonic::sle:
            timeScc(inst, t);
            break;

        // --- Jump ---
        case Mnemonic::jmp:
            t.a = t.b = timeModeSwitch(inst, jmpTable);
            break;

        case Mnemonic::jsr:
            t.a = t.b = timeModeSwitch(inst, jsrTable);
            break;

        // --- System ---
        case Mnemonic::rts:
            t.a = t.b = rtsCycles;
            break;

        case Mnemonic::rte:
            t.a = t.b = rteCycles;
            break;

        case Mnemonic::rtr:
            t.a = t.b = rtrCycles;
            break;

        case Mnemonic::nop:
            t.a = t.b = nopCycles;
            break;

        case Mnemonic::reset:
            t.a = t.b = resetCycles;
            break;

        case Mnemonic::stop:
            t.a = t.b = stopCycles;
            break;

        case Mnemonic::trap:
            t.a = t.b = trapCycles;
            break;

        case Mnemonic::trapv:
            t.a = 4;
            t.b = trapvMax;
            break;

        case Mnemonic::illegal:
            t.a = t.b = illegalCycles;
            break;

        case Mnemonic::tas: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = tasMemBase + eaFetch(*op, OpSize::byte);
            } else {
                t.a = t.b = tasReg;
            }
            break;
        }

        // --- Special moves ---
        case Mnemonic::moveToSR:
            if (inst.src) {
                t.a = t.b = moveToSRBase + eaFetch(*inst.src, OpSize::word);
            } else {
                t.a = t.b = moveToSRBase;
            }
            break;

        case Mnemonic::moveFromSR: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = moveFromSRMem + eaFetch(*op, OpSize::word);
            } else {
                t.a = t.b = moveFromSRReg;
            }
            break;
        }

        case Mnemonic::moveToCCR:
            if (inst.src) {
                t.a = t.b = moveToCCRBase + eaFetch(*inst.src, OpSize::word);
            } else {
                t.a = t.b = moveToCCRBase;
            }
            break;

        case Mnemonic::moveUSP:
            t.a = t.b = moveUSPCycles;
            break;

        // --- BCD ---
        case Mnemonic::abcd: case Mnemonic::sbcd:
            if (inst.src && inst.src->mode == AddrMode::preDec) {
                t.a = t.b = abcdMem;
            } else {
                t.a = t.b = abcdReg;
            }
            break;

        case Mnemonic::nbcd: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = nbcdMem + eaFetch(*op, OpSize::byte);
            } else {
                t.a = t.b = nbcdReg;
            }
            break;
        }

        case Mnemonic::cmpm:
            t.a = t.b = cmpmCycles[sizeIdx(inst.size)];
            break;

        // --- Bit field (020+, not on 68000, but handle gracefully) ---
        case Mnemonic::bftst: case Mnemonic::bfextu: case Mnemonic::bfexts:
        case Mnemonic::bfffo: case Mnemonic::bfchg: case Mnemonic::bfclr:
        case Mnemonic::bfset: case Mnemonic::bfins:
            t.a = t.b = defaultCycles;
            break;

        default:
            t.a = t.b = defaultCycles;
            break;
        }

        if (t.a < 0 || t.b < 0) {
            throw Error{2, "internal: negative timing for '" + inst.toString() +
                "': min=" + std::to_string(t.a) + " max=" + std::to_string(t.b)};
        }

        return t;
    }

private:
    int timeMovem(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 12;
        int regCount = movemRegCount(inst);
        if (regCount == 0) return 12;

        bool toMem = (inst.src->mode == AddrMode::regList);
        const EffectiveAddr* mem = toMem ? &*inst.dst : &*inst.src;
        int perReg = isLong(inst.size) ? 8 : 4;

        if (toMem) {
            int eaExtra = 0;
            if (mem->mode == AddrMode::disp || mem->mode == AddrMode::absW ||
                mem->mode == AddrMode::absL) {
                eaExtra = (mem->mode == AddrMode::absL) ? 8 : 4;
            } else if (mem->mode == AddrMode::index) {
                eaExtra = 6;
            }
            return 8 + regCount * perReg + eaExtra;
        } else {
            int eaExtra = 0;
            if (mem->mode == AddrMode::disp || mem->mode == AddrMode::absW ||
                mem->mode == AddrMode::pcDisp) {
                eaExtra = 4;
            } else if (mem->mode == AddrMode::absL) {
                eaExtra = 8;
            } else if (mem->mode == AddrMode::index ||
                       mem->mode == AddrMode::pcIndex) {
                eaExtra = 6;
            }
            return 12 + regCount * perReg + eaExtra;
        }
    }

    int timeAlu(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 4;
        if (inst.mnemonic == Mnemonic::cmp) {
            if (isLong(inst.size)) {
                if (isRegDirect(inst.src->mode)) return 6;
                return 6 + eaFetch(*inst.src, inst.size);
            }
            return 4 + eaFetch(*inst.src, inst.size);
        }
        if (inst.dst->mode != AddrMode::dn && isMem(*inst.dst)) {
            int base = isLong(inst.size) ? 12 : 8;
            return base + eaFetch(*inst.dst, inst.size);
        }
        if (isLong(inst.size)) {
            if (isRegDirect(inst.src->mode) || inst.src->mode == AddrMode::imm) {
                return 8 + eaFetch(*inst.src, inst.size);
            }
            return 6 + eaFetch(*inst.src, inst.size);
        }
        return 4 + eaFetch(*inst.src, inst.size);
    }

    int timeAdda(const Instruction& inst) {
        if (!inst.src) return 8;
        if (isLong(inst.size)) {
            if (isRegDirect(inst.src->mode) || inst.src->mode == AddrMode::imm) {
                return 8 + eaFetch(*inst.src, inst.size);
            }
            return 6 + eaFetch(*inst.src, inst.size);
        }
        return 8 + eaFetch(*inst.src, OpSize::word);
    }

    int timeCmpa(const Instruction& inst) {
        if (!inst.src) return 6;
        return 6 + eaFetch(*inst.src, inst.size);
    }

    void timeShift(const Instruction& inst, Timing& t) {
        auto op = getOp(inst);
        if (op && isMem(*op) && (!inst.dst || !inst.src)) {
            t.a = t.b = 8 + eaFetch(*op, OpSize::word);
            return;
        }
        int base = isLong(inst.size) ? 8 : 6;
        if (inst.src && inst.src->mode == AddrMode::imm) {
            int count = int(inst.src->ref.value);
            if (count < 1) count = 1;
            if (count > 63) count = 63;
            t.a = t.b = base + 2 * count;
        } else {
            t.a = base;
            t.b = base + 2 * 63;
        }
    }

    int timeBtst(const Instruction& inst) {
        if (!inst.dst) return 4;
        if (isMem(*inst.dst)) {
            int base = (inst.src && inst.src->mode == AddrMode::imm) ? 8 : 4;
            return base + eaFetch(*inst.dst, OpSize::byte);
        }
        return (inst.src && inst.src->mode == AddrMode::imm) ? 10 : 6;
    }

    int timeBitOp(const Instruction& inst) {
        if (!inst.dst) return 8;
        if (isMem(*inst.dst)) {
            int base = (inst.src && inst.src->mode == AddrMode::imm) ? 12 : 8;
            return base + eaFetch(*inst.dst, OpSize::byte);
        }
        if (inst.mnemonic == Mnemonic::bclr)
            return (inst.src && inst.src->mode == AddrMode::imm) ? 14 : 10;
        return (inst.src && inst.src->mode == AddrMode::imm) ? 12 : 8;
    }

    void timeScc(const Instruction& inst, Timing& t) {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = 8 + eaFetch(*op, OpSize::byte);
            return;
        }
        if (inst.mnemonic == Mnemonic::st) {
            t.a = t.b = 6;
        } else if (inst.mnemonic == Mnemonic::sf) {
            t.a = t.b = 4;
        } else {
            t.a = 4;
            t.b = 6;
        }
    }

    void timeBcc(const Instruction& inst, Timing& t) {
        bool isByte = (inst.size && *inst.size == OpSize::byte);
        t.a = 10;
        t.b = isByte ? 8 : 12;
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming000() {
    return std::make_unique<Timing000>();
}
