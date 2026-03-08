#include "timing_scalar.h"
#include "m68k.h"

namespace {

// MC68020 EA fetch timing: faster than 000 due to instruction cache and 32-bit bus.
constexpr int eaFetch020[12][2] = {
    {0, 0},   // Dn
    {0, 0},   // An
    {4, 8},   // (An)
    {4, 8},   // (An)+
    {4, 8},   // -(An)
    {6, 10},  // d(An)
    {8, 12},  // d(An,Xn)
    {6, 10},  // xxx.W
    {8, 12},  // xxx.L
    {6, 10},  // d(PC)
    {8, 12},  // d(PC,Xn)
    {4, 8},   // #imm
};

// MC68020 EA store timing.
constexpr int eaStore020[12][2] = {
    {0, 0},   // Dn
    {0, 0},   // An
    {4, 8},   // (An)
    {4, 8},   // (An)+
    {4, 8},   // -(An)
    {6, 10},  // d(An)
    {8, 12},  // d(An,Xn)
    {6, 10},  // xxx.W
    {8, 12},  // xxx.L
    {0, 0},   // d(PC)
    {0, 0},   // d(PC,Xn)
    {0, 0},   // #imm
};

// MC68020 timing constants (2-cycle bus, 32-bit bus width, 256B I-cache).
constexpr ScalarConst k020 = {
    .moveBase = 2,
    .moveqCycles = 2,
    .movepCycles = {12, 16},
    .linkCycles = 10, .unlkCycles = 6,
    .exgCycles = 4, .swapCycles = 4, .extCycles = 4,
    .braCycles = 6, .bsrCycles = 12,
    .dbtCycles = 8, .dbccLoop = 6, .dbccFall = 10,
    .rtsCycles = 10, .rteCycles = 16, .rtrCycles = 14,
    .nopCycles = 2, .resetCycles = 132, .stopCycles = 4,
    .trapCycles = 20, .trapvMax = 20, .illegalCycles = 20,
    .moveUSPCycles = 4,
    .cmpmCycles = {8, 12},
    .abcdMem = 14, .abcdReg = 4,
    .chkBase = 8,
    .defaultCycles = 2,
    .eorDefault = 2, .eorMem = {4, 4}, .eorReg = {2, 2},
    .immDefault = 4, .immSRCCR = 12,
    .immMem = {6, 6}, .immReg = {4, 6},
    .quickDefault = 2, .quickAn = 4,
    .quickMem = {4, 4}, .quickReg = {2, 2},
    .unaryDefault = 2,
    .unaryMem = {4, 4}, .unaryReg = {2, 2},
    .mulBase = 28, .mulMax = 44,
    .divuMin = 44, .divuMax = 44,
    .divsMin = 56, .divsMax = 56,
    .addxMem = {10, 14}, .addxReg = {2, 4},
    .tstBase = 2,
    .tasMemBase = 6, .tasReg = 4,
    .moveToSRBase = 8, .moveToCCRBase = 8,
    .moveFromSRMem = 6, .moveFromSRReg = 4,
    .nbcdMem = 6, .nbcdReg = 4,
    .lea = {2, 4, 6, 4, 6, 4, 6, 2},
    .pea = {6, 8, 10, 8, 10, 8, 10, 6},
    .jmp = {4, 6, 8, 6, 8, 6, 8, 4},
    .jsr = {8, 10, 12, 10, 12, 10, 12, 8},
};

// MC68020 timing: non-pipelined, 2-cycle bus, 32-bit data bus, 256B I-cache.
class Timing020 : public TimingScalar {
public:
    Timing020() : TimingScalar(eaFetch020, eaStore020, k020) {}

    int busAccessCycles() const override { return 2; }
    int busWidth() const override { return 4; }
    const char* name() const override { return "MC68020"; }

    std::optional<CacheConfig> iCache() const override {
        return CacheConfig{256, 4, 1};
    }

protected:
    int timeMovem(const Instruction& inst) override {
        if (!inst.src || !inst.dst) return 8;
        int perReg = isLong(inst.size) ? 4 : 2;
        return 8 + movemRegCount(inst) * perReg;
    }

    int timeAlu(const Instruction& inst) override {
        if (!inst.src || !inst.dst) return 2;
        if (inst.mnemonic == Mnemonic::cmp)
            return 2 + eaFetch(*inst.src, inst.size);
        if (isMem(*inst.dst)) {
            return 4 + eaFetch(*inst.dst, inst.size);
        }
        return 2 + eaFetch(*inst.src, inst.size);
    }

    int timeAdda(const Instruction& inst) override {
        if (!inst.src) return 2;
        return 2 + eaFetch(*inst.src, inst.size);
    }

    int timeCmpa(const Instruction& inst) override {
        return timeAdda(inst);
    }

    void timeShift(const Instruction& inst, Timing& t) override {
        auto op = getOp(inst);
        if (op && isMem(*op) && (!inst.dst || !inst.src)) {
            t.a = t.b = 4 + eaFetch(*op, OpSize::word);
            return;
        }
        t.a = t.b = 4;
    }

    int timeBtst(const Instruction& inst) override {
        if (inst.dst && isMem(*inst.dst)) {
            return 4 + eaFetch(*inst.dst, OpSize::byte);
        }
        return 4;
    }

    int timeBitOp(const Instruction& inst) override {
        if (inst.dst && isMem(*inst.dst)) {
            return 6 + eaFetch(*inst.dst, OpSize::byte);
        }
        return 6;
    }

    void timeScc(const Instruction& inst, Timing& t) override {
        auto op = getOp(inst);
        if (op && isMem(*op)) {
            t.a = t.b = 6 + eaFetch(*op, OpSize::byte);
        } else {
            t.a = t.b = 4;
        }
    }

    void timeBcc(const Instruction& /*inst*/, Timing& t) override {
        t.a = 6; // taken
        t.b = 4; // not taken
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming020() {
    return std::make_unique<Timing020>();
}
