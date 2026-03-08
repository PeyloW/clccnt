#include "timing.h"
#include "m68k.h"

// Forward: 000 factory so we can inherit
std::unique_ptr<TimingBase> makeTiming000();

namespace {

// MC68010: same instruction timing as 000 (loop-mode DBcc optimization not modeled).
class Timing010 : public TimingBase {
    std::unique_ptr<TimingBase> base;
public:
    using TimingBase::time;

    Timing010() : base(makeTiming000()) {}

    Timing computeTime(const Instruction& inst) override {
        auto t = base->time(inst);

        // 68010 loop mode: DBcc to self in a 3-word loop buffer
        // runs 2 cycles faster per iteration. We approximate this
        // by reducing DBcc taken cost by 2 when it's a tight loop.
        // The analysis phase determines if the loop is tight enough.
        // For individual instruction timing, we use standard 000 values.

        return t;
    }

    Timing time(std::span<const Instruction> block) override {
        return base->time(block);
    }

    int busAccessCycles() const override { return 4; }
    int busWidth() const override { return 2; }
    const char* name() const override { return "MC68010"; }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming010() {
    return std::make_unique<Timing010>();
}
