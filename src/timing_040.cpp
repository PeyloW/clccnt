#include "timing.h"
#include "m68k.h"

namespace {

// MC68040 timing from MC68040 User's Manual Section 10.
// Simplified flat model: base cost per instruction class + EA overhead for
// index and PC-relative modes. All timings assume cache hits (Section 10.2).
//
// The 040 pipeline (ea_calc → ea_fetch → execute → writeback) means most
// standard-mode instructions complete in 1 cycle. Index modes add +2 and
// PC-relative modes add +2/+4 due to pipeline interlocks.

// Pipeline overhead for complex addressing modes.
// Standard modes (Dn, An, (An), (An)+, -(An), d(An), abs, #imm) = 0.
int eaCost(AddrMode m) {
    switch (m) {
    case AddrMode::index:   return 2;  // d(An,Xn)
    case AddrMode::pcDisp:  return 2;  // d(PC)
    case AddrMode::pcIndex: return 4;  // d(PC,Xn)
    default:                return 0;
    }
}

// True for post-increment, pre-decrement, or displacement modes where
// immediate/quick ops to memory incur an extra pipeline cycle.
bool hasAddrUpdate(AddrMode m) {
    switch (m) {
    case AddrMode::postInc: case AddrMode::preDec:
    case AddrMode::disp:
        return true;
    default:
        return false;
    }
}

// MC68040 timing engine.
// Flat timing model with pipeline stall detection in time(span).
class Timing040 : public TimingBase {
public:
    // Skip bus rounding — 040 values are exact clock counts.
    Timing time(const Instruction& inst) override {
        return computeTime(inst);
    }

    // Pipeline-aware block timing: detect write→read stalls
    Timing time(std::span<const Instruction> block) override {
        Timing total;
        if (block.empty()) return total;

        total += time(block[0]);
        auto prev = regUsage(block[0]);

        for (size_t i = 1; i < block.size(); i++) {
            total += time(block[i]);

            // Check for pipeline stall: write-then-read on same register
            auto next = regUsage(block[i]);
            if (prev.write & next.read) {
                total += {1, 1};
            }
            prev = next;
        }
        return total;
    }

    std::vector<bool> stalls(std::span<const Instruction> block) override {
        std::vector<bool> result(block.size(), false);
        for (size_t i = 1; i < block.size(); i++) {
            auto prev = regUsage(block[i - 1]);
            auto next = regUsage(block[i]);
            if (prev.write & next.read) {
                result[i] = true;
            }
        }
        return result;
    }

    int busAccessCycles() const override { return 2; }
    int busWidth() const override { return 4; }
    const char* name() const override { return "MC68040"; }

    std::optional<CacheConfig> iCache() const override {
        return CacheConfig{4096, 16, 4};
    }
    std::optional<CacheConfig> dCache() const override {
        return CacheConfig{4096, 16, 4};
    }

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
            t.a = t.b = 1;
            break;

        case Mnemonic::movem:
            t.a = t.b = timeMovem(inst);
            break;

        case Mnemonic::movep:
            // MOVEP.W Dn→d(An): 11, d(An)→Dn: max(4,2L+5)=7
            // MOVEP.L Dn→d(An): 13, d(An)→Dn: max(8,2L+8)=10
            if (inst.dst && inst.dst->mode == AddrMode::disp) {
                // Register to memory
                t.a = t.b = isLong(inst.size) ? 13 : 11;
            } else {
                // Memory to register
                t.a = t.b = isLong(inst.size) ? 10 : 7;
            }
            break;

        case Mnemonic::lea:
            // LEA: (An)/abs=1, d(An)=2, d(An,Xn)=4, d(PC)=4
            if (inst.src) {
                t.a = t.b = 1 + (hasExtWord(inst.src->mode) ? 1 : 0)
                              + eaCost(inst.src->mode);
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::pea:
            // PEA: (An)/abs=2, d(An)=2, d(An,Xn)=4, d(PC)=4
            if (inst.src) {
                t.a = t.b = 2 + eaCost(inst.src->mode);
            } else {
                t.a = t.b = 2;
            }
            break;

        case Mnemonic::exg:
            // EXG Dy,Dx / Dy,Ax: 1; Ay,Ax: 2
            if (inst.src && inst.dst &&
                inst.src->mode == AddrMode::an && inst.dst->mode == AddrMode::an) {
                t.a = t.b = 2;
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::swap:
            t.a = t.b = 2;
            break;

        case Mnemonic::ext:
            // EXT.W: 2, EXT.L/EXTB.L: 1
            t.a = t.b = isLong(inst.size) ? 1 : 2;
            break;

        case Mnemonic::link:
            t.a = t.b = 3;
            break;

        case Mnemonic::unlk:
            t.a = t.b = 2;
            break;

        // --- Standard ALU (Section 10.6: ADD/AND/CMP/EOR/OR/SUB/TST) ---
        case Mnemonic::add: case Mnemonic::sub:
        case Mnemonic::and_: case Mnemonic::or_:
        case Mnemonic::eor: case Mnemonic::cmp:
        case Mnemonic::tst:
            t.a = t.b = timeAlu(inst);
            break;

        // ADDA/SUBA: base 2, +1 for addr-update modes (Section 10.6)
        case Mnemonic::adda: case Mnemonic::suba: {
            int base = 2;
            if (inst.src && hasAddrUpdate(inst.src->mode)) base = 3;
            t.a = t.b = base + (inst.src ? eaCost(inst.src->mode) : 0);
            break;
        }

        // CMPA: base 1, +1 for addr-update modes (Section 10.6)
        case Mnemonic::cmpa: {
            int base = 1;
            if (inst.src && hasAddrUpdate(inst.src->mode)) base = 2;
            t.a = t.b = base + (inst.src ? eaCost(inst.src->mode) : 0);
            break;
        }

        // --- Immediate ALU (Section 10.6: ADDI/ANDI/CMPI/EORI/ORI/SUBI) ---
        case Mnemonic::addi: case Mnemonic::subi:
        case Mnemonic::cmpi: case Mnemonic::andi:
        case Mnemonic::ori: case Mnemonic::eori:
            t.a = t.b = timeImmediate(inst);
            break;

        // --- Quick (Section 10.6: ADDQ/SUBQ) ---
        case Mnemonic::addq: case Mnemonic::subq:
            t.a = t.b = timeQuick(inst);
            break;

        // --- Multiprecision (Section 10.5) ---
        case Mnemonic::addx: case Mnemonic::subx:
            if (inst.src && inst.src->mode == AddrMode::preDec) {
                t.a = t.b = 3;  // -(Ay),-(Ax): calc=3, exec=1L+2=3
            } else {
                t.a = t.b = 1;  // Dy,Dx
            }
            break;

        case Mnemonic::abcd: case Mnemonic::sbcd:
            if (inst.src && inst.src->mode == AddrMode::preDec) {
                t.a = t.b = 4;  // -(Ay),-(Ax): calc=3, exec=1L+3=4
            } else {
                t.a = t.b = 3;  // Dy,Dx: calc=1, exec=3
            }
            break;

        case Mnemonic::cmpm:
            t.a = t.b = 3;  // calc=3, exec=1L+2=3
            break;

        // --- Multiply (Section 10.6) ---
        case Mnemonic::mulu:
            // MULU.W: 14, MULU.L: 20
            t.a = t.b = (isLong(inst.size) ? 20 : 14)
                       + (inst.src ? eaCost(inst.src->mode) : 0);
            break;

        case Mnemonic::muls:
            // MULS.W: 16, MULS.L: 20
            t.a = t.b = (isLong(inst.size) ? 20 : 16)
                       + (inst.src ? eaCost(inst.src->mode) : 0);
            break;

        // --- Divide (Section 10.6) ---
        case Mnemonic::divu: case Mnemonic::divs:
            // DIVS.W/DIVU.W: 27, DIVS.L/DIVU.L: 44
            t.a = t.b = (isLong(inst.size) ? 44 : 27)
                       + (inst.src ? eaCost(inst.src->mode) : 0);
            break;

        // --- Single operand (Section 10.6: NEG/NEGX/NOT/CLR) ---
        case Mnemonic::neg: case Mnemonic::negx:
        case Mnemonic::not_: case Mnemonic::clr: {
            auto* op = getOp(inst);
            t.a = t.b = 1 + (op ? eaCost(op->mode) : 0);
            break;
        }

        case Mnemonic::nbcd: {
            // NBCD Dn: 3, NBCD mem: 2
            auto* op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 2 + eaCost(op->mode);
            } else {
                t.a = t.b = 3;
            }
            break;
        }

        case Mnemonic::tas: {
            // TAS Dn: 2, TAS mem: 26 (bus lock, Section 10.6)
            auto* op = getOp(inst);
            t.a = t.b = (op && isMem(*op)) ? 26 : 2;
            break;
        }

        case Mnemonic::chk:
            // CHK: 8 + eaCost (within bounds, Section 10.6)
            t.a = t.b = 8 + (inst.src ? eaCost(inst.src->mode) : 0);
            break;

        // --- Scc (Section 10.6) ---
        case Mnemonic::st: case Mnemonic::sf:
        case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
        case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
        case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
        case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
        case Mnemonic::sgt: case Mnemonic::sle: {
            auto* op = getOp(inst);
            t.a = t.b = 2 + (op ? eaCost(op->mode) : 0);
            break;
        }

        // --- Shift/Rotate (Section 10.6) ---
        case Mnemonic::asl: case Mnemonic::asr:
        case Mnemonic::lsl: case Mnemonic::lsr:
        case Mnemonic::rol: case Mnemonic::ror:
        case Mnemonic::roxl: case Mnemonic::roxr:
            t.a = t.b = timeShift(inst);
            break;

        // --- Bit manipulation (Section 10.6) ---
        case Mnemonic::btst:
            // BTST: #imm=1, Dn=2 for register; same for mem + eaCost
            t.a = t.b = timeBitOp(inst, 1);
            break;

        case Mnemonic::bset: case Mnemonic::bclr: case Mnemonic::bchg:
            // BCHG/BCLR/BSET: #imm=3, Dn=4 for register; same for mem + eaCost
            t.a = t.b = timeBitOp(inst, 3);
            break;

        // --- Bit field (Section 10.6) ---
        case Mnemonic::bftst: case Mnemonic::bfextu: case Mnemonic::bfexts:
            t.a = t.b = timeBitField(inst, 4, 9);
            break;
        case Mnemonic::bfffo:
            t.a = t.b = timeBitField(inst, 6, 11);
            break;
        case Mnemonic::bfchg: case Mnemonic::bfclr: case Mnemonic::bfset:
            t.a = t.b = timeBitField(inst, 6, 9);
            break;
        case Mnemonic::bfins:
            t.a = t.b = timeBitField(inst, 5, 9);
            break;

        // --- Branches (Section 10.5) ---
        case Mnemonic::bra:
            t.a = t.b = 2;
            break;

        case Mnemonic::bsr:
            t.a = t.b = 2;
            break;

        case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
        case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
        case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
        case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
        case Mnemonic::bgt: case Mnemonic::ble:
            t.a = 2;   // taken
            t.b = 3;   // not taken
            break;

        // --- DBcc (Section 10.5) ---
        case Mnemonic::dbt:
            t.a = t.b = 4;  // cc True: calc=4, exec=4
            break;

        case Mnemonic::dbf:
        case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
        case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
        case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
        case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
        case Mnemonic::dbgt: case Mnemonic::dble:
            t.a = 3;   // loop back (count > -1)
            t.b = 4;   // fall through (count = -1)
            break;

        // --- JMP/JSR (Section 10.6) ---
        case Mnemonic::jmp:
        case Mnemonic::jsr:
            // (An)/abs: 3, d(An): 4, d(An,Xn): 6, d(PC): 6
            if (inst.src) {
                t.a = t.b = 3 + (hasExtWord(inst.src->mode) ? 1 : 0)
                              + eaCost(inst.src->mode);
            } else {
                t.a = t.b = 3;
            }
            break;

        // --- Returns (Section 10.5) ---
        case Mnemonic::rts:
            t.a = t.b = 5;
            break;

        case Mnemonic::rte:
            t.a = t.b = 15;  // typical (stack format $2)
            break;

        case Mnemonic::rtr:
            t.a = t.b = 7;
            break;

        // --- Special moves (Section 10.5/10.6) ---
        case Mnemonic::moveToSR:
            t.a = t.b = 9;   // Dn: calc=9, exec=1L+8=9
            break;

        case Mnemonic::moveToCCR:
            t.a = t.b = 2;   // calc=1, exec=2
            break;

        case Mnemonic::moveFromSR:
            t.a = t.b = 2;   // calc=2, exec=1L+2=3 → simplified to 2
            break;

        case Mnemonic::moveUSP:
            // MOVE An→USP: 7, MOVE USP→An: 3
            if (inst.src && inst.src->mode == AddrMode::an) {
                t.a = t.b = 7;   // An → USP
            } else {
                t.a = t.b = 3;   // USP → An
            }
            break;

        // --- Misc (Section 10.5) ---
        case Mnemonic::nop:
            t.a = t.b = 8;   // calc=8, exec=1L+7=8
            break;

        case Mnemonic::reset:
            t.a = t.b = 521;
            break;

        case Mnemonic::stop:
            t.a = t.b = 1;
            break;

        case Mnemonic::trap:
            t.a = t.b = 16;
            break;

        case Mnemonic::trapv:
            t.a = 5;    // not taken
            t.b = 19;   // taken (exception)
            break;

        case Mnemonic::illegal:
            t.a = t.b = 16;
            break;

        default:
            t.a = t.b = 1;
            break;
        }

        return t;
    }

private:
    // --- MOVE (Section 10.4) ---
    // To register: 1 + eaCost(src)
    // From register: 1 + eaCost(dst)
    // Other (imm→mem, mem→mem): 2 + eaCost(src) + eaCost(dst)
    int timeMove(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 1;

        bool srcReg = isRegDirect(inst.src->mode);
        bool dstReg = isRegDirect(inst.dst->mode);

        if (dstReg) return 1 + eaCost(inst.src->mode);
        if (srcReg) return 1 + eaCost(inst.dst->mode);

        // imm→mem or mem→mem: base 2
        return 2 + eaCost(inst.src->mode) + eaCost(inst.dst->mode);
    }

    // --- MOVEM (Section 10.5) ---
    // Approximate: 2 + register count
    int timeMovem(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 2;
        return 2 + movemRegCount(inst);
    }

    // --- Standard ALU: ADD/AND/CMP/EOR/OR/SUB/TST (Section 10.6) ---
    // All standard modes: 1 cycle. Index/PC add eaCost.
    int timeAlu(const Instruction& inst) {
        // For <ea>,Dn: use src mode. For Dn,<ea>: use dst mode.
        if (inst.dst && isMem(*inst.dst)) {
            return 1 + eaCost(inst.dst->mode);
        }
        if (inst.src) {
            return 1 + eaCost(inst.src->mode);
        }
        return 1;
    }

    // --- Immediate ALU: ADDI/ANDI/CMPI/EORI/ORI/SUBI (Section 10.6) ---
    // Dn: 1, (An): 1, other mem: 2 + eaCost
    int timeImmediate(const Instruction& inst) {
        auto* dst = getOp(inst);
        if (!dst || !isMem(*dst)) return 1;
        if (dst->mode == AddrMode::ind) return 1;  // (An): 1
        return 2 + eaCost(dst->mode);
    }

    // --- Quick: ADDQ/SUBQ (Section 10.6) ---
    // Dn/An/(An)/abs: 1, (An)+/-(An)/d(An): 2 + eaCost
    int timeQuick(const Instruction& inst) {
        auto* dst = getOp(inst);
        if (!dst || !isMem(*dst)) return 1;
        if (hasAddrUpdate(dst->mode)) return 2 + eaCost(dst->mode);
        return 1 + eaCost(dst->mode);
    }

    // --- Shifts and rotates (Section 10.6) ---
    // Memory shifts are single-operand (no inst.dst); register shifts have
    // both src (count) and dst (register). ROXL/ROXR differ: reg=5, mem=2.
    int timeShift(const Instruction& inst) {
        auto* op = getOp(inst);
        bool memShift = op && isMem(*op) && (!inst.dst || !inst.src);
        int ea = memShift ? eaCost(op->mode) : 0;

        switch (inst.mnemonic) {
        case Mnemonic::asl: case Mnemonic::rol: case Mnemonic::ror:
            return 3 + ea;
        case Mnemonic::roxl: case Mnemonic::roxr:
            return (memShift ? 2 : 5) + ea;
        default:  // ASR, LSL, LSR
            return 2 + ea;
        }
    }

    // --- Bit manipulation: BTST/BSET/BCLR/BCHG (Section 10.6) ---
    // baseCost is for #imm variant; Dn variant adds +1.
    int timeBitOp(const Instruction& inst, int baseCost) {
        bool isDynamic = inst.src && inst.src->mode == AddrMode::dn;
        int cost = baseCost + (isDynamic ? 1 : 0);
        if (inst.dst) {
            cost += eaCost(inst.dst->mode);
        }
        return cost;
    }

    // --- Bit field (Section 10.6) ---
    int timeBitField(const Instruction& inst, int regCost, int memCost) {
        const EffectiveAddr* op = getOp(inst);
        if (inst.mnemonic == Mnemonic::bfins && inst.dst) {
            op = &*inst.dst;
        }
        if (op && isMem(*op)) return memCost + eaCost(op->mode);
        return regCost;
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming040() {
    return std::make_unique<Timing040>();
}
