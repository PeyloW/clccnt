#include "timing.h"

// Forward: 020 factory
std::unique_ptr<TimingBase> makeTiming020();

namespace {

// MC68030: same instruction timing as 020, adds data cache.
class Timing030 : public TimingBase {
    std::unique_ptr<TimingBase> base;
public:
    using TimingBase::time;

    Timing030() : base(makeTiming020()) {}

    // Same instruction timing as 020
    Timing computeTime(const Instruction& inst) override {
        return base->time(inst);
    }

    Timing time(std::span<const Instruction> block) override {
        return base->time(block);
    }

    int busAccessCycles() const override { return 2; }
    int busWidth() const override { return 4; }
    const char* name() const override { return "MC68030"; }

    std::optional<CacheConfig> iCache() const override {
        return CacheConfig{256, 4, 1};
    }

    std::optional<CacheConfig> dCache() const override {
        return CacheConfig{256, 4, 1};
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming030() {
    return std::make_unique<Timing030>();
}
