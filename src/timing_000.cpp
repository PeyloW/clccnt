#include "timing_scalar.h"
#include "m68k.h"

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

// MC68000 timing constants (4-cycle bus, 16-bit bus width).
constexpr ScalarConst k000 = {
    .moveBase = 4,
    .moveqCycles = 4,
    .movepCycles = {16, 24},
    .linkCycles = 16, .unlkCycles = 12,
    .exgCycles = 6, .swapCycles = 4, .extCycles = 4,
    .braCycles = 10, .bsrCycles = 18,
    .dbtCycles = 12, .dbccLoop = 10, .dbccFall = 14,
    .rtsCycles = 16, .rteCycles = 20, .rtrCycles = 20,
    .nopCycles = 4, .resetCycles = 132, .stopCycles = 4,
    .trapCycles = 34, .trapvMax = 34, .illegalCycles = 34,
    .moveUSPCycles = 4,
    .cmpmCycles = {12, 20},
    .abcdMem = 18, .abcdReg = 6,
    .chkBase = 10,
    .defaultCycles = 4,
    .eorDefault = 4, .eorMem = {8, 12}, .eorReg = {4, 8},
    .immDefault = 8, .immSRCCR = 20,
    .immMem = {12, 20}, .immReg = {8, 16},
    .quickDefault = 4, .quickAn = 8,
    .quickMem = {8, 12}, .quickReg = {4, 8},
    .unaryDefault = 4,
    .unaryMem = {8, 12}, .unaryReg = {4, 6},
    .mulBase = 38, .mulMax = 70,
    .divuMin = 76, .divuMax = 140,
    .divsMin = 120, .divsMax = 158,
    .addxMem = {18, 30}, .addxReg = {4, 8},
    .tstBase = 4,
    .tasMemBase = 10, .tasReg = 4,
    .moveToSRBase = 12, .moveToCCRBase = 12,
    .moveFromSRMem = 8, .moveFromSRReg = 6,
    .nbcdMem = 8, .nbcdReg = 6,
    .lea = {4, 8, 12, 8, 12, 8, 12, 4},
    .pea = {12, 16, 20, 16, 20, 16, 20, 12},
    .jmp = {8, 10, 14, 10, 12, 10, 14, 8},
    .jsr = {16, 18, 22, 18, 20, 18, 22, 16},
};

// MC68000 timing: non-pipelined, 4-cycle bus, 16-bit data bus.
class Timing000 : public TimingScalar {
public:
    Timing000() : TimingScalar(eaFetchTable, eaStoreTable, k000) {}
    const char* name() const override { return "MC68000"; }

protected:
    int timeMovem(const Instruction& inst) override {
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

    int timeAlu(const Instruction& inst) override {
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

    int timeAdda(const Instruction& inst) override {
        if (!inst.src) return 8;
        if (isLong(inst.size)) {
            if (isRegDirect(inst.src->mode) || inst.src->mode == AddrMode::imm) {
                return 8 + eaFetch(*inst.src, inst.size);
            }
            return 6 + eaFetch(*inst.src, inst.size);
        }
        return 8 + eaFetch(*inst.src, OpSize::word);
    }

    int timeCmpa(const Instruction& inst) override {
        if (!inst.src) return 6;
        return 6 + eaFetch(*inst.src, inst.size);
    }

    void timeShift(const Instruction& inst, Timing& t) override {
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

    int timeBtst(const Instruction& inst) override {
        if (!inst.dst) return 4;
        if (isMem(*inst.dst)) {
            int base = (inst.src && inst.src->mode == AddrMode::imm) ? 8 : 4;
            return base + eaFetch(*inst.dst, OpSize::byte);
        }
        return (inst.src && inst.src->mode == AddrMode::imm) ? 10 : 6;
    }

    int timeBitOp(const Instruction& inst) override {
        if (!inst.dst) return 8;
        if (isMem(*inst.dst)) {
            int base = (inst.src && inst.src->mode == AddrMode::imm) ? 12 : 8;
            return base + eaFetch(*inst.dst, OpSize::byte);
        }
        if (inst.mnemonic == Mnemonic::bclr)
            return (inst.src && inst.src->mode == AddrMode::imm) ? 14 : 10;
        return (inst.src && inst.src->mode == AddrMode::imm) ? 12 : 8;
    }

    void timeScc(const Instruction& inst, Timing& t) override {
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

    void timeBcc(const Instruction& inst, Timing& t) override {
        bool isByte = (inst.size && *inst.size == OpSize::byte);
        t.a = 10;
        t.b = isByte ? 8 : 12;
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming000() {
    return std::make_unique<Timing000>();
}
