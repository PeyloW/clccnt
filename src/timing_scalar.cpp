#include "timing_scalar.h"
#include "m68k.h"
#include "error.h"
#include <bit>

TimingScalar::TimingScalar(const int (*fetch)[2], const int (*store)[2],
                           ScalarConst c)
    : _fetchTable(fetch), _storeTable(store), _c(c) {}

int TimingScalar::eaFetch(const EffectiveAddr& ea,
                          std::optional<OpSize> sz) const {
    return _fetchTable[eaIndex(ea.mode)][sizeIdx(sz)];
}

int TimingScalar::eaStore(const EffectiveAddr& ea,
                          std::optional<OpSize> sz) const {
    return _storeTable[eaIndex(ea.mode)][sizeIdx(sz)];
}

int TimingScalar::timeMove(const Instruction& inst) const {
    if (!inst.src || !inst.dst) return _c.moveBase;
    return _c.moveBase + eaFetch(*inst.src, inst.size) +
                         eaStore(*inst.dst, inst.size);
}

// Looks up timing from a mode-indexed table for LEA/PEA/JMP/JSR.
int TimingScalar::timeModeSwitch(const Instruction& inst,
                                 const int table[8]) const {
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

int TimingScalar::timeEor(const Instruction& inst) const {
    if (!inst.dst) return _c.eorDefault;
    if (isMem(*inst.dst)) {
        return _c.eorMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return _c.eorReg[sizeIdx(inst.size)];
}

int TimingScalar::timeImmediate(const Instruction& inst) const {
    if (!inst.dst) return _c.immDefault;
    if (inst.dst->mode == AddrMode::sr || inst.dst->mode == AddrMode::ccr) {
        return _c.immSRCCR;
    }
    if (isMem(*inst.dst)) {
        return _c.immMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return _c.immReg[sizeIdx(inst.size)];
}

int TimingScalar::timeQuick(const Instruction& inst) const {
    if (!inst.dst) return _c.quickDefault;
    if (inst.dst->mode == AddrMode::an) return _c.quickAn;
    if (isMem(*inst.dst)) {
        return _c.quickMem[sizeIdx(inst.size)] + eaFetch(*inst.dst, inst.size);
    }
    return _c.quickReg[sizeIdx(inst.size)];
}

int TimingScalar::timeUnary(const Instruction& inst) const {
    auto op = getOp(inst);
    if (!op) return _c.unaryDefault;
    if (isMem(*op)) {
        return _c.unaryMem[sizeIdx(inst.size)] + eaFetch(*op, inst.size);
    }
    return _c.unaryReg[sizeIdx(inst.size)];
}

void TimingScalar::timeMulu(const Instruction& inst, Timing& t) const {
    int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
    if (inst.src && inst.src->mode == AddrMode::imm) {
        int bits = std::popcount(immWord(*inst.src));
        t.a = t.b = _c.mulBase + 2 * bits + ea;
    } else {
        t.a = _c.mulBase + ea;
        t.b = _c.mulMax + ea;
    }
}

void TimingScalar::timeMuls(const Instruction& inst, Timing& t) const {
    int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
    if (inst.src && inst.src->mode == AddrMode::imm) {
        int transitions = mulsBitTransitions(immWord(*inst.src));
        t.a = t.b = _c.mulBase + 2 * transitions + ea;
    } else {
        t.a = _c.mulBase + ea;
        t.b = _c.mulMax + ea;
    }
}

Timing TimingScalar::computeTime(const Instruction& inst) {
    Timing t;

    switch (inst.mnemonic) {
    // --- Data movement ---
    case Mnemonic::move:
    case Mnemonic::movea:
        t.a = t.b = timeMove(inst);
        break;

    case Mnemonic::moveq:
        t.a = t.b = _c.moveqCycles;
        break;

    case Mnemonic::movem:
        t.a = t.b = timeMovem(inst);
        break;

    case Mnemonic::movep:
        t.a = t.b = _c.movepCycles[sizeIdx(inst.size)];
        break;

    case Mnemonic::lea:
        t.a = t.b = timeModeSwitch(inst, _c.lea);
        break;

    case Mnemonic::pea:
        t.a = t.b = timeModeSwitch(inst, _c.pea);
        break;

    case Mnemonic::exg:
        t.a = t.b = _c.exgCycles;
        break;

    case Mnemonic::swap:
        t.a = t.b = _c.swapCycles;
        break;

    case Mnemonic::link:
        t.a = t.b = _c.linkCycles;
        break;

    case Mnemonic::unlk:
        t.a = t.b = _c.unlkCycles;
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
            t.a = t.b = _c.addxMem[sizeIdx(inst.size)];
        } else {
            t.a = t.b = _c.addxReg[sizeIdx(inst.size)];
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
        t.a = _c.divuMin + ea;
        t.b = _c.divuMax + ea;
        break;
    }
    case Mnemonic::divs: {
        int ea = inst.src ? eaFetch(*inst.src, OpSize::word) : 0;
        t.a = _c.divsMin + ea;
        t.b = _c.divsMax + ea;
        break;
    }

    // --- Unary ---
    case Mnemonic::neg: case Mnemonic::negx:
    case Mnemonic::not_: case Mnemonic::clr:
        t.a = t.b = timeUnary(inst);
        break;

    case Mnemonic::ext:
        t.a = t.b = _c.extCycles;
        break;

    case Mnemonic::tst: {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = _c.tstBase + eaFetch(*op, inst.size);
        } else {
            t.a = t.b = _c.tstBase;
        }
        break;
    }

    case Mnemonic::chk:
        if (inst.src) {
            t.a = t.b = _c.chkBase + eaFetch(*inst.src, OpSize::word);
        } else {
            t.a = t.b = _c.chkBase;
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
        t.a = t.b = _c.braCycles;
        break;

    case Mnemonic::bsr:
        t.a = t.b = _c.bsrCycles;
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
        t.a = t.b = _c.dbtCycles;
        break;

    case Mnemonic::dbf:
    case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
    case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
    case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
    case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
    case Mnemonic::dbgt: case Mnemonic::dble:
        t.a = _c.dbccLoop;
        t.b = _c.dbccFall;
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
        t.a = t.b = timeModeSwitch(inst, _c.jmp);
        break;

    case Mnemonic::jsr:
        t.a = t.b = timeModeSwitch(inst, _c.jsr);
        break;

    // --- System ---
    case Mnemonic::rts:
        t.a = t.b = _c.rtsCycles;
        break;

    case Mnemonic::rte:
        t.a = t.b = _c.rteCycles;
        break;

    case Mnemonic::rtr:
        t.a = t.b = _c.rtrCycles;
        break;

    case Mnemonic::nop:
        t.a = t.b = _c.nopCycles;
        break;

    case Mnemonic::reset:
        t.a = t.b = _c.resetCycles;
        break;

    case Mnemonic::stop:
        t.a = t.b = _c.stopCycles;
        break;

    case Mnemonic::trap:
        t.a = t.b = _c.trapCycles;
        break;

    case Mnemonic::trapv:
        t.a = 4;
        t.b = _c.trapvMax;
        break;

    case Mnemonic::illegal:
        t.a = t.b = _c.illegalCycles;
        break;

    case Mnemonic::tas: {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = _c.tasMemBase + eaFetch(*op, OpSize::byte);
        } else {
            t.a = t.b = _c.tasReg;
        }
        break;
    }

    // --- Special moves ---
    case Mnemonic::moveToSR:
        if (inst.src) {
            t.a = t.b = _c.moveToSRBase + eaFetch(*inst.src, OpSize::word);
        } else {
            t.a = t.b = _c.moveToSRBase;
        }
        break;

    case Mnemonic::moveFromSR: {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = _c.moveFromSRMem + eaFetch(*op, OpSize::word);
        } else {
            t.a = t.b = _c.moveFromSRReg;
        }
        break;
    }

    case Mnemonic::moveToCCR:
        if (inst.src) {
            t.a = t.b = _c.moveToCCRBase + eaFetch(*inst.src, OpSize::word);
        } else {
            t.a = t.b = _c.moveToCCRBase;
        }
        break;

    case Mnemonic::moveUSP:
        t.a = t.b = _c.moveUSPCycles;
        break;

    // --- BCD ---
    case Mnemonic::abcd: case Mnemonic::sbcd:
        if (inst.src && inst.src->mode == AddrMode::preDec) {
            t.a = t.b = _c.abcdMem;
        } else {
            t.a = t.b = _c.abcdReg;
        }
        break;

    case Mnemonic::nbcd: {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = _c.nbcdMem + eaFetch(*op, OpSize::byte);
        } else {
            t.a = t.b = _c.nbcdReg;
        }
        break;
    }

    case Mnemonic::cmpm:
        t.a = t.b = _c.cmpmCycles[sizeIdx(inst.size)];
        break;

    // --- Bit field (020+) ---
    case Mnemonic::bftst: {
        int ea = inst.src ? eaFetch(*inst.src, OpSize::long_) : 0;
        t.a = 6 + ea;
        t.b = 18 + ea;
        break;
    }
    case Mnemonic::bfextu: case Mnemonic::bfexts: {
        int ea = inst.src ? eaFetch(*inst.src, OpSize::long_) : 0;
        t.a = 8 + ea;
        t.b = 19 + ea;
        break;
    }
    case Mnemonic::bfffo: {
        int ea = inst.src ? eaFetch(*inst.src, OpSize::long_) : 0;
        t.a = 18 + ea;
        t.b = 32 + ea;
        break;
    }
    case Mnemonic::bfchg: case Mnemonic::bfclr: case Mnemonic::bfset: {
        auto op = getOp(inst);
        int ea = op ? eaFetch(*op, OpSize::long_) : 0;
        t.a = 12 + ea;
        t.b = 22 + ea;
        break;
    }
    case Mnemonic::bfins: {
        int ea = inst.dst ? eaFetch(*inst.dst, OpSize::long_) : 0;
        t.a = 10 + ea;
        t.b = 21 + ea;
        break;
    }

    default:
        t.a = t.b = _c.defaultCycles;
        break;
    }

    if (t.a < 0 || t.b < 0) {
        throw Error{2, "internal: negative timing for '" + inst.toString() +
            "': min=" + std::to_string(t.a) + " max=" + std::to_string(t.b)};
    }

    return t;
}
