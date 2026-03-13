#include "timing.h"
#include "m68k.h"
#include "error.h"

namespace {

// MC68020/030 timing from MC68030 User's Manual Section 11.6, NCC (No-Cache Case).
// All values derived from the Total(reads/prefetches/writes) decomposition.
// head = Total_NCC - (reads + prefetches + writes) * 2  [assuming 32-bit bus, 2-clock access]

// --- 11.6.1 Fetch Effective Address (fea) ---
// Cost to fetch a source operand from the given addressing mode.
// Indexed by eaIndex(). Index 12 = #imm.L (not an eaIndex value).
constexpr int feaImmL = 12;
constexpr BusCost feaTable[13] = {
    {0, 0, 0, 0},  //  0 Dn          0(0/0/0)
    {0, 0, 0, 0},  //  1 An          0(0/0/0)
    {1, 1, 0, 0},  //  2 (An)        3(1/0/0)
    {1, 1, 0, 0},  //  3 (An)+       3(1/0/0)
    {2, 1, 0, 0},  //  4 -(An)       4(1/0/0)
    {0, 1, 1, 0},  //  5 d(An)       4(1/1/0)
    {2, 1, 1, 0},  //  6 d(An,Xn)    6(1/1/0)  brief format
    {0, 1, 1, 0},  //  7 xxx.W       4(1/1/0)
    {1, 1, 1, 0},  //  8 xxx.L       5(1/1/0)
    {0, 1, 1, 0},  //  9 d(PC)       4(1/1/0)
    {2, 1, 1, 0},  // 10 d(PC,Xn)    6(1/1/0)  brief format
    {0, 0, 1, 0},  // 11 #imm.bw     2(0/1/0)
    {2, 0, 1, 0},  // 12 #imm.l      4(0/1/0)
};

// Lookup fea cost for an EA, considering operand size for immediates.
constexpr BusCost fea(const EffectiveAddr& ea, std::optional<OpSize> sz) {
    int idx = eaIndex(ea.mode);
    if (idx == 11 && isLong(sz)) idx = feaImmL;
    return feaTable[idx];
}

// --- 11.6.2 Fetch Immediate Effective Address (fiea) ---
// NCC values from MC68030UM Section 11.6.2 tables.
// Indexed by [sizeIdx(sz)][eaIndex(dest.mode)].
// Pipeline overlaps make these NOT simply fieaImm + feaTable.
constexpr BusCost fieaTable[2][13] = {
    // #imm.bw (word immediate)
    {
        {0, 0, 1, 0},  //  0 Dn        2(0/1/0)
        {0, 0, 1, 0},  //  1 An        2(0/1/0)
        {0, 1, 1, 0},  //  2 (An)      4(1/1/0)
        {1, 1, 1, 0},  //  3 (An)+     5(1/1/0)
        {0, 1, 1, 0},  //  4 -(An)     4(1/1/0)
        {1, 1, 1, 0},  //  5 d(An)     5(1/1/0)
        {2, 1, 2, 0},  //  6 d(An,Xn)  8(1/2/0)
        {2, 1, 1, 0},  //  7 abs.W     6(1/1/0)
        {1, 1, 2, 0},  //  8 abs.L     7(1/2/0)
        {1, 1, 1, 0},  //  9 d(PC)     5(1/1/0)
        {2, 1, 2, 0},  // 10 d(PC,Xn)  8(1/2/0)
        {0, 0, 2, 0},  // 11 #imm.bw   4(0/2/0) (not valid dest)
        {0, 0, 2, 0},  // 12 #imm.l    (placeholder)
    },
    // #imm.l (long immediate)
    {
        {2, 0, 1, 0},  //  0 Dn        4(0/1/0)
        {2, 0, 1, 0},  //  1 An        4(0/1/0)
        {1, 1, 1, 0},  //  2 (An)      5(1/1/0)
        {3, 1, 1, 0},  //  3 (An)+     7(1/1/0)
        {2, 1, 1, 0},  //  4 -(An)     6(1/1/0)
        {2, 1, 2, 0},  //  5 d(An)     8(1/2/0)
        {4, 1, 2, 0},  //  6 d(An,Xn) 10(1/2/0)
        {2, 1, 2, 0},  //  7 abs.W     8(1/2/0)
        {3, 1, 2, 0},  //  8 abs.L     9(1/2/0)
        {2, 1, 2, 0},  //  9 d(PC)     8(1/2/0)
        {4, 1, 2, 0},  // 10 d(PC,Xn) 10(1/2/0)
        {2, 0, 2, 0},  // 11 #imm.bw   6(0/2/0) (not valid dest)
        {2, 0, 2, 0},  // 12 #imm.l    (placeholder)
    },
};

constexpr BusCost fieaImm(std::optional<OpSize> sz) {
    return fieaTable[sizeIdx(sz)][0]; // Same as Dn entry
}

constexpr BusCost fiea(const EffectiveAddr& dest, std::optional<OpSize> sz) {
    int idx = eaIndex(dest.mode);
    if (idx == 11 && isLong(sz)) idx = feaImmL; // not valid dest, but safe
    return fieaTable[sizeIdx(sz)][idx];
}

// --- Calculate Effective Address (cea) ---
// Derived from LEA timing: LEA base = {0, 0, 1, 0} (2 NCC), cea(mode) = LEA(mode) - base.
// Used for CLR, Scc, TAS destinations (address-only, no data read).
constexpr BusCost ceaTable[12] = {
    {0, 0, 0, 0},  // Dn          0
    {0, 0, 0, 0},  // An          0
    {0, 0, 0, 0},  // (An)        0
    {0, 0, 0, 0},  // (An)+       0
    {0, 0, 0, 0},  // -(An)       0
    {0, 0, 1, 0},  // d(An)       2  (extension word)
    {2, 0, 1, 0},  // d(An,Xn)    4  (brief ext + index calc)
    {0, 0, 1, 0},  // xxx.W       2  (address word)
    {2, 0, 1, 0},  // xxx.L       4  (address long)
    {0, 0, 1, 0},  // d(PC)       2
    {2, 0, 1, 0},  // d(PC,Xn)    4
    {0, 0, 0, 0},  // #imm        0  (not applicable)
};

constexpr BusCost cea(const EffectiveAddr& ea) {
    return ceaTable[eaIndex(ea.mode)];
}

// --- Jump Effective Address (JEA) ---
// Derived from JSR: JSR base = {1, 0, 2, 1} (9 NCC), JEA(mode) = JSR(mode) - base.
// JSR (An) = 9 -> JEA(An) = {2, 0, 0, 0}
constexpr BusCost jeaTable[12] = {
    {0, 0, 0, 0},  // Dn          (n/a)
    {0, 0, 0, 0},  // An          (n/a)
    {2, 0, 0, 0},  // (An)        2
    {0, 0, 0, 0},  // (An)+       (n/a)
    {0, 0, 0, 0},  // -(An)       (n/a)
    {2, 0, 1, 0},  // d(An)       4  (disp ext word)
    {4, 0, 1, 0},  // d(An,Xn)    6  (brief ext + index)
    {2, 0, 1, 0},  // xxx.W       4
    {4, 0, 1, 0},  // xxx.L       6
    {2, 0, 1, 0},  // d(PC)       4
    {4, 0, 1, 0},  // d(PC,Xn)    6
    {0, 0, 0, 0},  // #imm        (n/a)
};

// --- MOVE destination base costs (11.6.6) ---
// Complete base cost (NCC decomposition) for MOVE to each destination mode.
// For (An)/(An)+/-(An), register source saves 1 head cycle vs mem/imm source.
// For other modes, only one entry (EA source, add fea for source).
constexpr BusCost moveDstBase[12] = {
    {0, 0, 1, 0},  // Dn          2(0/1/0) MOVE EA,Dn
    {0, 0, 1, 0},  // An          2(0/1/0) MOVE EA,An
    {1, 0, 1, 1},  // (An)        5(0/1/1) * MOVE SRC,(An)
    {1, 0, 1, 1},  // (An)+       5(0/1/1) * MOVE SRC,(An)+
    {1, 0, 1, 1},  // -(An)       5(0/1/1) * MOVE SRC,-(An)
    {1, 0, 1, 1},  // d(An)       5(0/1/1) * MOVE EA,(d16,An)
    {3, 0, 1, 1},  // d(An,Xn)    7(0/1/1) * MOVE EA,(d8,An,Xn) brief
    {1, 0, 1, 1},  // xxx.W       5(0/1/1) * MOVE EA,xxx.W
    {1, 0, 2, 1},  // xxx.L       7(0/2/1) * MOVE EA,xxx.L
    {1, 0, 1, 1},  // d(PC)       (not valid dst, use disp cost)
    {3, 0, 1, 1},  // d(PC,Xn)    (not valid dst, use index cost)
    {0, 0, 1, 0},  // #imm        (not valid dst, fallback)
};


// MC68020 timing: decomposed bus model from MC68030 User's Manual.
class Timing020 : public TimingBase {
public:
    const char* name() const override { return "MC68020"; }
    int busAccessCycles() const override { return 2; }
    int busWidth() const override { return 4; }

    std::optional<CacheConfig> iCache() const override {
        return CacheConfig{256, 4, 1};
    }

    // 020/030 have non-even cycle counts; skip bus rounding.
    Timing time(const Instruction& inst) override {
        return computeTime(inst);
    }

protected:
    Timing computeTime(const Instruction& inst) override {
        Timing t;

        switch (inst.mnemonic) {
        // --- Data movement ---
        case Mnemonic::move:
        case Mnemonic::movea:
            timeMove(inst, t);
            break;

        case Mnemonic::moveq:
            // MOVEQ: 2(0/1/0)
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::movem:
            t.a = t.b = timeMovem(inst);
            break;

        case Mnemonic::movep:
            // MOVEP.W: 10(0/1/2) or 10(2/1/0), MOVEP.L: 14(0/1/4) or 14(4/1/0)
            if (inst.src && inst.src->mode == AddrMode::dn) {
                // Dn -> mem: writes are always byte-sized (no long scaling)
                if (isLong(inst.size))
                    t.a = t.b = BusCost{4, 0, 1, 4}.toCycles();  // 14(0/1/4)
                else
                    t.a = t.b = BusCost{4, 0, 1, 2}.toCycles();  // 10(0/1/2)
            } else {
                // mem -> Dn: reads are always byte-sized
                if (isLong(inst.size))
                    t.a = t.b = BusCost{4, 4, 1, 0}.toCycles();  // 14(4/1/0)
                else
                    t.a = t.b = BusCost{4, 2, 1, 0}.toCycles();  // 10(2/1/0)
            }
            break;

        case Mnemonic::lea:
            timeLea(inst, t);
            break;

        case Mnemonic::pea:
            timePea(inst, t);
            break;

        case Mnemonic::exg:
            // EXG: 4(0/1/0)
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::swap:
            // SWAP: 4(0/1/0)
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::link:
            // LINK.W: 5(0/1/1), LINK.L: 7(0/2/1)
            if (isLong(inst.size))
                t.a = t.b = BusCost{1, 0, 2, 1}.toCycles(true);  // push is long
            else
                t.a = t.b = BusCost{1, 0, 1, 1}.toCycles(true);  // push is long
            break;

        case Mnemonic::unlk:
            // UNLK: 5(1/1/0)
            t.a = t.b = BusCost{1, 1, 1, 0}.toCycles(true);  // read is long (An)
            break;

        // --- Arithmetic (11.6.8) ---
        case Mnemonic::add: case Mnemonic::sub:
        case Mnemonic::and_: case Mnemonic::or_:
        case Mnemonic::cmp:
            if (inst.src && inst.src->mode == AddrMode::imm) {
                timeImmAlu(inst, t);
            } else {
                timeAlu(inst, t);
            }
            break;

        case Mnemonic::adda: case Mnemonic::suba:
            timeAdda(inst, t);
            break;

        case Mnemonic::cmpa:
            timeCmpa(inst, t);
            break;

        case Mnemonic::eor:
            if (inst.src && inst.src->mode == AddrMode::imm) {
                timeImmAlu(inst, t);
            } else {
                timeEor(inst, t);
            }
            break;

        case Mnemonic::addi: case Mnemonic::subi:
        case Mnemonic::cmpi: case Mnemonic::andi:
        case Mnemonic::ori: case Mnemonic::eori:
            timeImmAlu(inst, t);
            break;

        case Mnemonic::addq: case Mnemonic::subq:
            timeQuick(inst, t);
            break;

        case Mnemonic::addx: case Mnemonic::subx:
            // ADDX/SUBX Dn,Dn: 2(0/1/0), -(An),-(An): 10(2/1/1)
            if (inst.src && inst.src->mode == AddrMode::preDec)
                t.a = t.b = BusCost{2, 2, 1, 1}.toCycles(isLong(inst.size));
            else
                t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            break;

        // --- Multiply/Divide (11.6.8) ---
        case Mnemonic::mulu:
            timeMul(inst, t, false);
            break;

        case Mnemonic::muls:
            timeMul(inst, t, true);
            break;

        case Mnemonic::divu:
            timeDiv(inst, t, false);
            break;

        case Mnemonic::divs:
            timeDiv(inst, t, true);
            break;

        // --- Single operand (11.6.11) ---
        case Mnemonic::neg: case Mnemonic::negx:
        case Mnemonic::not_:
            timeUnary(inst, t, false);
            break;

        case Mnemonic::clr:
            timeUnary(inst, t, true);
            break;

        case Mnemonic::ext:
            // EXT: 4(0/1/0)
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::tst: {
            // TST Dn: 2(0/1/0), TST Mem: 2(0/1/0) + fea
            auto op = getOp(inst);
            BusCost base = {0, 0, 1, 0};  // 2(0/1/0)
            if (op && isMem(*op))
                t.a = t.b = (base + fea(*op, inst.size)).toCycles(isLong(inst.size));
            else
                t.a = t.b = base.toCycles();
            break;
        }

        case Mnemonic::chk:
            // CHK Dn,Dn (no exc): 8(0/1/0) + fea
            if (inst.src && isMem(*inst.src)) {
                BusCost base = {6, 0, 1, 0};
                t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(isLong(inst.size));
            } else {
                t.a = t.b = BusCost{6, 0, 1, 0}.toCycles();
            }
            break;

        // --- Shift/Rotate (11.6.12) ---
        case Mnemonic::asl: case Mnemonic::asr:
        case Mnemonic::lsl: case Mnemonic::lsr:
        case Mnemonic::rol: case Mnemonic::ror:
        case Mnemonic::roxl: case Mnemonic::roxr:
            timeShift(inst, t);
            break;

        // --- Bit manipulation (11.6.13) ---
        case Mnemonic::btst:
            timeBtst(inst, t);
            break;

        case Mnemonic::bset: case Mnemonic::bclr: case Mnemonic::bchg:
            timeBitOp(inst, t);
            break;

        // --- Bit field (11.6.14) ---
        case Mnemonic::bftst:
            timeBitField(getOp(inst), t, {6, 0, 1, 0}, {6, 1, 1, 0}, {8, 2, 1, 0});
            break;
        case Mnemonic::bfextu: case Mnemonic::bfexts:
            timeBitField(getOp(inst), t, {8, 0, 1, 0}, {8, 1, 1, 0}, {12, 2, 1, 0});
            break;
        case Mnemonic::bfffo:
            timeBitField(getOp(inst), t, {18, 0, 1, 0}, {18, 1, 1, 0}, {22, 2, 1, 0});
            break;
        case Mnemonic::bfchg: case Mnemonic::bfclr: case Mnemonic::bfset:
            timeBitField(getOp(inst), t, {12, 0, 1, 0}, {8, 1, 1, 1}, {12, 2, 1, 2});
            break;
        case Mnemonic::bfins:
            timeBitField(inst.dst ? &*inst.dst : nullptr, t, {10, 0, 1, 0}, {6, 1, 1, 1}, {8, 2, 1, 2});
            break;

        // --- Conditional branches (11.6.15) ---
        case Mnemonic::bra:
            // BRA: same as Bcc taken: 8(0/2/0)
            t.a = t.b = BusCost{4, 0, 2, 0}.toCycles();
            break;

        case Mnemonic::bsr:
            // BSR: 9(0/2/1) — write is long (push PC)
            t.a = t.b = BusCost{3, 0, 2, 1}.toCycles(true);
            break;

        case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
        case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
        case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
        case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
        case Mnemonic::bgt: case Mnemonic::ble:
            timeBcc(inst, t);
            break;

        // --- DBcc (11.6.15) ---
        case Mnemonic::dbt:
            // DBcc (cc=True): 8(0/1/0)
            t.a = t.b = BusCost{6, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::dbf:
        case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
        case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
        case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
        case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
        case Mnemonic::dbgt: case Mnemonic::dble:
            // cc=False, not expired (loop): 8(0/2/0)
            // cc=False, expired (fall): 13(0/3/0)  [was 10 in old code]
            t.a = BusCost{4, 0, 2, 0}.toCycles();
            t.b = BusCost{7, 0, 3, 0}.toCycles();
            break;

        // --- Scc (11.6.11) ---
        case Mnemonic::st: case Mnemonic::sf:
        case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
        case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
        case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
        case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
        case Mnemonic::sgt: case Mnemonic::sle:
            timeScc(inst, t);
            break;

        // --- Jump (11.6.16) ---
        case Mnemonic::jmp:
            // JMP: 6(0/2/0) + JEA
            if (inst.src) {
                BusCost base = {2, 0, 2, 0};
                t.a = t.b = (base + jeaTable[eaIndex(inst.src->mode)]).toCycles();
            } else {
                t.a = t.b = BusCost{2, 0, 2, 0}.toCycles();
            }
            break;

        case Mnemonic::jsr:
            // JSR: 7(0/2/1) + JEA — write is long (push PC)
            if (inst.src) {
                BusCost base = {1, 0, 2, 1};
                t.a = t.b = (base + jeaTable[eaIndex(inst.src->mode)]).toCycles(true);
            } else {
                t.a = t.b = BusCost{1, 0, 2, 1}.toCycles(true);
            }
            break;

        // --- System (11.6.16) ---
        case Mnemonic::rts:
            // RTS: 11(1/2/0) — read is long (pop PC)
            t.a = t.b = BusCost{5, 1, 2, 0}.toCycles(true);
            break;

        case Mnemonic::rte:
            // RTE: ~20(4/2/0) — reads are long (pop SR+PC+format)
            t.a = t.b = BusCost{8, 4, 2, 0}.toCycles(true);
            break;

        case Mnemonic::rtr:
            // RTR: 14(2/2/0) — reads are long (pop CCR+PC)
            t.a = t.b = BusCost{6, 2, 2, 0}.toCycles(true);
            break;

        case Mnemonic::nop:
            // NOP: 2(0/1/0)
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::reset:
            t.a = t.b = 132;
            break;

        case Mnemonic::stop:
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            break;

        case Mnemonic::trap:
            // TRAP: ~20(0/2/4) — writes are long (push PC+SR)
            t.a = t.b = BusCost{6, 0, 2, 4}.toCycles(true);
            break;

        case Mnemonic::trapv:
            t.a = 4;
            t.b = 20;
            break;

        case Mnemonic::illegal:
            t.a = t.b = 20;
            break;

        case Mnemonic::tas: {
            // TAS Dn: 4(0/1/0), TAS Mem: 12(1/1/1) + cea
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                BusCost base = {6, 1, 1, 1};
                t.a = t.b = (base + cea(*op)).toCycles();  // byte-sized r/w
            } else {
                t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            }
            break;
        }

        // --- Special moves (11.6.7) ---
        case Mnemonic::moveToSR:
            // MOVE EA,SR: 10(0/2/0) + fea
            if (inst.src && isMem(*inst.src)) {
                BusCost base = {6, 0, 2, 0};
                t.a = t.b = (base + fea(*inst.src, OpSize::word)).toCycles();
            } else {
                t.a = t.b = BusCost{6, 0, 2, 0}.toCycles();
            }
            break;

        case Mnemonic::moveFromSR: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                // MOVE SR,Mem: 5(0/1/1) + cea
                BusCost base = {1, 0, 1, 1};
                t.a = t.b = (base + cea(*op)).toCycles();
            } else {
                // MOVE SR,Dn: 4(0/1/0)
                t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            }
            break;
        }

        case Mnemonic::moveToCCR:
            // MOVE EA,CCR: 4(0/1/0) + fea
            if (inst.src && isMem(*inst.src)) {
                BusCost base = {2, 0, 1, 0};
                t.a = t.b = (base + fea(*inst.src, OpSize::word)).toCycles();
            } else {
                t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            }
            break;

        case Mnemonic::moveUSP:
            // MOVE USP: 4(0/1/0)
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            break;

        // --- BCD (11.6.10) ---
        case Mnemonic::abcd: case Mnemonic::sbcd:
            if (inst.src && inst.src->mode == AddrMode::preDec)
                t.a = t.b = BusCost{6, 2, 1, 1}.toCycles();  // 14(2/1/1) byte r/w
            else
                t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();  // 4(0/1/0)
            break;

        case Mnemonic::nbcd: {
            // NBCD Dn: 6(0/1/0), NBCD Mem: + fea
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                BusCost base = {4, 0, 1, 0};
                t.a = t.b = (base + fea(*op, OpSize::byte)).toCycles();
            } else {
                t.a = t.b = BusCost{4, 0, 1, 0}.toCycles();
            }
            break;
        }

        case Mnemonic::cmpm:
            // CMPM: 8(2/1/0)
            t.a = t.b = BusCost{2, 2, 1, 0}.toCycles(isLong(inst.size));
            break;

        // --- Control (11.6.16) ---
        default:
            // Fallback for any unhandled instruction
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            break;
        }

        if (t.a < 0 || t.b < 0) {
            throw Error{2, "internal: negative timing for '" + inst.toString() +
                "': min=" + std::to_string(t.a) + " max=" + std::to_string(t.b)};
        }

        return t;
    }

private:
    // --- MOVE (11.6.6) ---
    void timeMove(const Instruction& inst, Timing& t) {
        if (!inst.src || !inst.dst) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        bool dstIsReg = (inst.dst->mode == AddrMode::dn || inst.dst->mode == AddrMode::an);
        bool srcIsReg = isRegDirect(inst.src->mode);
        bool longOp = isLong(inst.size);

        if (dstIsReg) {
            // MOVE EA,Rn: 2(0/1/0) + fea(src)
            BusCost base = {0, 0, 1, 0};
            t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
        } else {
            // Memory destination: use per-mode base from moveDstBase.
            BusCost base = moveDstBase[eaIndex(inst.dst->mode)];

            // For (An)/(An)+/-(An) with register source, 1 less head cycle.
            bool simpleMemDst = (inst.dst->mode == AddrMode::ind ||
                                 inst.dst->mode == AddrMode::postInc ||
                                 inst.dst->mode == AddrMode::preDec);
            if (srcIsReg && simpleMemDst) {
                // MOVE Rn,(An): 4(0/1/1) — 1 less head than the 5(0/1/1) base
                base.head -= 1;
                t.a = t.b = base.toCycles(longOp);
            } else {
                // Add fea for source EA
                t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
            }
        }
    }

    // --- MOVEM (11.6.7) ---
    int timeMovem(const Instruction& inst) {
        if (!inst.src || !inst.dst) return BusCost{6, 0, 1, 0}.toCycles();
        int n = movemRegCount(inst);
        if (n == 0) return BusCost{6, 0, 1, 0}.toCycles();

        bool toMem = (inst.src->mode == AddrMode::regList);
        bool longOp = isLong(inst.size);

        if (toMem) {
            // MOVEM RL,EA: 4+2n(0/1/n) -> head=2, pf=1, writes=n
            BusCost bc = {2, 0, 1, n};
            return bc.toCycles(longOp);
        } else {
            // MOVEM EA,RL: 8+4n(n/1/0) -> head=6+2n, reads=n, pf=1
            BusCost bc = {6 + 2 * n, n, 1, 0};
            return bc.toCycles(longOp);
        }
    }

    // --- LEA (11.6.16) ---
    void timeLea(const Instruction& inst, Timing& t) {
        // LEA: 2(0/1/0) + cea
        BusCost base = {0, 0, 1, 0};
        if (inst.src)
            t.a = t.b = (base + cea(*inst.src)).toCycles();
        else
            t.a = t.b = base.toCycles();
    }

    // --- PEA (11.6.16) ---
    void timePea(const Instruction& inst, Timing& t) {
        // PEA: 4(0/1/1) + cea — write is long (push address)
        BusCost base = {0, 0, 1, 1};
        if (inst.src)
            t.a = t.b = (base + cea(*inst.src)).toCycles(true);
        else
            t.a = t.b = base.toCycles(true);
    }

    // --- ALU: ADD/SUB/AND/OR/CMP (11.6.8) ---
    void timeAlu(const Instruction& inst, Timing& t) {
        if (!inst.src || !inst.dst) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        bool longOp = isLong(inst.size);
        if (inst.mnemonic == Mnemonic::cmp) {
            // CMP EA,Dn: 2(0/1/0) + fea
            BusCost base = {0, 0, 1, 0};
            t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
        } else if (isMem(*inst.dst)) {
            // ADD Dn,EA: 4(0/1/1) + fea(dst) — read-modify-write
            BusCost base = {0, 0, 1, 1};
            t.a = t.b = (base + fea(*inst.dst, inst.size)).toCycles(longOp);
        } else {
            // ADD EA,Dn: 2(0/1/0) + fea
            BusCost base = {0, 0, 1, 0};
            t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
        }
    }

    // --- ADDA/SUBA (11.6.8) ---
    void timeAdda(const Instruction& inst, Timing& t) {
        if (!inst.src) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        bool longOp = isLong(inst.size);
        // ADDA.W: 4(0/1/0), ADDA.L: 2(0/1/0) + fea
        if (!longOp) {
            BusCost base = {2, 0, 1, 0};  // 4(0/1/0) for .W
            t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
        } else {
            BusCost base = {0, 0, 1, 0};  // 2(0/1/0) for .L
            t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(longOp);
        }
    }

    // --- CMPA (11.6.8) ---
    void timeCmpa(const Instruction& inst, Timing& t) {
        if (!inst.src) {
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
            return;
        }
        // CMPA: 4(0/1/0) + fea
        BusCost base = {2, 0, 1, 0};
        t.a = t.b = (base + fea(*inst.src, inst.size)).toCycles(isLong(inst.size));
    }

    // --- EOR (11.6.8) ---
    void timeEor(const Instruction& inst, Timing& t) {
        if (!inst.dst) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        bool longOp = isLong(inst.size);
        if (isMem(*inst.dst)) {
            // EOR Dn,EA: 4(0/1/1) + fea
            BusCost base = {0, 0, 1, 1};
            t.a = t.b = (base + fea(*inst.dst, inst.size)).toCycles(longOp);
        } else {
            // EOR Dn,Dn: 2(0/1/0)
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
        }
    }

    // --- Immediate ALU (11.6.9): ADDI/SUBI/ANDI/ORI/EORI/CMPI ---
    void timeImmAlu(const Instruction& inst, Timing& t) {
        auto* dst = getOp(inst);
        if (!dst) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        bool longOp = isLong(inst.size);

        // ANDI/ORI/EORI to SR/CCR: 14(0/2/0)
        if (dst->mode == AddrMode::sr || dst->mode == AddrMode::ccr) {
            t.a = t.b = BusCost{10, 0, 2, 0}.toCycles();
            return;
        }

        if (isMem(*dst)) {
            // #data,Mem: 4(0/1/1) + fiea — read-modify-write
            BusCost base = {0, 0, 1, 1};
            bool isCmp = (inst.mnemonic == Mnemonic::cmpi || inst.mnemonic == Mnemonic::cmp);
            if (isCmp) {
                // CMPI #data,Mem: 2(0/1/0) + fiea — read only
                base = {0, 0, 1, 0};
            }
            t.a = t.b = (base + fiea(*dst, inst.size)).toCycles(longOp);
        } else {
            // #data,Dn: 2(0/1/0) + fiea(Dn) = fiea handles it
            BusCost base = {0, 0, 1, 0};
            t.a = t.b = (base + fieaImm(inst.size)).toCycles();
        }
    }

    // --- ADDQ/SUBQ (11.6.9) ---
    void timeQuick(const Instruction& inst, Timing& t) {
        auto* dst = getOp(inst);
        if (!dst) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        if (isMem(*dst)) {
            // ADDQ #,Mem: 4(0/1/1) + fea
            BusCost base = {0, 0, 1, 1};
            t.a = t.b = (base + fea(*dst, inst.size)).toCycles(isLong(inst.size));
        } else {
            // ADDQ #,Rn: 2(0/1/0)
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
        }
    }

    // --- Unary: NEG/NEGX/NOT/CLR (11.6.11) ---
    // clrUseCea: CLR uses cea (no data read), others use fea (read-modify-write).
    void timeUnary(const Instruction& inst, Timing& t, bool clrUseCea) {
        auto op = getOp(inst);
        if (!op) {
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
            return;
        }
        if (isMem(*op)) {
            // Mem: 4(0/1/1) + fea (NEG/NOT) or + cea (CLR)
            BusCost base = {0, 0, 1, 1};
            if (clrUseCea)
                t.a = t.b = (base + cea(*op)).toCycles(isLong(inst.size));
            else
                t.a = t.b = (base + fea(*op, inst.size)).toCycles(isLong(inst.size));
        } else {
            // Dn: 2(0/1/0)
            t.a = t.b = BusCost{0, 0, 1, 0}.toCycles();
        }
    }

    // --- MUL (11.6.8) ---
    void timeMul(const Instruction& inst, Timing& t, bool /*isSigned*/) {
        BusCost eaCost = {};
        if (inst.src && isMem(*inst.src))
            eaCost = fea(*inst.src, inst.size);

        bool longMul = isLong(inst.size);
        // MULS/MULU.W: 28(0/1/0) max, MULS/MULU.L: 44(0/1/0) max
        int maxCycles = longMul ? 44 : 28;
        BusCost base = {maxCycles - 2, 0, 1, 0};

        // 020 MUL is data-dependent but we report max
        t.a = (eaCost + base).toCycles(longMul);
        t.b = t.a;
    }

    // --- DIV (11.6.8) ---
    void timeDiv(const Instruction& inst, Timing& t, bool isSigned) {
        BusCost eaCost = {};
        if (inst.src && isMem(*inst.src))
            eaCost = fea(*inst.src, inst.size);

        bool longDiv = isLong(inst.size);
        int maxCycles;
        if (isSigned)
            maxCycles = longDiv ? 90 : 56;
        else
            maxCycles = longDiv ? 78 : 44;

        BusCost base = {maxCycles - 2, 0, 1, 0};
        t.a = t.b = (eaCost + base).toCycles(longDiv);
    }

    // --- Shift/Rotate (11.6.12) ---
    void timeShift(const Instruction& inst, Timing& t) {
        auto op = getOp(inst);

        // Memory shift by 1: different per instruction
        if (op && isMem(*op) && (!inst.dst || !inst.src)) {
            BusCost base;
            if (inst.mnemonic == Mnemonic::asl) {
                base = {2, 0, 1, 1};  // ASL mem: 6(0/1/1) + fea
            } else if (inst.mnemonic == Mnemonic::rol || inst.mnemonic == Mnemonic::ror) {
                base = {2, 0, 1, 1};  // ROd mem: 6(0/1/1) + fea
            } else if (inst.mnemonic == Mnemonic::roxl || inst.mnemonic == Mnemonic::roxr) {
                // ROXd mem: 4(0/1/0) + fea (note: manual says 4(0/1/0), no write?)
                // Actually the manual has ROXd Mem as 4(0/1/0). This seems like read-only.
                // But ROXd Mem by 1 is a read-modify-write. Let's use the given timing.
                base = {0, 0, 1, 0};  // 4(0/1/0) per reference
            } else {
                base = {0, 0, 1, 1};  // LSd/ASR mem: 4(0/1/1) + fea
            }
            t.a = t.b = (base + fea(*op, OpSize::word)).toCycles();
            return;
        }

        // Register shifts:
        // LSd #imm,Dy:  4(0/1/0)
        // LSd Dx,Dy:    6(0/1/0) or 8(0/1/0) depending on count vs data size
        // ASL #imm,Dy:  6(0/1/0)   <- asymmetric!
        // ASR #imm,Dy:  4(0/1/0)
        // ASL Dx,Dy:    8(0/1/0)
        // ASR Dx,Dy:    6(0/1/0) or 10(0/1/0)
        // ROd #imm,Dy:  6(0/1/0)
        // ROd Dx,Dy:    8(0/1/0)
        // ROXd Dn:      12(0/1/0)

        bool isImm = inst.src && inst.src->mode == AddrMode::imm;

        if (inst.mnemonic == Mnemonic::roxl || inst.mnemonic == Mnemonic::roxr) {
            // ROXd Dn: 12(0/1/0) — register only
            t.a = t.b = BusCost{10, 0, 1, 0}.toCycles();
            return;
        }

        int head;
        switch (inst.mnemonic) {
        case Mnemonic::asl:
            head = isImm ? 4 : 6;  // ASL #: 6, ASL Dx,Dy: 8
            break;
        case Mnemonic::asr:
            if (isImm) {
                head = 2;  // ASR #: 4
            } else {
                head = 4;  // ASR Dx,Dy: 6 (min), 10 (max)
                t.a = BusCost{4, 0, 1, 0}.toCycles();
                t.b = BusCost{8, 0, 1, 0}.toCycles();
                return;
            }
            break;
        case Mnemonic::lsl: case Mnemonic::lsr:
            if (isImm) {
                head = 2;  // LSd #: 4
            } else {
                head = 4;  // LSd Dx,Dy: 6 (min), 8 (max)
                t.a = BusCost{4, 0, 1, 0}.toCycles();
                t.b = BusCost{6, 0, 1, 0}.toCycles();
                return;
            }
            break;
        case Mnemonic::rol: case Mnemonic::ror:
            head = isImm ? 4 : 6;  // ROd #: 6, ROd Dx,Dy: 8
            break;
        default:
            head = 2;
            break;
        }

        t.a = t.b = BusCost{head, 0, 1, 0}.toCycles();
    }

    // --- BTST (11.6.13) ---
    void timeBtst(const Instruction& inst, Timing& t) {
        bool isImm = inst.src && inst.src->mode == AddrMode::imm;

        if (inst.dst && isMem(*inst.dst)) {
            // BTST #data,Mem: 4(0/1/0) + fiea;  BTST Dn,Mem: 4(0/1/0) + fea
            BusCost base = {2, 0, 1, 0};
            if (isImm) {
                // fiea for the bit number immediate + fea for the memory operand
                BusCost immCost = fieaImm(OpSize::word);  // bit number is always word-sized imm
                t.a = t.b = (base + immCost + fea(*inst.dst, OpSize::byte)).toCycles();
            } else {
                t.a = t.b = (base + fea(*inst.dst, OpSize::byte)).toCycles();
            }
        } else {
            // BTST Dn,Dn or BTST #,Dn: 4(0/1/0)
            // Base already includes the immediate prefetch; no fieaImm needed.
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
        }
    }

    // --- BCHG/BCLR/BSET (11.6.13) ---
    void timeBitOp(const Instruction& inst, Timing& t) {
        bool isImm = inst.src && inst.src->mode == AddrMode::imm;

        if (inst.dst && isMem(*inst.dst)) {
            // BCHG/BCLR/BSET #data,Mem: 6(0/1/1) + fiea; Dn,Mem: 6(0/1/1) + fea
            BusCost base = {2, 0, 1, 1};
            if (isImm) {
                BusCost immCost = fieaImm(OpSize::word);
                t.a = t.b = (base + immCost + fea(*inst.dst, OpSize::byte)).toCycles();
            } else {
                t.a = t.b = (base + fea(*inst.dst, OpSize::byte)).toCycles();
            }
        } else {
            // BCHG/BCLR/BSET Dn,Dn or #,Dn: 6(0/1/0)
            // Base already includes the immediate prefetch; no fieaImm needed.
            t.a = t.b = BusCost{4, 0, 1, 0}.toCycles();
        }
    }

    // --- Bit field (11.6.14) ---
    // regCost: register operand. memSmall: <5 byte mem. memLarge: 5 byte mem.
    void timeBitField(const EffectiveAddr* op, Timing& t,
                      BusCost regCost, BusCost memSmall, BusCost memLarge) {
        if (op && isMem(*op)) {
            BusCost eaCost = cea(*op);
            t.a = (memSmall + eaCost).toCycles(true);
            t.b = (memLarge + eaCost).toCycles(true);
        } else {
            t.a = t.b = regCost.toCycles();
        }
    }

    // --- Bcc (11.6.15) ---
    void timeBcc(const Instruction& inst, Timing& t) {
        // Taken: 8(0/2/0)
        t.a = BusCost{4, 0, 2, 0}.toCycles();

        // Not taken depends on displacement size:
        // .B: 4(0/1/0), .W: 6(0/1/0), .L: 8(0/2/0)
        if (inst.size && *inst.size == OpSize::byte) {
            t.b = BusCost{2, 0, 1, 0}.toCycles();  // 4 NCC
        } else if (inst.size && *inst.size == OpSize::long_) {
            t.b = BusCost{4, 0, 2, 0}.toCycles();  // 8 NCC
        } else {
            t.b = BusCost{4, 0, 1, 0}.toCycles();  // 6 NCC (.W default)
        }
    }

    // --- Scc (11.6.11) ---
    void timeScc(const Instruction& inst, Timing& t) {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            // Scc Mem: 5(0/1/1) + cea
            BusCost base = {1, 0, 1, 1};
            t.a = t.b = (base + cea(*op)).toCycles();
        } else {
            // Scc Dn: 4(0/1/0)
            t.a = t.b = BusCost{2, 0, 1, 0}.toCycles();
        }
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming020() {
    return std::make_unique<Timing020>();
}
