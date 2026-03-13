#include "timing.h"
#include "m68k.h"
#include <algorithm>

namespace {

// MC68060 timing from MC68060 User's Manual Section 10.
// All timings assume cache hits (Section 10.2 assumption 4).
// EA calculation time = 0 for all standard addressing modes (Table 10-5).
// Instruction costs are the direct C(r/w) values from the tables.

// Superscalar classification (Table 10-2).
// pOEP|sOEP instructions can execute in either OEP and are eligible for
// dual-issue pairing. The classification depends on the instruction form.
bool isPairable(const Instruction& inst) {
    switch (inst.mnemonic) {
    // Standard ALU: pOEP | sOEP
    case Mnemonic::add: case Mnemonic::adda:
    case Mnemonic::sub: case Mnemonic::suba:
    case Mnemonic::and_: case Mnemonic::or_:
    case Mnemonic::eor:
    case Mnemonic::cmp: case Mnemonic::cmpa:
    case Mnemonic::addq: case Mnemonic::subq:
        return true;

    // ADDI/SUBI/ANDI/ORI/EORI/CMPI: pOEP|sOEP for Dx and (Ax)+/-(Ax) only;
    // remaining destinations are pOEP-until-last (not pairable in simple model).
    case Mnemonic::addi: case Mnemonic::subi:
    case Mnemonic::andi: case Mnemonic::ori:
    case Mnemonic::eori: case Mnemonic::cmpi: {
        auto* dst = getOp(inst);
        if (!dst) return true;
        return dst->mode == AddrMode::dn || dst->mode == AddrMode::an ||
               dst->mode == AddrMode::postInc || dst->mode == AddrMode::preDec;
    }

    // MOVE: pOEP|sOEP for MOVE to Rx or MOVE from Ry (reg-involved);
    // mem→mem and #imm→mem are pOEP-until-last.
    case Mnemonic::move: {
        if (!inst.src || !inst.dst) return true;
        return isRegDirect(inst.src->mode) || isRegDirect(inst.dst->mode);
    }
    case Mnemonic::movea: case Mnemonic::moveq:
        return true;

    // Single operand: pOEP | sOEP
    case Mnemonic::clr: case Mnemonic::neg: case Mnemonic::not_:
    case Mnemonic::tst: case Mnemonic::ext:
        return true;

    // Shifts (not ROX): pOEP | sOEP
    case Mnemonic::asl: case Mnemonic::asr:
    case Mnemonic::lsl: case Mnemonic::lsr:
    case Mnemonic::rol: case Mnemonic::ror:
        return true;

    // LEA, MOVE to CCR: pOEP | sOEP
    case Mnemonic::lea:
    case Mnemonic::moveToCCR:
        return true;

    // TRAP, ILLEGAL: pOEP | sOEP (Table 10-2)
    case Mnemonic::trap: case Mnemonic::trapv: case Mnemonic::illegal:
        return true;

    default:
        return false;
    }
}

// MC68060 timing engine.
// Flat timing model: most instructions are 1 cycle. Superscalar dual-issue in time(span).
class Timing060 : public TimingBase {
public:
    // Skip bus rounding — 060 values are exact clock counts.
    Timing time(const Instruction& inst) override {
        return computeTime(inst);
    }

    Timing time(std::span<const Instruction> block) override {
        Timing total;
        size_t i = 0;
        while (i < block.size()) {
            auto t = time(block[i]);

            // Try to pair with next instruction (Table 10-1)
            if (i + 1 < block.size() &&
                isPairable(block[i]) &&
                isPairable(block[i + 1])) {
                auto prev = regUsage(block[i]);
                auto next = regUsage(block[i + 1]);

                // Test 4: only one memory operand across the pair
                bool prevMem = (block[i].src && isMem(*block[i].src)) ||
                               (block[i].dst && isMem(*block[i].dst));
                bool nextMem = (block[i+1].src && isMem(*block[i+1].src)) ||
                               (block[i+1].dst && isMem(*block[i+1].dst));

                // Tests 5/6: no register conflicts
                bool noConflict = !(prev.write & next.read) &&
                                  !(prev.write & next.write);

                if (noConflict && !(prevMem && nextMem)) {
                    auto t2 = time(block[i + 1]);
                    total += {std::max(t.a, t2.a), std::max(t.b, t2.b)};
                    i += 2;
                    continue;
                }
            }

            total += t;
            i++;
        }
        return total;
    }

    int busAccessCycles() const override { return 1; }
    int busWidth() const override { return 4; }
    const char* name() const override { return "MC68060"; }

    std::optional<CacheConfig> iCache() const override {
        return CacheConfig{8192, 16, 4};
    }
    std::optional<CacheConfig> dCache() const override {
        return CacheConfig{8192, 16, 4};
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
            // MOVEP is emulated on 060 — use rough estimate
            t.a = t.b = isLong(inst.size) ? 6 : 4;
            break;

        case Mnemonic::lea:
            // LEA: 1(0/0) for all our modes (Table 10-20)
            t.a = t.b = 1;
            break;

        case Mnemonic::pea:
            // PEA: 1(0/1) for (An)/abs/d(PC), 2(0/1) for d(An)/d(An,Xn)/d(PC,Xn)
            // Table 10-20: +1 for disp, index, pcIndex only (NOT abs/pcDisp)
            if (inst.src) {
                AddrMode m = inst.src->mode;
                bool extra = (m == AddrMode::disp || m == AddrMode::index ||
                              m == AddrMode::pcIndex);
                t.a = t.b = extra ? 2 : 1;
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::exg:
        case Mnemonic::swap:
        case Mnemonic::ext:
            t.a = t.b = 1;
            break;

        case Mnemonic::link:
            // LINK: 2(0/1) (Table 10-24)
            t.a = t.b = 2;
            break;

        case Mnemonic::unlk:
            // UNLK: 1(1/0) (Table 10-24)
            t.a = t.b = 1;
            break;

        // --- Standard ALU (Table 10-9) ---
        case Mnemonic::add: case Mnemonic::sub:
        case Mnemonic::and_: case Mnemonic::or_:
        case Mnemonic::eor: case Mnemonic::cmp:
        case Mnemonic::adda: case Mnemonic::suba: case Mnemonic::cmpa:
            // All forms: 1 cycle (EA,Rn = 1(1/0), Rn,M = 1(1/1))
            t.a = t.b = 1;
            break;

        // --- Immediate ALU (Table 10-10) ---
        case Mnemonic::addi: case Mnemonic::subi:
        case Mnemonic::andi: case Mnemonic::ori:
        case Mnemonic::eori: case Mnemonic::cmpi:
            t.a = t.b = timeImmAlu(inst);
            break;

        // --- Quick (Table 10-10) ---
        case Mnemonic::addq: case Mnemonic::subq:
            // ADDQ/SUBQ: 1 cycle for all our modes
            t.a = t.b = 1;
            break;

        // --- Multiprecision (Table 10-21) ---
        case Mnemonic::addx: case Mnemonic::subx:
            if (inst.src && inst.src->mode == AddrMode::preDec)
                t.a = t.b = 2;  // 2(2/1)
            else
                t.a = t.b = 1;  // 1(0/0)
            break;

        case Mnemonic::abcd: case Mnemonic::sbcd:
            if (inst.src && inst.src->mode == AddrMode::preDec)
                t.a = t.b = 2;  // 2(2/1)
            else
                t.a = t.b = 1;  // 1(0/0)
            break;

        case Mnemonic::cmpm:
            t.a = t.b = 2;  // 2(2/0)
            break;

        // --- Multiply/Divide (Table 10-9) ---
        case Mnemonic::mulu: case Mnemonic::muls:
            // MULS/MULU: 2(1/0) for both .W and .L
            t.a = t.b = 2;
            break;

        case Mnemonic::divu: case Mnemonic::divs:
            // DIVS/DIVU.W: <=22(1/0), .L: 38(1/0)
            if (isLong(inst.size))
                t.a = t.b = 38;
            else
                t.a = t.b = 22;
            break;

        // --- Single operand (Tables 10-11, 10-12) ---
        case Mnemonic::neg: case Mnemonic::negx:
        case Mnemonic::not_:
            // NEG/NEGX/NOT: reg 1(0/0), mem 1(1/1)
            t.a = t.b = 1;
            break;

        case Mnemonic::clr:
            // CLR: reg 1(0/0), mem 1(0/1) — all our modes are 1
            t.a = t.b = 1;
            break;

        case Mnemonic::tst:
            // TST: reg 1(0/0), mem 1(1/0)
            t.a = t.b = 1;
            break;

        case Mnemonic::nbcd:
            // NBCD: reg 1(0/0), mem 1(1/1)
            t.a = t.b = 1;
            break;

        case Mnemonic::tas: {
            // TAS: reg 1(0/0), mem 17(1/1) — bus lock (Table 10-11)
            auto op = getOp(inst);
            t.a = t.b = (op && isMem(*op)) ? 17 : 1;
            break;
        }

        case Mnemonic::chk:
            // CHK: reg 2(0/0), mem 2(1/0) (Table 10-24)
            t.a = t.b = 2;
            break;

        // --- Scc (Table 10-11) ---
        case Mnemonic::st: case Mnemonic::sf:
        case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
        case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
        case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
        case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
        case Mnemonic::sgt: case Mnemonic::sle:
            // Scc: reg 1(0/0), mem 1(1/1)
            t.a = t.b = 1;
            break;

        // --- Shift/Rotate (Table 10-13) ---
        case Mnemonic::asl: case Mnemonic::asr:
        case Mnemonic::lsl: case Mnemonic::lsr:
        case Mnemonic::rol: case Mnemonic::ror:
        case Mnemonic::roxl: case Mnemonic::roxr:
            // All shifts: reg 1(0/0), mem 1(1/1)
            t.a = t.b = 1;
            break;

        // --- Bit manipulation (Tables 10-14, 10-15) ---
        case Mnemonic::btst:
            t.a = t.b = timeBitManip(inst, false);
            break;

        case Mnemonic::bset: case Mnemonic::bclr: case Mnemonic::bchg:
            t.a = t.b = timeBitManip(inst, true);
            break;

        // --- Bit field (Table 10-16) ---
        case Mnemonic::bftst:
            timeBitField(inst, t, 6, 8);
            break;
        case Mnemonic::bfextu: case Mnemonic::bfexts:
            timeBitField(inst, t, 6, 8);
            break;
        case Mnemonic::bfffo:
            timeBitField(inst, t, 9, 11);
            break;
        case Mnemonic::bfchg: case Mnemonic::bfclr: case Mnemonic::bfset:
            timeBitField(inst, t, 8, 12);
            break;
        case Mnemonic::bfins:
            timeBitField(inst, t, 6, 6);  // BFINS: 6(0/0) reg, 6(1/1)/6(2/2) mem
            break;

        // --- Branches (Table 10-17) ---
        // Using not-predicted backward model: taken=3, not-taken=7.
        case Mnemonic::bra:
            t.a = t.b = 3;
            break;

        case Mnemonic::bsr:
            t.a = t.b = 3;  // 3(0/1)
            break;

        case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
        case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
        case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
        case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
        case Mnemonic::bgt: case Mnemonic::ble:
            // Bcc backward: taken=3, not-taken=7
            t.a = 3;
            t.b = 7;
            break;

        // --- DBcc (Table 10-17) ---
        case Mnemonic::dbt:
            t.a = t.b = 8;  // cc=True: 8(0/0)
            break;

        case Mnemonic::dbf:
            // DBRA: loop=3, fall=7
            t.a = 3;
            t.b = 7;
            break;

        case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
        case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
        case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
        case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
        case Mnemonic::dbgt: case Mnemonic::dble:
            // DBcc: loop=3, fall=8
            t.a = 3;
            t.b = 8;
            break;

        // --- JMP/JSR (Table 10-18) ---
        case Mnemonic::jmp:
            // JMP d(PC)/abs: 3, other: 5
            if (inst.src && (inst.src->mode == AddrMode::pcDisp ||
                             inst.src->mode == AddrMode::absW ||
                             inst.src->mode == AddrMode::absL))
                t.a = t.b = 3;
            else
                t.a = t.b = 5;
            break;

        case Mnemonic::jsr:
            // JSR d(PC)/abs: 3(0/1), other: 5(0/1)
            if (inst.src && (inst.src->mode == AddrMode::pcDisp ||
                             inst.src->mode == AddrMode::absW ||
                             inst.src->mode == AddrMode::absL))
                t.a = t.b = 3;
            else
                t.a = t.b = 5;
            break;

        // --- Returns (Table 10-19) ---
        case Mnemonic::rts:
            t.a = t.b = 7;   // 7(1/0)
            break;

        case Mnemonic::rte:
            t.a = t.b = 17;  // 17(3/0)
            break;

        case Mnemonic::rtr:
            t.a = t.b = 8;   // 8(2/0)
            break;

        // --- Special moves (Table 10-22) ---
        case Mnemonic::moveToSR:
            t.a = t.b = 12;  // 12(1/0) + EA calc
            break;

        case Mnemonic::moveFromSR:
            t.a = t.b = 1;   // 1(0/1) + EA calc
            break;

        case Mnemonic::moveToCCR:
            t.a = t.b = 1;   // 1(0/0) reg, 1(1/0) mem
            break;

        case Mnemonic::moveUSP:
            // MOVE from USP: 1(0/0), MOVE to USP: 2(0/0)
            if (inst.src && inst.src->mode == AddrMode::an)
                t.a = t.b = 2;  // An → USP
            else
                t.a = t.b = 1;  // USP → An
            break;

        // --- Misc (Table 10-24) ---
        case Mnemonic::nop:
            t.a = t.b = 9;   // 9(0/0) — pipeline sync
            break;

        case Mnemonic::reset:
            t.a = t.b = 520;
            break;

        case Mnemonic::stop:
            t.a = t.b = 8;   // 8(0/0)
            break;

        case Mnemonic::trap:
            t.a = t.b = 1;   // 1(0/0) — TRAP instruction itself
            break;

        case Mnemonic::trapv:
            t.a = t.b = 1;   // 1(0/0)
            break;

        case Mnemonic::illegal:
            t.a = t.b = 1;
            break;

        default:
            t.a = t.b = 1;
            break;
        }

        return t;
    }

private:
    // --- MOVE (Tables 10-6, 10-7) ---
    int timeMove(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 1;

        bool srcReg = isRegDirect(inst.src->mode);
        bool dstReg = isRegDirect(inst.dst->mode);

        if (dstReg) return 1;       // Any → Rn: 1
        if (srcReg) return 1;       // Rn → Mem: 1 for all our modes

        // #imm → Mem: 1 for simple, +1 for ext-word destination
        if (inst.src->mode == AddrMode::imm)
            return hasExtWord(inst.dst->mode) ? 2 : 1;

        // Mem → Mem: 2
        return 2;
    }

    // --- MOVEM (Table 10-20) ---
    int timeMovem(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 1;
        int n = movemRegCount(inst);
        if (n == 0) return 1;

        bool toMem = (inst.src->mode == AddrMode::regList);
        const auto& ea = toMem ? *inst.dst : *inst.src;

        // Base: n cycles. +1 for index/abs/pcIndex only (Table 10-20).
        // d(An) and d(PC) do NOT add +1 for MOVEM.
        AddrMode m = ea.mode;
        bool extra = (m == AddrMode::index || m == AddrMode::absW ||
                      m == AddrMode::absL || m == AddrMode::pcIndex);
        return n + (extra ? 1 : 0);
    }

    // --- Immediate ALU (Table 10-10) ---
    // ADDI/SUBI/ANDI/ORI/EORI: Dn = 1, mem = 1 simple, 2 ext-word
    // CMPI: Dn = 1, mem = 1 simple, 2 ext-word
    int timeImmAlu(const Instruction& inst) {
        auto* dst = getOp(inst);
        if (!dst || !isMem(*dst)) return 1;
        return hasExtWord(dst->mode) ? 2 : 1;
    }

    // --- Bit manipulation (Tables 10-14, 10-15) ---
    // Dynamic (Dn bit): reg=1, mem=1. Static (#imm): reg=1, mem=1 simple, 2 ext-word.
    int timeBitManip(const Instruction& inst, bool /*isRMW*/) {
        bool isImm = inst.src && inst.src->mode == AddrMode::imm;
        if (!inst.dst) return 1;
        auto* target = &*inst.dst;

        if (!isMem(*target)) return 1;

        // Dynamic Dn,<ea>: 1 cycle for all mem modes
        if (!isImm) return 1;

        // Static #imm,<ea>: +1 for ext-word modes
        return hasExtWord(target->mode) ? 2 : 1;
    }

    // --- Bit field (Table 10-16) ---
    void timeBitField(const Instruction& inst, Timing& t,
                      int regSmall, int regLarge) {
        const EffectiveAddr* op = getOp(inst);
        if (inst.mnemonic == Mnemonic::bfins)
            op = inst.dst ? &*inst.dst : nullptr;

        if (op && isMem(*op)) {
            // Memory: same base as register + EA overhead
            // +1 for index/abs/pcIndex only (Table 10-16); NOT d(An)/d(PC)
            AddrMode m = op->mode;
            int extra = (m == AddrMode::index || m == AddrMode::absW ||
                         m == AddrMode::absL || m == AddrMode::pcIndex) ? 1 : 0;
            t.a = regSmall + extra;
            t.b = regLarge + extra;
        } else {
            t.a = regSmall;
            t.b = regLarge;
        }
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming060() {
    return std::make_unique<Timing060>();
}
