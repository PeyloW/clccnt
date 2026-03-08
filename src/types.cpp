#include "types.h"
#include "m68k.h"
#include "error.h"
#include <string>

const char* mnemonicName(Mnemonic m) {
    static const char* names[] = {
        // Data movement
        "move", "movea", "moveq", "movem", "movep",
        "lea", "pea", "exg", "swap", "link", "unlk",
        // Arithmetic
        "add", "adda", "addi", "addq", "addx",
        "sub", "suba", "subi", "subq", "subx",
        "mulu", "muls", "divu", "divs",
        "neg", "negx", "clr", "ext", "chk",
        "cmp", "cmpa", "cmpi", "cmpm", "tst",
        // Logic
        "and", "andi", "or", "ori", "eor", "eori", "not",
        // Shift/Rotate
        "asl", "asr", "lsl", "lsr", "rol", "ror", "roxl", "roxr",
        // Bit manipulation
        "btst", "bset", "bclr", "bchg",
        // Bit field (020+)
        "bfchg", "bfclr", "bfexts", "bfextu", "bfffo", "bfins", "bfset", "bftst",
        // Branches
        "bra", "bsr",
        "bhi", "bls", "bcc", "bcs", "bne", "beq",
        "bvc", "bvs", "bpl", "bmi", "bge", "blt", "bgt", "ble",
        // DBcc
        "dbt", "dbf",
        "dbhi", "dbls", "dbcc", "dbcs", "dbne", "dbeq",
        "dbvc", "dbvs", "dbpl", "dbmi", "dbge", "dblt", "dbgt", "dble",
        // Scc
        "st", "sf",
        "shi", "sls", "scc", "scs", "sne", "seq",
        "svc", "svs", "spl", "smi", "sge", "slt", "sgt", "sle",
        // Jump
        "jmp", "jsr",
        // System
        "rts", "rte", "rtr", "nop", "reset", "stop",
        "trap", "trapv", "illegal",
        "tas",
        // Special moves (auto-promoted from Move)
        "move", "move", "move", "move",
        // BCD
        "abcd", "sbcd", "nbcd",
    };
    int idx = static_cast<int>(m);
    if (idx >= 0 && idx < static_cast<int>(Mnemonic::count_)) {
        return names[idx];
    }
    return "???";
}

namespace {

// Formats an index register with size suffix (e.g., "d0.w", "a3.l").
std::string indexStr(int reg, OpSize size) {
    char type = reg >= 8 ? 'a' : 'd';
    int num = reg >= 8 ? reg - 8 : reg;
    std::string r(1, type);
    r += char('0' + num);
    r += (size == OpSize::long_) ? ".l" : ".w";
    return r;
}

// Formats a 16-bit register bitmask as a compact register list (e.g., "d0-d3/a0-a2").
std::string regListStr(uint16_t mask) {
    std::string result;
    auto addRange = [&](char type, int start, int end) {
        if (!result.empty()) result += '/';
        result += type;
        result += char('0' + start);
        if (end > start) {
            result += '-';
            result += type;
            result += char('0' + end);
        }
    };
    int rangeStart = -1;
    for (int i = 0; i <= 8; i++) {
        if (i < 8 && (mask & (1 << i))) {
            if (rangeStart < 0) rangeStart = i;
        } else if (rangeStart >= 0) {
            addRange('d', rangeStart, i - 1);
            rangeStart = -1;
        }
    }
    rangeStart = -1;
    for (int i = 0; i <= 8; i++) {
        if (i < 8 && (mask & (1 << (i + 8)))) {
            if (rangeStart < 0) rangeStart = i;
        } else if (rangeStart >= 0) {
            addRange('a', rangeStart, i - 1);
            rangeStart = -1;
        }
    }
    return result;
}

} // namespace

std::string EffectiveAddr::toString() const {
    switch (mode) {
    case AddrMode::dn:
        return "d" + std::to_string(reg);
    case AddrMode::an:
        return "a" + std::to_string(reg);
    case AddrMode::ind:
        return "(a" + std::to_string(reg) + ")";
    case AddrMode::postInc:
        return "(a" + std::to_string(reg) + ")+";
    case AddrMode::preDec:
        return "-(a" + std::to_string(reg) + ")";
    case AddrMode::disp:
        return ref.toString() + "(a" + std::to_string(reg) + ")";
    case AddrMode::index:
        return ref.toString() + "(a" + std::to_string(reg) + "," +
               indexStr(indexReg, indexSize) + ")";
    case AddrMode::absW:
        return ref.toString() + ".w";
    case AddrMode::absL:
        return ref.toString();
    case AddrMode::pcDisp:
        return ref.toString() + "(pc)";
    case AddrMode::pcIndex:
        return ref.toString() + "(pc," + indexStr(indexReg, indexSize) + ")";
    case AddrMode::imm:
        return "#" + ref.toString();
    case AddrMode::regList:
        return regListStr(regMask);
    case AddrMode::sr:
        return "sr";
    case AddrMode::ccr:
        return "ccr";
    case AddrMode::usp:
        return "usp";
    }
    return "???";
}

void EffectiveAddr::validate() const {
    switch (mode) {
    case AddrMode::dn:
        if (reg < 0 || reg > 7) {
            throw Error{1, "data register d" + std::to_string(reg) + " out of range"};
        }
        break;
    case AddrMode::an:
    case AddrMode::ind:
    case AddrMode::postInc:
    case AddrMode::preDec:
    case AddrMode::disp:
        if (reg < 0 || reg > 7) {
            throw Error{1, "address register a" + std::to_string(reg) + " out of range"};
        }
        break;
    case AddrMode::index:
        if (reg < 0 || reg > 7) {
            throw Error{1, "address register a" + std::to_string(reg) + " out of range"};
        }
        if (indexReg < 0 || indexReg > 15) {
            throw Error{1, "index register " + std::to_string(indexReg) + " out of range"};
        }
        break;
    case AddrMode::pcIndex:
        if (indexReg < 0 || indexReg > 15) {
            throw Error{1, "index register " + std::to_string(indexReg) + " out of range"};
        }
        break;
    case AddrMode::regList:
        if (regMask == 0) {
            throw Error{1, "empty register list"};
        }
        break;
    default:
        break;
    }
}

void Instruction::validate() const {
    if (src) {
        src->validate();
    }
    if (dst) {
        dst->validate();
    }

    auto mn = mnemonicName(mnemonic);

    // moveq: #imm,Dn
    if (mnemonic == Mnemonic::moveq) {
        if (!src || src->mode != AddrMode::imm) {
            throw Error{1, std::string(mn) + " source must be immediate"};
        }
        if (!dst || dst->mode != AddrMode::dn) {
            throw Error{1, std::string(mn) + " destination must be data register"};
        }
        if (src->ref.value < -128 || src->ref.value > 127) {
            throw Error{1, std::string(mn) + " immediate " +
                std::to_string(src->ref.value) + " out of range (-128..127)"};
        }
    }

    // ext/swap: operand must be Dn
    if (mnemonic == Mnemonic::ext || mnemonic == Mnemonic::swap) {
        auto op = src ? &*src : (dst ? &*dst : nullptr);
        if (op && op->mode != AddrMode::dn) {
            throw Error{1, std::string(mn) + " operand must be data register"};
        }
    }

    // exg: both register-direct
    if (mnemonic == Mnemonic::exg) {
        if (src && !isRegDirect(src->mode)) {
            throw Error{1, "exg source must be a register"};
        }
        if (dst && !isRegDirect(dst->mode)) {
            throw Error{1, "exg destination must be a register"};
        }
    }

    // link: src must be An
    if (mnemonic == Mnemonic::link) {
        if (src && src->mode != AddrMode::an) {
            throw Error{1, "link first operand must be address register"};
        }
    }

    // unlk: operand must be An
    if (mnemonic == Mnemonic::unlk) {
        if (src && src->mode != AddrMode::an) {
            throw Error{1, "unlk operand must be address register"};
        }
    }

    // trap: operand must be imm 0-15
    if (mnemonic == Mnemonic::trap) {
        if (src && src->mode == AddrMode::imm) {
            if (src->ref.value < 0 || src->ref.value > 15) {
                throw Error{1, "trap vector " +
                    std::to_string(src->ref.value) + " out of range (0-15)"};
            }
        }
    }

    // Branch targets must be labels, not immediates
    if (isBranch(mnemonic) || mnemonic == Mnemonic::bsr) {
        if (src && src->mode == AddrMode::imm) {
            throw Error{1, std::string(mn) + " target must be a label, not immediate"};
        }
    }

    // DBcc: src=Dn, dst=label
    if (isDbcc(mnemonic)) {
        if (src && src->mode != AddrMode::dn) {
            throw Error{1, std::string(mn) + " first operand must be data register"};
        }
        if (dst && dst->mode == AddrMode::imm) {
            throw Error{1, std::string(mn) + " target must be a label, not immediate"};
        }
    }
}

std::string Instruction::toString() const {
    std::string r = mnemonicName(mnemonic);
    if (size) {
        switch (*size) {
        case OpSize::byte: r += ".b"; break;
        case OpSize::word: r += ".w"; break;
        case OpSize::long_: r += ".l"; break;
        }
    }
    if (src) {
        r += ' ';
        r += src->toString();
    }
    if (dst) {
        r += ',';
        r += dst->toString();
    }
    return r;
}
