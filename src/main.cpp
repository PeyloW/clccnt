#include "analysis.h"
#include "error.h"
#include "m68k.h"
#include "parse.h"
#include "timing.h"
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

bool verbose = false;

std::string readFile(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "error: cannot open '%s'\n", path);
        std::exit(1);
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string buf(sz, '\0');
    std::fread(buf.data(), 1, sz, f);
    std::fclose(f);
    return buf;
}

std::string readStdin() {
    std::string buf;
    char chunk[4096];
    while (size_t n = std::fread(chunk, 1, sizeof(chunk), stdin))
        buf.append(chunk, n);
    return buf;
}

void printHelp() {
    std::printf(
        "Usage: clccnt [options] [file.s]\n"
        "\n"
        "MC68k Clock Cycle Counter\n"
        "\n"
        "Options:\n"
        "  -i INST   Count cycles for a single instruction\n"
        "  -c CPU    CPU model: 000 010 020 030 040 060 (default: 000)\n"
        "  -v        Verbose: show blocks and per-instruction detail\n"
        "  -b N      Bus cycles for rounding (default: from CPU)\n"
        "  -w N      Bus width in bytes (default: from CPU)\n"
        "  -e F      Estimate factor 0.0-1.0 for variable-timing instructions (default: 0.5)\n"
        "  -n N      Max loop iterations for path analysis (default: 4)\n"
        "  -h        Show this help\n"
    );
}

// --- Output formatting ---

std::string formatRange(const std::vector<int>& blocks,
                        const std::vector<LoopAnnotation>& loops,
                        int from, int to, int skipIdx = -1) {
    std::string result;
    bool first = true;
    int i = from;
    while (i <= to) {
        if (!first) result += ">";
        first = false;

        const LoopAnnotation* loop = nullptr;
        int loopIdx = -1;
        for (int li = 0; li < (int)loops.size(); li++) {
            if (li == skipIdx) continue;
            if (loops[li].startIdx == i && loops[li].endIdx <= to) {
                // Prefer the widest span so nested loops render inside
                if (!loop || loops[li].endIdx > loop->endIdx) {
                    loop = &loops[li];
                    loopIdx = li;
                }
            }
        }

        if (loop) {
            result += "(";
            result += formatRange(blocks, loops, loop->startIdx, loop->endIdx, loopIdx);
            result += ")*" + std::to_string(loop->count);
            i = loop->endIdx + 1;
        } else {
            result += std::to_string(blocks[i]);
            i++;
        }
    }
    return result;
}

std::string formatPath(const PathResult& p) {
    if (p.loops.empty()) {
        std::string s;
        for (size_t i = 0; i < p.blocks.size(); i++) {
            if (i > 0) s += ">";
            s += std::to_string(p.blocks[i]);
        }
        return s;
    }
    return formatRange(p.blocks, p.loops, 0, (int)p.blocks.size() - 1);
}

void printSingleInstruction(const Instruction& inst, TimingBase& timing) {
    auto t = timing.time(inst);
    if (!isCondBranch(inst.mnemonic) && !isDbcc(inst.mnemonic)) {
        t = estimateTiming(t);
    }
    auto s = inst.toString();

    if (t.a == t.b) {
        std::printf("  %-40s %d cycles\n", s.c_str(), t.a);
    } else {
        std::printf("  %-40s %d-%d cycles\n", s.c_str(), t.min(), t.max());
    }
}

void printVerbose(const FunctionResult& fr) {
    std::printf("function %s:\n", fr.name.c_str());

    // Compute loop depth for each block from back-edges
    std::vector<int> loopDepth(fr.blocks.size(), 0);
    for (auto& b : fr.blocks) {
        for (auto& e : b.successors) {
            if (e.target <= b.id) {
                for (int i = e.target; i <= b.id; i++) {
                    loopDepth[i]++;
                }
            }
        }
    }

    for (auto& b : fr.blocks) {
        int ind = 2 * loopDepth[b.id];
        if (!b.label.empty()) {
            std::printf("  %*sblock %d: %s\n", ind, "", b.id, b.label.c_str());
        } else {
            std::printf("  %*sblock %d:\n", ind, "", b.id);
        }

        for (int i = b.firstLine; i <= b.lastLine; i++) {
            if (i < 0 || i >= (int)fr.lines.size()) continue;
            auto& sl = fr.lines[i];
            if (!sl.inst) continue;

            auto& t = sl.timing;
            auto m = sl.inst->mnemonic;
            auto instStr = sl.inst->toString();
            bool isLast = (i == b.lastLine);

            int pad = std::max(1, 36 - ind);
            const char* annot = sl.paired ? "^" : sl.stalled ? "+" : " ";
            if (isCondBranch(m) || isDbcc(m)) {
                std::printf("  %4d: %*s%-*s %4d%s %4d",
                    sl.lineNum, ind, "", pad, instStr.c_str(),
                    t.b, annot, t.a);
            } else if (t.a != t.b) {
                std::printf("  %4d: %*s%-*s %4d%s %4d",
                    sl.lineNum, ind, "", pad, instStr.c_str(),
                    t.min(), annot, t.max());
            } else {
                std::printf("  %4d: %*s%-*s %4d%s     ",
                    sl.lineNum, ind, "", pad, instStr.c_str(),
                    t.a, annot);
            }

            if (isLast) {
                // Block total: body (with pairing) + terminator
                Timing bt = b.body;
                if (isBlockTerminator(m)) {
                    bt += t;
                }
                if (bt.a != bt.b) {
                    std::printf("  = %5d %5d", bt.min(), bt.max());
                } else {
                    std::printf("  = %5d", bt.a);
                }
            }
            std::printf("\n");
        }
    }

    // Print paths sorted by ascending cost
    std::vector<size_t> order(fr.paths.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return fr.paths[a].cycles.max() < fr.paths[b].cycles.max();
    });
    for (auto idx : order) {
        auto& p = fr.paths[idx];
        auto label = "path " + formatPath(p) + ":";
        if (p.cycles.a == p.cycles.b) {
            std::printf("  %54s = %5d\n", label.c_str(), p.cycles.a);
        } else {
            std::printf("  %54s = %5d %5d\n", label.c_str(),
                p.cycles.min(), p.cycles.max());
        }
    }
}

void printSummary(const FunctionResult& fr) {
    if (fr.minCycles == fr.maxCycles) {
        std::printf("  %-40s %d\n", fr.name.c_str(), fr.minCycles);
    } else {
        std::printf("  %-40s %d-%d\n", fr.name.c_str(), fr.minCycles, fr.maxCycles);
    }
}

} // anonymous namespace

int main(int argc, char** argv) {
    const char* singleInst = nullptr;
    const char* cpuName = "000";
    int loopCount = 4;
    bool busOverride = false;
    bool widthOverride = false;
    int busVal = 0, widthVal = 0;
    const char* inputFile = nullptr;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printHelp();
            return 0;
        }
        if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
            continue;
        }
        if (std::strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            singleInst = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cpuName = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            busVal = std::atoi(argv[++i]);
            busOverride = true;
            continue;
        }
        if (std::strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            widthVal = std::atoi(argv[++i]);
            widthOverride = true;
            continue;
        }
        if (std::strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            char* end;
            double val = std::strtod(argv[++i], &end);
            if (*end != '\0' || val < 0.0 || val > 1.0) {
                std::fprintf(stderr, "error: estimate factor (-e) must be 0.0-1.0\n");
                return 1;
            }
            timingEstimate = val;
            continue;
        }
        if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            loopCount = std::atoi(argv[++i]);
            if (loopCount < 0) {
                std::fprintf(stderr, "error: loop count (-n) must be non-negative\n");
                return 1;
            }
            continue;
        }
        // Positional argument: input file
        inputFile = argv[i];
    }

    try {
        // Create timing engine
        auto timing = createTiming(cpuName);
        if (!timing) {
            std::fprintf(stderr, "error: unknown CPU '%s'\n", cpuName);
            return 1;
        }

        // Apply bus parameters
        busCycles = busOverride ? busVal : timing->busAccessCycles();
        busWidthBytes = widthOverride ? widthVal : timing->busWidth();

        if (busCycles <= 0) {
            std::fprintf(stderr, "error: bus cycles (-b) must be positive\n");
            return 1;
        }
        if (busWidthBytes <= 0) {
            std::fprintf(stderr, "error: bus width (-w) must be positive\n");
            return 1;
        }

        // Single instruction mode
        if (singleInst) {
            auto inst = parseInstruction(singleInst);
            printSingleInstruction(inst, *timing);
            return 0;
        }

        // File mode
        std::string source;
        if (inputFile) {
            source = readFile(inputFile);
        } else {
            source = readStdin();
        }

        auto lines = parseSource(source);
        if (lines.empty()) {
            std::fprintf(stderr, "error: no instructions found\n");
            return 1;
        }

        auto funcs = analyzeSource(lines, *timing, loopCount);

        for (auto& fr : funcs) {
            if (verbose) {
                printVerbose(fr);
            } else {
                printSummary(fr);
            }
        }

        return 0;
    } catch (const Error& e) {
        std::fprintf(stderr, "error: %s\n", e.message.c_str());
        return e.code;
    }
}
