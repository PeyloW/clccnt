#include "timing.h"
#include "m68k.h"
#include <bit>
#include "error.h"

// Forward declarations for CPU factory functions
std::unique_ptr<TimingBase> makeTiming000();
std::unique_ptr<TimingBase> makeTiming010();
std::unique_ptr<TimingBase> makeTiming020();
std::unique_ptr<TimingBase> makeTiming030();
std::unique_ptr<TimingBase> makeTiming040();
std::unique_ptr<TimingBase> makeTiming060();

std::unique_ptr<TimingBase> createTiming(std::string_view cpu) {
    if (cpu == "000" || cpu == "68000") return makeTiming000();
    if (cpu == "010" || cpu == "68010") return makeTiming010();
    if (cpu == "020" || cpu == "68020") return makeTiming020();
    if (cpu == "030" || cpu == "68030") return makeTiming030();
    if (cpu == "040" || cpu == "68040") return makeTiming040();
    if (cpu == "060" || cpu == "68060") return makeTiming060();
    return nullptr;
}

// Returns the byte size of an EA's extension word(s) in the instruction encoding.
static int extensionSize(const EffectiveAddr& ea, std::optional<OpSize> instSize) {
    switch (ea.mode) {
    case AddrMode::imm:
        return isLong(instSize) ? 4 : 2;
    case AddrMode::disp:
    case AddrMode::index:
    case AddrMode::absW:
    case AddrMode::pcDisp:
    case AddrMode::pcIndex:
        return 2;
    case AddrMode::absL:
        return 4;
    default:
        return 0;
    }
}

int computeInstSize(const Instruction& inst) {
    int sz = 2; // opcode word

    // Special cases: value encoded in opcode, no extension
    if (inst.mnemonic == Mnemonic::moveq) return 2;
    if (inst.mnemonic == Mnemonic::addq || inst.mnemonic == Mnemonic::subq) {
        // Immediate is in opcode; dst may have extension
        if (inst.dst) sz += extensionSize(*inst.dst, inst.size);
        return sz;
    }
    if (inst.mnemonic == Mnemonic::trap) return 2;
    if (inst.mnemonic == Mnemonic::link) return 4; // opcode + 16-bit displacement

    // Branches
    if (isBranch(inst.mnemonic) || inst.mnemonic == Mnemonic::bsr) {
        if (inst.size && *inst.size == OpSize::byte) return 2; // byte displacement in opcode
        return 4; // word displacement
    }
    if (isDbcc(inst.mnemonic)) return 4; // opcode + 16-bit displacement

    if (inst.src) sz += extensionSize(*inst.src, inst.size);
    if (inst.dst) sz += extensionSize(*inst.dst, inst.size);

    if (sz <= 0 || (sz & 1) != 0) {
        throw Error{2, "internal: instruction '" + inst.toString() +
            "' computed size " + std::to_string(sz) + " (must be positive and even)"};
    }

    return sz;
}

// Determines which registers an instruction reads and writes.
// Post-increment and pre-decrement modes both read and write the address register.
RegUsage regUsage(const Instruction& inst) {
    RegUsage u;

    auto addRead = [&](const EffectiveAddr& ea) {
        switch (ea.mode) {
        case AddrMode::dn:
            u.read |= regBit(ea.reg); break;
        case AddrMode::an:
            u.read |= regBit(ea.reg + 8); break;
        case AddrMode::ind: case AddrMode::disp: case AddrMode::index:
            u.read |= regBit(ea.reg + 8);
            if (ea.mode == AddrMode::index) {
                u.read |= regBit(ea.indexReg);
            }
            break;
        case AddrMode::postInc: case AddrMode::preDec:
            u.read |= regBit(ea.reg + 8);
            u.write |= regBit(ea.reg + 8); // also modified
            break;
        default: break;
        }
    };

    auto addWrite = [&](const EffectiveAddr& ea) {
        switch (ea.mode) {
        case AddrMode::dn:
            u.write |= regBit(ea.reg); break;
        case AddrMode::an:
            u.write |= regBit(ea.reg + 8); break;
        case AddrMode::postInc: case AddrMode::preDec:
            u.write |= regBit(ea.reg + 8); break;
        default: break;
        }
    };

    // Single-operand instructions: operand is in src, no dst.
    // The operand is the target being read+written (or just written for CLR/Scc).
    bool singleOp = !inst.dst && inst.src &&
        (inst.mnemonic == Mnemonic::clr || inst.mnemonic == Mnemonic::neg ||
         inst.mnemonic == Mnemonic::negx || inst.mnemonic == Mnemonic::not_ ||
         inst.mnemonic == Mnemonic::tst || inst.mnemonic == Mnemonic::nbcd ||
         inst.mnemonic == Mnemonic::tas || isScc(inst.mnemonic) ||
         isShift(inst.mnemonic));

    if (singleOp) {
        auto& ea = *inst.src;
        // TST is read-only; CLR/Scc are pure write; rest are read+write
        if (inst.mnemonic == Mnemonic::tst) {
            addRead(ea);
        } else if (inst.mnemonic == Mnemonic::clr || isScc(inst.mnemonic)) {
            if (isMem(ea)) {
                addRead(ea);  // address calc reads base/index regs
            }
            addWrite(ea);
        } else {
            addRead(ea);
            addWrite(ea);
        }
    } else {
        if (inst.src) addRead(*inst.src);
    }

    if (inst.dst) {
        // For most instructions, dst is both read and written if it's a register
        auto m = inst.dst->mode;
        if (m == AddrMode::dn || m == AddrMode::an) {
            // Pure write for MOVE/LEA/MOVEQ, read+write for ALU
            if (inst.mnemonic == Mnemonic::move || inst.mnemonic == Mnemonic::movea ||
                inst.mnemonic == Mnemonic::moveq || inst.mnemonic == Mnemonic::lea) {
                addWrite(*inst.dst);
            } else {
                addRead(*inst.dst);
                addWrite(*inst.dst);
            }
        } else {
            addRead(*inst.dst);  // address calc reads
            // Memory destination: no register write (just memory)
        }
    }

    return u;
}
