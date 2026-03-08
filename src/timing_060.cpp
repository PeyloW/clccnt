#include "timing.h"
#include "m68k.h"
#include <algorithm>
#include <bit>

// Forward: 040 factory
std::unique_ptr<TimingBase> makeTiming040();

namespace {

// Checks if an instruction is eligible for 060 superscalar dual-issue pairing.
// Only simple ALU and shift instructions can pair; mul/div/branches cannot.
bool isPairable(Mnemonic m) {
    switch (m) {
    case Mnemonic::move: case Mnemonic::movea: case Mnemonic::moveq:
    case Mnemonic::add: case Mnemonic::adda: case Mnemonic::addi:
    case Mnemonic::addq:
    case Mnemonic::sub: case Mnemonic::suba: case Mnemonic::subi:
    case Mnemonic::subq:
    case Mnemonic::and_: case Mnemonic::andi:
    case Mnemonic::or_: case Mnemonic::ori:
    case Mnemonic::eor: case Mnemonic::eori:
    case Mnemonic::cmp: case Mnemonic::cmpa: case Mnemonic::cmpi:
    case Mnemonic::not_: case Mnemonic::neg: case Mnemonic::negx:
    case Mnemonic::clr: case Mnemonic::ext: case Mnemonic::swap:
    case Mnemonic::tst:
    case Mnemonic::asl: case Mnemonic::asr:
    case Mnemonic::lsl: case Mnemonic::lsr:
    case Mnemonic::rol: case Mnemonic::ror:
    case Mnemonic::roxl: case Mnemonic::roxr:
        return true;
    default:
        return false;
    }
}

// MC68060 timing: superscalar dual-issue with faster MUL/DIV.
// Wraps 040 timing and overrides MUL/DIV costs and block-level pairing.
class Timing060 : public TimingBase {
    std::unique_ptr<TimingBase> base;
public:
    using TimingBase::time;

    Timing060() : base(makeTiming040()) {}

    Timing computeTime(const Instruction& inst) override {
        auto t = base->time(inst);

        // 060: MUL/DIV are faster (superscalar, flat cost)
        if (inst.mnemonic == Mnemonic::mulu || inst.mnemonic == Mnemonic::muls) {
            t.a = t.b = 2;
        } else if (inst.mnemonic == Mnemonic::divu) {
            t.a = t.b = 18;
        } else if (inst.mnemonic == Mnemonic::divs) {
            t.a = t.b = 22;
        }

        return t;
    }

    // Dual-issue pairing detection
    Timing time(std::span<const Instruction> block) override {
        Timing total;
        size_t i = 0;
        while (i < block.size()) {
            auto t = time(block[i]);

            // Try to pair with next instruction
            if (i + 1 < block.size() &&
                isPairable(block[i].mnemonic) &&
                isPairable(block[i + 1].mnemonic)) {
                auto prev = regUsage(block[i]);
                auto next = regUsage(block[i + 1]);

                // Can dual-issue if no data dependency
                if (!(prev.write & next.read) && !(prev.write & next.write)) {
                    auto t2 = time(block[i + 1]);
                    // Both execute in parallel: take the max of the two
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
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming060() {
    return std::make_unique<Timing060>();
}
