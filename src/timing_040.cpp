#include "timing.h"
#include "m68k.h"
#include <algorithm>
#include <bit>

namespace {

// MC68040 EA costs: pipelined execution means most EA modes are 0-1 cycles.
// Index modes cost 3 due to the address calculation pipeline stage.
int eaFetch040(const EffectiveAddr& ea) {
    switch (ea.mode) {
    case AddrMode::dn:
    case AddrMode::an:
        return 0;
    case AddrMode::ind:
    case AddrMode::postInc:
    case AddrMode::preDec:
        return 0;
    case AddrMode::disp:
    case AddrMode::absW:
    case AddrMode::pcDisp:
        return 1;
    case AddrMode::index:
    case AddrMode::pcIndex:
        return 3;
    case AddrMode::absL:
        return 2;
    case AddrMode::imm:
        return 0;
    default:
        return 0;
    }
}


// MC68040 timing: pipelined, 1-clock ALU, pipeline stall detection on write→read hazards.
class Timing040 : public TimingBase {
public:
    using TimingBase::time;

    Timing computeTime(const Instruction& inst) override {
        Timing t;

        switch (inst.mnemonic) {
        case Mnemonic::move:
        case Mnemonic::movea:
            t.a = t.b = timeMove(inst);
            break;

        case Mnemonic::moveq:
            t.a = t.b = 1;
            break;

        case Mnemonic::lea:
            if (inst.src) {
                t.a = t.b = 1 + eaFetch040(*inst.src);
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::pea:
            if (inst.src) {
                t.a = t.b = 2 + eaFetch040(*inst.src);
            } else {
                t.a = t.b = 2;
            }
            break;

        case Mnemonic::exg:
        case Mnemonic::swap:
        case Mnemonic::ext:
            t.a = t.b = 1;
            break;

        case Mnemonic::link:
            t.a = t.b = 3;
            break;

        case Mnemonic::unlk:
            t.a = t.b = 2;
            break;

        case Mnemonic::movem:
            t.a = t.b = timeMovem(inst);
            break;

        case Mnemonic::movep:
            t.a = t.b = isLong(inst.size) ? 6 : 4;
            break;

        case Mnemonic::add: case Mnemonic::sub:
        case Mnemonic::and_: case Mnemonic::or_:
        case Mnemonic::eor: case Mnemonic::cmp:
        case Mnemonic::adda: case Mnemonic::suba: case Mnemonic::cmpa:
            t.a = t.b = timeAlu(inst);
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
                t.a = t.b = 4;
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::mulu:
        case Mnemonic::muls:
            t.a = t.b = timeMul(inst);
            break;

        case Mnemonic::divu: {
            int ea = inst.src ? eaFetch040(*inst.src) : 0;
            t.a = 20 + ea;
            t.b = 22 + ea;
            break;
        }
        case Mnemonic::divs: {
            int ea = inst.src ? eaFetch040(*inst.src) : 0;
            t.a = 26 + ea;
            t.b = 28 + ea;
            break;
        }

        case Mnemonic::neg: case Mnemonic::negx:
        case Mnemonic::not_: case Mnemonic::clr: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 2 + eaFetch040(*op);
            } else {
                t.a = t.b = 1;
            }
            break;
        }

        case Mnemonic::tst: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 1 + eaFetch040(*op);
            } else {
                t.a = t.b = 1;
            }
            break;
        }

        case Mnemonic::chk:
            if (inst.src) {
                t.a = t.b = 4 + eaFetch040(*inst.src);
            } else {
                t.a = t.b = 4;
            }
            break;

        case Mnemonic::asl: case Mnemonic::asr:
        case Mnemonic::lsl: case Mnemonic::lsr:
        case Mnemonic::rol: case Mnemonic::ror:
        case Mnemonic::roxl: case Mnemonic::roxr: {
            auto op = getOp(inst);
            if (op && isMem(*op) && (!inst.dst || !inst.src)) {
                t.a = t.b = 2 + eaFetch040(*op);
            } else {
                t.a = t.b = 1;
            }
            break;
        }

        case Mnemonic::btst:
            if (inst.dst && isMem(*inst.dst)) {
                t.a = t.b = 1 + eaFetch040(*inst.dst);
            } else {
                t.a = t.b = 1;
            }
            break;

        case Mnemonic::bset: case Mnemonic::bclr: case Mnemonic::bchg:
            if (inst.dst && isMem(*inst.dst)) {
                t.a = t.b = 2 + eaFetch040(*inst.dst);
            } else {
                t.a = t.b = 2;
            }
            break;

        case Mnemonic::bra:
            t.a = t.b = 2;
            break;

        case Mnemonic::bsr:
            t.a = t.b = 3;
            break;

        case Mnemonic::bhi: case Mnemonic::bls: case Mnemonic::bcc:
        case Mnemonic::bcs: case Mnemonic::bne: case Mnemonic::beq:
        case Mnemonic::bvc: case Mnemonic::bvs: case Mnemonic::bpl:
        case Mnemonic::bmi: case Mnemonic::bge: case Mnemonic::blt:
        case Mnemonic::bgt: case Mnemonic::ble:
            t.a = 2;   // taken (predicted)
            t.b = 1;   // not taken
            break;

        case Mnemonic::dbt:
            t.a = t.b = 2;
            break;

        case Mnemonic::dbf:
        case Mnemonic::dbhi: case Mnemonic::dbls: case Mnemonic::dbcc:
        case Mnemonic::dbcs: case Mnemonic::dbne: case Mnemonic::dbeq:
        case Mnemonic::dbvc: case Mnemonic::dbvs: case Mnemonic::dbpl:
        case Mnemonic::dbmi: case Mnemonic::dbge: case Mnemonic::dblt:
        case Mnemonic::dbgt: case Mnemonic::dble:
            t.a = 3;   // loop back
            t.b = 4;   // fall through
            break;

        case Mnemonic::st: case Mnemonic::sf:
        case Mnemonic::shi: case Mnemonic::sls: case Mnemonic::scc:
        case Mnemonic::scs: case Mnemonic::sne: case Mnemonic::seq:
        case Mnemonic::svc: case Mnemonic::svs: case Mnemonic::spl:
        case Mnemonic::smi: case Mnemonic::sge: case Mnemonic::slt:
        case Mnemonic::sgt: case Mnemonic::sle: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 2 + eaFetch040(*op);
            } else {
                t.a = t.b = 1;
            }
            break;
        }

        case Mnemonic::jmp:
            t.a = t.b = 2;
            break;

        case Mnemonic::jsr:
            t.a = t.b = 3;
            break;

        case Mnemonic::rts:
            t.a = t.b = 3;
            break;

        case Mnemonic::rte:
            t.a = t.b = 4;
            break;

        case Mnemonic::rtr:
            t.a = t.b = 3;
            break;

        case Mnemonic::nop:
            t.a = t.b = 1;
            break;

        case Mnemonic::reset:
            t.a = t.b = 132;
            break;

        case Mnemonic::stop:
            t.a = t.b = 1;
            break;

        case Mnemonic::trap:
            t.a = t.b = 8;
            break;

        case Mnemonic::trapv:
            t.a = 1;
            t.b = 8;
            break;

        case Mnemonic::illegal:
            t.a = t.b = 8;
            break;

        case Mnemonic::tas: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 2 + eaFetch040(*op);
            } else {
                t.a = t.b = 1;
            }
            break;
        }

        case Mnemonic::moveToSR:
        case Mnemonic::moveToCCR:
            t.a = t.b = 3;
            break;

        case Mnemonic::moveFromSR:
            t.a = t.b = 1;
            break;

        case Mnemonic::moveUSP:
            t.a = t.b = 1;
            break;

        case Mnemonic::abcd: case Mnemonic::sbcd:
            if (inst.src && inst.src->mode == AddrMode::preDec) {
                t.a = t.b = 4;
            } else {
                t.a = t.b = 2;
            }
            break;

        case Mnemonic::nbcd: {
            auto op = getOp(inst);
            if (op && isMem(*op)) {
                t.a = t.b = 3;
            } else {
                t.a = t.b = 2;
            }
            break;
        }

        case Mnemonic::cmpm:
            t.a = t.b = 2;
            break;

        // Bit field (pipelined on 040)
        case Mnemonic::bftst: case Mnemonic::bfextu: case Mnemonic::bfexts:
        case Mnemonic::bfffo: case Mnemonic::bfchg: case Mnemonic::bfclr:
        case Mnemonic::bfset: case Mnemonic::bfins:
            if ((inst.src && isMem(*inst.src)) || (inst.dst && isMem(*inst.dst))) {
                t.a = t.b = 3;
            } else {
                t.a = t.b = 1;
            }
            break;

        default:
            t.a = t.b = 1;
            break;
        }

        return t;
    }

    // Pipeline-aware block timing: detect write->read stalls
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
    int timeMove(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 1;
        int ea = 0;
        if (isMem(*inst.src)) { ea += eaFetch040(*inst.src); }
        if (isMem(*inst.dst)) { ea += eaFetch040(*inst.dst); }
        return 1 + ea;
    }

    int timeMovem(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 2;
        // 040: approximately 1 cycle per register + overhead
        return 2 + movemRegCount(inst);
    }

    int timeAlu(const Instruction& inst) {
        if (!inst.src || !inst.dst) return 1;
        if (isMem(*inst.dst)) {
            return 2 + eaFetch040(*inst.dst);
        }
        if (isMem(*inst.src)) {
            return 1 + eaFetch040(*inst.src);
        }
        return 1;
    }

    int timeImmediate(const Instruction& inst) {
        if (!inst.dst) return 1;
        if (isMem(*inst.dst)) {
            return 2 + eaFetch040(*inst.dst);
        }
        return 1;
    }

    int timeQuick(const Instruction& inst) {
        if (!inst.dst) return 1;
        if (isMem(*inst.dst)) {
            return 2 + eaFetch040(*inst.dst);
        }
        return 1;
    }

    int timeMul(const Instruction& inst) {
        int ea = inst.src ? eaFetch040(*inst.src) : 0;
        // 040: MULU/MULS.W = 3-4 cycles
        return 3 + ea;
    }
};

} // anonymous namespace

std::unique_ptr<TimingBase> makeTiming040() {
    return std::make_unique<Timing040>();
}
