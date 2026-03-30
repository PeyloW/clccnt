#include "parse.h"
#include "error.h"
#include "m68k.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace {

// --- String helpers ---

std::string toLower(std::string_view s) {
    std::string r(s);
    for (auto& c : r) { c = char(std::tolower((unsigned char)c)); }
    return r;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) { s.remove_prefix(1); }
    while (!s.empty() && std::isspace((unsigned char)s.back())) { s.remove_suffix(1); }
    return s;
}

// --- Mnemonic table ---

// Static hash map of all recognized mnemonic strings to enum values.
// Includes MIT/GAS aliases (jbsr→bsr, jne→bne, etc.) and dbra→dbf.
const std::unordered_map<std::string, Mnemonic>& mnemonicTable() {
    static const std::unordered_map<std::string, Mnemonic> t = {
        {"move", Mnemonic::move}, {"movea", Mnemonic::movea},
        {"moveq", Mnemonic::moveq}, {"movem", Mnemonic::movem},
        {"movep", Mnemonic::movep},
        {"lea", Mnemonic::lea}, {"pea", Mnemonic::pea},
        {"exg", Mnemonic::exg}, {"swap", Mnemonic::swap},
        {"link", Mnemonic::link}, {"unlk", Mnemonic::unlk},

        {"add", Mnemonic::add}, {"adda", Mnemonic::adda},
        {"addi", Mnemonic::addi}, {"addq", Mnemonic::addq},
        {"addx", Mnemonic::addx},
        {"sub", Mnemonic::sub}, {"suba", Mnemonic::suba},
        {"subi", Mnemonic::subi}, {"subq", Mnemonic::subq},
        {"subx", Mnemonic::subx},
        {"mulu", Mnemonic::mulu}, {"muls", Mnemonic::muls},
        {"divu", Mnemonic::divu}, {"divs", Mnemonic::divs},
        {"neg", Mnemonic::neg}, {"negx", Mnemonic::negx},
        {"clr", Mnemonic::clr}, {"ext", Mnemonic::ext},
        {"extb", Mnemonic::ext}, {"chk", Mnemonic::chk},
        {"cmp", Mnemonic::cmp}, {"cmpa", Mnemonic::cmpa},
        {"cmpi", Mnemonic::cmpi}, {"cmpm", Mnemonic::cmpm},
        {"tst", Mnemonic::tst},

        {"and", Mnemonic::and_}, {"andi", Mnemonic::andi},
        {"or", Mnemonic::or_}, {"ori", Mnemonic::ori},
        {"eor", Mnemonic::eor}, {"eori", Mnemonic::eori},
        {"not", Mnemonic::not_},

        {"asl", Mnemonic::asl}, {"asr", Mnemonic::asr},
        {"lsl", Mnemonic::lsl}, {"lsr", Mnemonic::lsr},
        {"rol", Mnemonic::rol}, {"ror", Mnemonic::ror},
        {"roxl", Mnemonic::roxl}, {"roxr", Mnemonic::roxr},

        {"btst", Mnemonic::btst}, {"bset", Mnemonic::bset},
        {"bclr", Mnemonic::bclr}, {"bchg", Mnemonic::bchg},

        {"bra", Mnemonic::bra}, {"bsr", Mnemonic::bsr},
        {"bhi", Mnemonic::bhi}, {"bls", Mnemonic::bls},
        {"bcc", Mnemonic::bcc}, {"bcs", Mnemonic::bcs},
        {"bne", Mnemonic::bne}, {"beq", Mnemonic::beq},
        {"bvc", Mnemonic::bvc}, {"bvs", Mnemonic::bvs},
        {"bpl", Mnemonic::bpl}, {"bmi", Mnemonic::bmi},
        {"bge", Mnemonic::bge}, {"blt", Mnemonic::blt},
        {"bgt", Mnemonic::bgt}, {"ble", Mnemonic::ble},

        {"dbt", Mnemonic::dbt}, {"dbf", Mnemonic::dbf},
        {"dbra", Mnemonic::dbf},
        {"dbhi", Mnemonic::dbhi}, {"dbls", Mnemonic::dbls},
        {"dbcc", Mnemonic::dbcc}, {"dbcs", Mnemonic::dbcs},
        {"dbne", Mnemonic::dbne}, {"dbeq", Mnemonic::dbeq},
        {"dbvc", Mnemonic::dbvc}, {"dbvs", Mnemonic::dbvs},
        {"dbpl", Mnemonic::dbpl}, {"dbmi", Mnemonic::dbmi},
        {"dbge", Mnemonic::dbge}, {"dblt", Mnemonic::dblt},
        {"dbgt", Mnemonic::dbgt}, {"dble", Mnemonic::dble},

        {"st", Mnemonic::st}, {"sf", Mnemonic::sf},
        {"shi", Mnemonic::shi}, {"sls", Mnemonic::sls},
        {"scc", Mnemonic::scc}, {"scs", Mnemonic::scs},
        {"sne", Mnemonic::sne}, {"seq", Mnemonic::seq},
        {"svc", Mnemonic::svc}, {"svs", Mnemonic::svs},
        {"spl", Mnemonic::spl}, {"smi", Mnemonic::smi},
        {"sge", Mnemonic::sge}, {"slt", Mnemonic::slt},
        {"sgt", Mnemonic::sgt}, {"sle", Mnemonic::sle},

        {"jmp", Mnemonic::jmp}, {"jsr", Mnemonic::jsr},

        {"rts", Mnemonic::rts}, {"rte", Mnemonic::rte},
        {"rtr", Mnemonic::rtr}, {"nop", Mnemonic::nop},
        {"reset", Mnemonic::reset}, {"stop", Mnemonic::stop},
        {"trap", Mnemonic::trap}, {"trapv", Mnemonic::trapv},
        {"illegal", Mnemonic::illegal}, {"tas", Mnemonic::tas},

        {"abcd", Mnemonic::abcd}, {"sbcd", Mnemonic::sbcd},
        {"nbcd", Mnemonic::nbcd},

        // Bit field (020+)
        {"bfchg", Mnemonic::bfchg}, {"bfclr", Mnemonic::bfclr},
        {"bfexts", Mnemonic::bfexts}, {"bfextu", Mnemonic::bfextu},
        {"bfffo", Mnemonic::bfffo}, {"bfins", Mnemonic::bfins},
        {"bfset", Mnemonic::bfset}, {"bftst", Mnemonic::bftst},

        // MIT/GAS aliases (jb* and short j* forms)
        {"jbsr", Mnemonic::bsr}, {"jbra", Mnemonic::bra},
        {"jra", Mnemonic::bra},
        {"jbeq", Mnemonic::beq}, {"jbne", Mnemonic::bne},
        {"jbcc", Mnemonic::bcc}, {"jbcs", Mnemonic::bcs},
        {"jbhi", Mnemonic::bhi}, {"jbls", Mnemonic::bls},
        {"jbge", Mnemonic::bge}, {"jbgt", Mnemonic::bgt},
        {"jble", Mnemonic::ble}, {"jblt", Mnemonic::blt},
        {"jbmi", Mnemonic::bmi}, {"jbpl", Mnemonic::bpl},
        {"jbvc", Mnemonic::bvc}, {"jbvs", Mnemonic::bvs},
        // GCC emits these short forms
        {"jeq", Mnemonic::beq}, {"jne", Mnemonic::bne},
        {"jcc", Mnemonic::bcc}, {"jcs", Mnemonic::bcs},
        {"jhi", Mnemonic::bhi}, {"jls", Mnemonic::bls},
        {"jge", Mnemonic::bge}, {"jgt", Mnemonic::bgt},
        {"jle", Mnemonic::ble}, {"jlt", Mnemonic::blt},
        {"jmi", Mnemonic::bmi}, {"jpl", Mnemonic::bpl},
        {"jvc", Mnemonic::bvc}, {"jvs", Mnemonic::bvs},
    };
    return t;
}

// --- Directive detection ---

// Static hash set of assembler directives to skip (Motorola, GAS, and vasm).
const std::unordered_set<std::string>& directiveSet() {
    static const std::unordered_set<std::string> s = {
        "dc", "ds", "dcb", "equ", "set", "org", "section", "include",
        "incbin", "incdir", "macro", "endm", "rept", "endr", "opt",
        "machine", "rsreset", "rs", "end", "cnop", "even", "odd",
        "align", "fail", "if", "else", "endif", "ifdef", "ifndef",
        "ifeq", "ifne", "ifgt", "ifge", "iflt", "ifle",
        // GAS
        ".text", ".data", ".bss", ".section", ".globl", ".global",
        ".extern", ".align", ".even", ".ascii", ".asciz", ".string",
        ".byte", ".word", ".short", ".long", ".quad", ".float", ".double",
        ".comm", ".lcomm", ".if", ".else", ".endif", ".ifdef", ".ifndef",
        ".include", ".file", ".size", ".type", ".skip", ".space",
        ".balign", ".p2align", ".set", ".equ", ".equiv", ".macro",
        ".endm", ".rept", ".endr", ".org", ".fill",
        // vasm
        "xdef", "xref",
    };
    return s;
}

bool isDirective(std::string_view word) {
    auto low = toLower(word);
    if (directiveSet().count(low)) return true;
    // Check with size suffix stripped: dc.b, ds.w, rs.l etc.
    auto dot = low.find('.');
    if (dot != std::string::npos) {
        auto base = low.substr(0, dot);
        if (directiveSet().count(base)) return true;
    }
    return false;
}

// --- Syntax normalization ---

// Strip % prefix from register names (GAS/MIT)
std::string stripRegPrefix(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 1 < s.size() &&
            (s[i+1] == 'd' || s[i+1] == 'a' || s[i+1] == 's' ||
             s[i+1] == 'p' || s[i+1] == 'f')) {
            continue; // skip %
        }
        result += s[i];
    }
    return result;
}

// Convert MIT @ notation to Motorola parenthesized form
// a0@     -> (a0)
// a0@+    -> (a0)+
// a0@-    -> -(a0)
// a0@(8)  -> 8(a0)
// a0@(8,d0:w) -> 8(a0,d0.w)
// pc@(lab) -> lab(pc)
std::string convertMitAt(std::string_view s) {
    auto at = s.find('@');
    if (at == std::string::npos) return std::string(s);

    auto base = s.substr(0, at);
    auto rest = s.substr(at + 1);

    if (rest.empty()) {
        return "(" + std::string(base) + ")";
    }
    if (rest == "+") {
        return "(" + std::string(base) + ")+";
    }
    if (rest == "-") {
        return "-(" + std::string(base) + ")";
    }
    // @(offset) or @(offset,index)
    if (rest.front() == '(' && rest.back() == ')') {
        auto inner = rest.substr(1, rest.size() - 2);
        auto comma = inner.find(',');
        if (comma == std::string::npos) {
            // @(offset) -> offset(base)
            return std::string(inner) + "(" + std::string(base) + ")";
        } else {
            // @(offset,index:size) -> offset(base,index.size)
            auto offset = inner.substr(0, comma);
            auto idx = inner.substr(comma + 1);
            return std::string(offset) + "(" + std::string(base) + "," +
                   std::string(idx) + ")";
        }
    }
    return std::string(s);
}

// Convert :w/:l size markers to .w/.l
std::string convertSizeMarker(std::string_view s) {
    std::string result(s);
    for (size_t i = 0; i + 1 < result.size(); i++) {
        if (result[i] == ':' && (result[i+1] == 'w' || result[i+1] == 'l' ||
                                  result[i+1] == 'W' || result[i+1] == 'L')) {
            result[i] = '.';
        }
    }
    return result;
}

// Replace fp with a6
std::string replaceFp(std::string_view s) {
    std::string result(s);
    for (size_t i = 0; i + 1 < result.size(); i++) {
        if ((result[i] == 'f' || result[i] == 'F') &&
            (result[i+1] == 'p' || result[i+1] == 'P')) {
            // Check it's not part of a longer identifier
            bool prevOk = (i == 0 || !std::isalnum((unsigned char)result[i-1]));
            bool nextOk = (i + 2 >= result.size() ||
                          !std::isalnum((unsigned char)result[i+2]));
            if (prevOk && nextOk) {
                result[i] = 'a';
                result[i+1] = '6';
            }
        }
    }
    return result;
}

// Chains all syntax normalizations: GAS % prefix, MIT @ notation, :w/:l markers, fp→a6.
std::string normalizeOperand(std::string_view s) {
    auto r = stripRegPrefix(s);
    r = convertMitAt(r);
    r = convertSizeMarker(r);
    r = replaceFp(r);
    return r;
}

// --- Value parsing ---

bool parseValue(std::string_view s, int64_t& out) {
    if (s.empty()) return false;

    // Hex: $xxxx or 0xXXXX
    if (s[0] == '$') {
        s.remove_prefix(1);
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out, 16);
        return ec == std::errc{};
    }
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.remove_prefix(2);
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out, 16);
        return ec == std::errc{};
    }

    // Decimal (possibly negative)
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{};
}

// --- Register parsing ---

bool isDataReg(std::string_view s, int& reg) {
    if (s.size() == 2 && (s[0] == 'd' || s[0] == 'D') &&
        s[1] >= '0' && s[1] <= '7') {
        reg = s[1] - '0';
        return true;
    }
    return false;
}

bool isAddrReg(std::string_view s, int& reg) {
    if (s.size() == 2 && (s[0] == 'a' || s[0] == 'A') &&
        s[1] >= '0' && s[1] <= '7') {
        reg = s[1] - '0';
        return true;
    }
    if (toLower(s) == "sp") { reg = 7; return true; }
    return false;
}

// Parse "d0.w", "a2.l", etc. into index register number (0-7 data, 8-15 addr)
// and index size. Returns false if the string is not a valid index register.
bool parseIndexReg(std::string_view s, int& xreg, OpSize& indexSize) {
    auto dot = s.find('.');
    std::string_view regPart = s, sizePart;
    if (dot != std::string_view::npos) {
        regPart = s.substr(0, dot);
        sizePart = s.substr(dot + 1);
    }
    if (isDataReg(regPart, xreg)) {}
    else if (isAddrReg(regPart, xreg)) { xreg += 8; }
    else return false;
    indexSize = (sizePart == "l") ? OpSize::long_ : OpSize::word;
    return true;
}

// --- Register list parsing (MOVEM) ---

// Parses a MOVEM register list like "d0-d3/a0-a2" into a 16-bit bitmask.
uint16_t parseRegList(std::string_view s) {
    uint16_t mask = 0;
    auto low = toLower(s);

    size_t pos = 0;
    while (pos < low.size()) {
        // Skip separators
        if (low[pos] == '/' || low[pos] == ' ') { pos++; continue; }

        // Parse register: d0-d7, a0-a7
        bool isA = false;
        if (pos + 1 < low.size() && (low[pos] == 'd' || low[pos] == 'a') &&
            low[pos+1] >= '0' && low[pos+1] <= '7') {
            isA = (low[pos] == 'a');
            int r1 = low[pos+1] - '0';
            pos += 2;

            // Check for range: d0-d3 or d0-3 (abbreviated)
            if (pos + 2 < low.size() && low[pos] == '-' &&
                low[pos+1] == (isA ? 'a' : 'd') &&
                low[pos+2] >= '0' && low[pos+2] <= '7') {
                int r2 = low[pos+2] - '0';
                pos += 3;
                int base = isA ? 8 : 0;
                for (int i = r1; i <= r2; i++) {
                    mask |= regBit(base + i);
                }
            } else if (pos + 1 < low.size() && low[pos] == '-' &&
                       low[pos+1] >= '0' && low[pos+1] <= '7') {
                int r2 = low[pos+1] - '0';
                pos += 2;
                int base = isA ? 8 : 0;
                for (int i = r1; i <= r2; i++) {
                    mask |= regBit(base + i);
                }
            } else {
                mask |= regBit((isA ? 8 : 0) + r1);
            }
        } else {
            pos++; // skip unknown
        }
    }
    return mask;
}

// Parses one operand string into an EffectiveAddr after syntax normalization.
// Handles all MC68k addressing modes including register lists (when isMovem is true).
EffectiveAddr parseOperand(std::string_view text,
                           bool isMovem = false) {
    auto s = trim(text);
    if (s.empty()) throw Error{1, "empty operand"};

    // Strip bitfield specification {offset:width} (020+ bit field ops)
    auto brace = s.find('{');
    if (brace != std::string_view::npos) { s = s.substr(0, brace); }

    auto low = toLower(s);
    EffectiveAddr ea;
    int reg;

    // Immediate: #xxx
    if (s[0] == '#') {
        ea.mode = AddrMode::imm;
        auto val = s.substr(1);
        parseValue(val, ea.ref.value);
        return ea;
    }

    // Pre-decrement: -(an)
    if (s.size() >= 4 && s[0] == '-' && s[1] == '(') {
        auto close = s.find(')');
        if (close != std::string::npos) {
            auto regStr = toLower(s.substr(2, close - 2));
            if (isAddrReg(regStr, reg)) {
                ea.mode = AddrMode::preDec;
                ea.reg = reg;
                return ea;
            }
        }
    }

    // Data register: d0-d7
    if (isDataReg(low, reg)) {
        ea.mode = AddrMode::dn;
        ea.reg = reg;
        return ea;
    }

    // Address register: a0-a7, sp
    if (isAddrReg(low, reg)) {
        ea.mode = AddrMode::an;
        ea.reg = reg;
        return ea;
    }

    // Special registers
    if (low == "sr") { ea.mode = AddrMode::sr; return ea; }
    if (low == "ccr") { ea.mode = AddrMode::ccr; return ea; }
    if (low == "usp") { ea.mode = AddrMode::usp; return ea; }

    // Register list (MOVEM)
    if (isMovem && (s.find('/') != std::string::npos ||
                    s.find('-') != std::string::npos)) {
        bool looksLikeRegList = false;
        if (low.size() >= 2 && (low[0] == 'd' || low[0] == 'a') &&
            low[1] >= '0' && low[1] <= '7') {
            looksLikeRegList = true;
        }
        if (looksLikeRegList) {
            ea.mode = AddrMode::regList;
            ea.regMask = parseRegList(s);
            return ea;
        }
    }

    // Post-increment: (an)+
    if (s.back() == '+') {
        auto close = s.rfind(')');
        if (close != std::string::npos && close + 1 < s.size()) {
            auto open = s.find('(');
            if (open != std::string::npos) {
                auto regStr = toLower(s.substr(open + 1, close - open - 1));
                if (isAddrReg(regStr, reg)) {
                    ea.mode = AddrMode::postInc;
                    ea.reg = reg;
                    return ea;
                }
            }
        }
    }

    // Parenthesized forms: (an), disp(an), disp(an,xn.s), disp(pc), disp(pc,xn.s)
    auto openParen = s.find('(');
    auto closeParen = s.rfind(')');
    if (openParen != std::string::npos && closeParen != std::string::npos &&
        closeParen > openParen) {
        auto prefix = s.substr(0, openParen);
        auto inner = s.substr(openParen + 1, closeParen - openParen - 1);
        auto innerLow = toLower(inner);

        // Parse displacement from prefix
        int64_t disp = 0;
        bool hasDisp = !prefix.empty();
        if (hasDisp && !parseValue(prefix, disp)) {
            ea.ref.label = std::string(prefix);
        }

        // Split inner by comma
        auto comma = innerLow.find(',');
        if (comma == std::string::npos) {
            // (an) or disp(an) or (pc) or disp(pc)
            auto regPart = trim(innerLow);
            if (regPart == "pc") {
                ea.mode = AddrMode::pcDisp;
                ea.ref.value = disp;
                return ea;
            }
            if (isAddrReg(regPart, reg)) {
                if (hasDisp) {
                    ea.mode = AddrMode::disp;
                    ea.reg = reg;
                    ea.ref.value = disp;
                } else {
                    ea.mode = AddrMode::ind;
                    ea.reg = reg;
                }
                return ea;
            }
        } else {
            // Standard: (base,index.sz) or disp(base,index.sz)
            // GAS alt:  (disp,base) or (disp,base,index.sz)
            std::string_view innerView(innerLow);
            auto first = trim(innerView.substr(0, comma));
            auto rest = trim(innerView.substr(comma + 1));

            bool firstIsPC = (first == "pc");
            bool firstIsReg = firstIsPC || isAddrReg(first, reg);

            if (firstIsReg) {
                // Standard: first is base register, rest is index.sz
                int xreg;
                OpSize xsize;
                if (!parseIndexReg(rest, xreg, xsize)) {
                    throw Error{1, "bad index register: " + std::string(rest)};
                }
                ea.mode = firstIsPC ? AddrMode::pcIndex : AddrMode::index;
                if (!firstIsPC) ea.reg = reg;
                ea.ref.value = disp;
                ea.indexReg = xreg;
                ea.indexSize = xsize;
            } else {
                // GAS: first is displacement, rest is base[,index.sz]
                int64_t innerDisp = 0;
                if (!parseValue(first, innerDisp)) {
                    ea.ref.label = std::string(first);
                }

                auto secondComma = rest.find(',');
                if (secondComma == std::string_view::npos) {
                    // (disp,base) -> d(An) or d(PC)
                    if (rest == "pc") {
                        ea.mode = AddrMode::pcDisp;
                    } else if (isAddrReg(rest, reg)) {
                        ea.mode = AddrMode::disp;
                        ea.reg = reg;
                    } else {
                        throw Error{1, "bad base register: " + std::string(rest)};
                    }
                    ea.ref.value = innerDisp;
                } else {
                    // (disp,base,index.sz) -> d(An,Xn) or d(PC,Xn)
                    auto basePart = trim(rest.substr(0, secondComma));
                    auto idxPart = trim(rest.substr(secondComma + 1));
                    int xreg;
                    OpSize xsize;
                    if (!parseIndexReg(idxPart, xreg, xsize)) {
                        throw Error{1, "bad index register: " + std::string(idxPart)};
                    }

                    if (basePart == "pc") {
                        ea.mode = AddrMode::pcIndex;
                    } else if (isAddrReg(basePart, reg)) {
                        ea.mode = AddrMode::index;
                        ea.reg = reg;
                    } else {
                        throw Error{1, "bad base register: " + std::string(basePart)};
                    }
                    ea.ref.value = innerDisp;
                    ea.indexReg = xreg;
                    ea.indexSize = xsize;
                }
            }
            return ea;
        }
    }

    // Absolute with size: xxx.w or xxx.l
    if (s.size() > 2) {
        auto dot = s.rfind('.');
        if (dot != std::string::npos && dot + 2 == s.size()) {
            auto suffix = std::tolower((unsigned char)s[dot + 1]);
            if (suffix == 'w' || suffix == 'l') {
                auto addr = s.substr(0, dot);
                if (!parseValue(addr, ea.ref.value)) {
                    ea.ref.label = std::string(addr);
                }
                ea.mode = (suffix == 'w') ? AddrMode::absW : AddrMode::absL;
                return ea;
            }
        }
    }

    // Default: treat as label/absolute long
    ea.mode = AddrMode::absL;
    if (!parseValue(s, ea.ref.value)) {
        ea.ref.label = std::string(s);
    }
    return ea;
}

// --- Split comma-separated operands (respecting parentheses) ---

std::vector<std::string_view> splitOperands(std::string_view s) {
    std::vector<std::string_view> result;
    int depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '(') { depth++; }
        else if (s[i] == ')') { depth--; }
        else if (s[i] == ',' && depth == 0) {
            result.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < s.size()) {
        result.push_back(s.substr(start));
    }

    return result;
}

} // anonymous namespace

// --- Public API ---

bool isLocalLabel(std::string_view label) {
    if (label.empty()) return false;
    if (label[0] == '.') return true;
    // GAS local labels: purely numeric or starting with .L
    if (label.size() >= 2 && label[0] == '.' && label[1] == 'L') return true;
    // Numeric labels (GAS)
    bool allDigits = true;
    for (char c : label) {
        if (!std::isdigit((unsigned char)c)) { allDigits = false; break; }
    }
    return allDigits;
}

Instruction parseInstruction(std::string_view text) {
    auto s = trim(text);
    if (s.empty()) throw Error{1, "empty instruction"};

    Instruction inst;

    // Split mnemonic from operands at first whitespace
    size_t split = 0;
    while (split < s.size() && !std::isspace((unsigned char)s[split])) { split++; }
    auto mnStr = toLower(s.substr(0, split));
    auto opStr = trim(s.substr(split));

    // Extract size suffix: .b, .w, .l, .s
    std::optional<OpSize> size;
    bool isByteDisp = false;
    if (mnStr.size() > 2 && mnStr[mnStr.size() - 2] == '.') {
        char sz = mnStr.back();
        if (sz == 'b') { size = OpSize::byte; }
        else if (sz == 'w') { size = OpSize::word; }
        else if (sz == 'l') { size = OpSize::long_; }
        else if (sz == 's') { isByteDisp = true; } // .s = short branch
        if (sz == 'b' || sz == 'w' || sz == 'l' || sz == 's') {
            mnStr.resize(mnStr.size() - 2);
        }
    }

    // Look up mnemonic
    auto& table = mnemonicTable();
    auto it = table.find(mnStr);

    // Try MIT suffix: movl -> move + .l
    if (it == table.end() && mnStr.size() > 1) {
        char last = mnStr.back();
        if (last == 'b' || last == 'w' || last == 'l') {
            auto base = mnStr.substr(0, mnStr.size() - 1);
            it = table.find(base);
            if (it != table.end() && !size) {
                if (last == 'b') { size = OpSize::byte; }
                else if (last == 'w') { size = OpSize::word; }
                else if (last == 'l') { size = OpSize::long_; }
            }
        }
    }

    if (it == table.end()) {
        throw Error{1, "unknown mnemonic: " + mnStr};
    }

    inst.mnemonic = it->second;
    inst.size = size;

    // For .s branches, treat size as byte for displacement calculation
    if (isByteDisp && (isBranch(inst.mnemonic) || isDbcc(inst.mnemonic))) {
        inst.size = OpSize::byte;
    }

    // Parse operands
    bool isMovemInst = (inst.mnemonic == Mnemonic::movem);
    if (!opStr.empty()) {
        // Normalize all operands
        auto normalized = normalizeOperand(opStr);
        auto parts = splitOperands(normalized);

        if (parts.size() >= 1) {
            inst.src = parseOperand(parts[0], isMovemInst);
        }
        if (parts.size() >= 2) {
            inst.dst = parseOperand(parts[1], isMovemInst);
        }
    }

    // Auto-promotion
    if (inst.dst) {
        auto dstMode = inst.dst->mode;
        // MOVE to/from special registers
        if (inst.mnemonic == Mnemonic::move) {
            if (dstMode == AddrMode::sr) {
                inst.mnemonic = Mnemonic::moveToSR;
            } else if (dstMode == AddrMode::ccr) {
                inst.mnemonic = Mnemonic::moveToCCR;
            } else if (dstMode == AddrMode::usp || (inst.src && inst.src->mode == AddrMode::usp)) {
                inst.mnemonic = Mnemonic::moveUSP;
            } else if (inst.src && inst.src->mode == AddrMode::sr) {
                inst.mnemonic = Mnemonic::moveFromSR;
            } else if (dstMode == AddrMode::an) {
                inst.mnemonic = Mnemonic::movea;
            }
        }
        // ADD/SUB/CMP to An -> ADDA/SUBA/CMPA
        if (dstMode == AddrMode::an) {
            if (inst.mnemonic == Mnemonic::add) { inst.mnemonic = Mnemonic::adda; }
            else if (inst.mnemonic == Mnemonic::sub) { inst.mnemonic = Mnemonic::suba; }
            else if (inst.mnemonic == Mnemonic::cmp) { inst.mnemonic = Mnemonic::cmpa; }
        }
    }

    inst.validate();
    return inst;
}

std::vector<SourceLine> parseSource(std::string_view text) {
    std::vector<SourceLine> lines;
    std::vector<std::string> pendingLabels;

    int lineNum = 0;
    size_t pos = 0;

    while (pos <= text.size()) {
        // Find end of line
        auto eol = text.find('\n', pos);
        if (eol == std::string::npos) { eol = text.size(); }
        auto line = text.substr(pos, eol - pos);
        pos = eol + 1;
        lineNum++;

        // Strip trailing CR
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        auto raw = std::string(line);

        // Skip whole-line comments
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '*' || trimmed[0] == '#') continue;

        // Strip inline comments: ; and |
        std::string working(trimmed);
        bool inQuote = false;
        for (size_t i = 0; i < working.size(); i++) {
            if (working[i] == '\'' || working[i] == '"') {
                inQuote = !inQuote;
            }
            if (!inQuote && (working[i] == ';' || working[i] == '|')) {
                working.resize(i);
                break;
            }
        }
        auto content = trim(working);
        if (content.empty()) continue;

        // Check for label: identifier followed by :
        // Labels can be at column 0 without colon in Motorola syntax
        std::string_view rest = content;

        // Label at start of line (possibly with colon)
        while (!rest.empty()) {
            // Check if this looks like a label
            size_t labelEnd = 0;
            if (rest[0] == '.') { labelEnd++; } // local label prefix
            while (labelEnd < rest.size() &&
                   (std::isalnum((unsigned char)rest[labelEnd]) || rest[labelEnd] == '_')) {
                labelEnd++;
            }

            if (labelEnd > 0 && labelEnd < rest.size() && rest[labelEnd] == ':') {
                pendingLabels.push_back(std::string(rest.substr(0, labelEnd)));
                rest = trim(rest.substr(labelEnd + 1));
                continue;
            }

            // Motorola: label in column 0 without colon
            if (!std::isspace((unsigned char)line[0]) && labelEnd > 0) {
                auto candidate = rest.substr(0, labelEnd);
                auto low = toLower(candidate);
                // Make sure it's not a mnemonic
                if (mnemonicTable().find(low) == mnemonicTable().end() &&
                    !isDirective(low)) {
                    pendingLabels.push_back(std::string(candidate));
                    rest = trim(rest.substr(labelEnd));
                    // Skip optional colon
                    if (!rest.empty() && rest[0] == ':') {
                        rest = trim(rest.substr(1));
                    }
                    continue;
                }
            }
            break;
        }

        if (rest.empty()) continue;

        // Check for directive
        size_t wordEnd = 0;
        while (wordEnd < rest.size() && !std::isspace((unsigned char)rest[wordEnd])) {
            wordEnd++;
        }
        if (isDirective(rest.substr(0, wordEnd))) continue;

        // Try to parse as instruction
        try {
            auto inst = parseInstruction(rest);
            SourceLine sl;
            sl.lineNum = lineNum;
            sl.raw = raw;
            sl.labels = std::move(pendingLabels);
            pendingLabels.clear();
            sl.inst = std::move(inst);
            lines.push_back(std::move(sl));
        } catch (const Error&) {
            // Skip unparseable lines (could be data or unsupported)
        }
    }

    return lines;
}
