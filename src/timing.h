#pragma once

#include "types.h"
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// I-cache or D-cache geometry for a CPU model.
struct CacheConfig {
    size_t size;
    size_t lineSize;
    size_t ways;
};

// Bitmask of registers read and written by an instruction (0-7=Dn, 8-15=An).
// Used by 040/060 pipeline analysis to detect data hazards between instructions.
struct RegUsage {
    uint16_t read = 0;
    uint16_t write = 0;
};

// Determines which registers an instruction reads and writes.
RegUsage regUsage(const Instruction& inst);
// Computes the byte length of an encoded instruction (always positive and even).
int computeInstSize(const Instruction& inst);

// Globals
inline int busCycles = 4;
inline int busWidthBytes = 2;
inline double timingEstimate = 0.5;

// Rounds cycle count up to the next bus access boundary.
inline int roundToBus(int cycles) {
    if (busCycles <= 1 || cycles <= 0) return cycles;
    return ((cycles + busCycles - 1) / busCycles) * busCycles;
}

// Collapses a variable-timing result to a single bus-rounded value using timingEstimate.
// 0.0 = best case (min), 1.0 = worst case (max), 0.5 = midpoint.
inline Timing estimateTiming(Timing t) {
    if (t.a != t.b) {
        int lo = t.min(), hi = t.max();
        int v = roundToBus(static_cast<int>(
            std::round(lo + timingEstimate * (hi - lo))));
        t.a = t.b = v;
    }
    return t;
}

// Decomposed bus timing for 020/030: separates internal cycles from bus accesses.
// Allows accurate computation for different bus widths (16-bit vs 32-bit).
struct BusCost {
    int head = 0;        // internal cycles (no bus access)
    int reads = 0;       // data read bus accesses
    int prefetches = 0;  // instruction prefetch accesses (always long-word)
    int writes = 0;      // data write bus accesses

    // Convert to cycles. dataLong=true when data reads/writes are long-word sized.
    int toCycles(bool dataLong = false) const {
        int dataScale = (dataLong && busWidthBytes < 4) ? 2 : 1;
        int pfScale = (busWidthBytes < 4) ? 2 : 1;
        return head
            + (reads * dataScale + writes * dataScale) * busCycles
            + prefetches * pfScale * busCycles;
    }

    BusCost operator+(BusCost rhs) const {
        return {head + rhs.head, reads + rhs.reads,
                prefetches + rhs.prefetches, writes + rhs.writes};
    }
};

// Abstract interface for CPU-specific instruction timing.
// Subclasses override computeTime(); the public time() wrapper applies bus rounding.
class TimingBase {
public:
    virtual ~TimingBase() = default;

    // Returns timing for an instruction. Default applies bus rounding;
    // 020/030 override to return exact (non-rounded) values.
    virtual Timing time(const Instruction& inst) {
        auto t = computeTime(inst);
        t.a = roundToBus(t.a);
        t.b = roundToBus(t.b);
        return t;
    }

    virtual Timing time(std::span<const Instruction> block) {
        Timing total;
        for (auto& inst : block) {
            total += time(inst);
        }
        return total;
    }

    // Returns a vector marking which instructions in a block are paired with
    // the previous instruction (dual-issue). Default: no pairing.
    virtual std::vector<bool> pairings(std::span<const Instruction> block) {
        return std::vector<bool>(block.size(), false);
    }

    // Returns a vector marking which instructions in a block are stalled
    // by a pipeline hazard from the previous instruction. Default: no stalls.
    virtual std::vector<bool> stalls(std::span<const Instruction> block) {
        return std::vector<bool>(block.size(), false);
    }

    virtual int busAccessCycles() const { return 4; }
    virtual int busWidth() const { return 2; }
    virtual const char* name() const = 0;
    virtual std::optional<CacheConfig> iCache() const { return std::nullopt; }
    virtual std::optional<CacheConfig> dCache() const { return std::nullopt; }

protected:
    // Subclasses implement raw (pre-rounding) instruction timing here.
    virtual Timing computeTime(const Instruction& inst) = 0;
};

// Factory that returns the timing engine for a CPU name ("000", "020", etc.).
std::unique_ptr<TimingBase> createTiming(std::string_view cpu);
