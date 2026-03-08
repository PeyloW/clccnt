#include "analysis.h"
#include "error.h"
#include "m68k.h"
#include <algorithm>
#include <climits>
#include <unordered_map>

namespace {

// --- Function splitting ---

// Pre-analysis container for a function's name and source lines before CFG construction.
struct RawFunction {
    std::string name;
    std::vector<SourceLine> lines;
};

// Splits source lines into functions at each non-local label.
std::vector<RawFunction> splitFunctions(std::vector<SourceLine>& lines) {
    std::vector<RawFunction> funcs;
    RawFunction* cur = nullptr;

    for (auto& sl : lines) {
        // Check for non-local labels -> new function
        for (auto& label : sl.labels) {
            if (!isLocalLabel(label)) {
                funcs.push_back({label, {}});
                cur = &funcs.back();
            }
        }
        if (!cur) {
            funcs.push_back({"<unnamed>", {}});
            cur = &funcs.back();
        }
        cur->lines.push_back(std::move(sl));
    }

    return funcs;
}

// --- Basic block construction ---

// Creates basic blocks by splitting at labels and after block terminators.
std::vector<BasicBlock> buildBlocks(const std::vector<SourceLine>& lines) {
    std::vector<BasicBlock> blocks;

    // Identify block starts: first instruction, labeled instructions,
    // instructions after terminators
    std::vector<bool> isStart(lines.size(), false);
    if (!lines.empty()) {
        isStart[0] = true;
    }

    for (int i = 0; i < (int)lines.size(); i++) {
        // Any labeled instruction starts a new block
        if (!lines[i].labels.empty() && i > 0) {
            isStart[i] = true;
        }

        // Instruction after a terminator starts a new block
        if (lines[i].inst && isBlockTerminator(lines[i].inst->mnemonic)) {
            if (i + 1 < (int)lines.size()) {
                isStart[i + 1] = true;
            }
        }
    }

    // Create blocks
    int blockId = 0;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (isStart[i]) {
            BasicBlock b;
            b.id = blockId++;
            b.firstLine = i;
            // Find label for this block
            if (!lines[i].labels.empty()) {
                b.label = lines[i].labels[0];
            }
            blocks.push_back(b);
        }
    }

    // Set lastLine for each block
    for (int i = 0; i < (int)blocks.size(); i++) {
        if (i + 1 < (int)blocks.size()) {
            blocks[i].lastLine = blocks[i + 1].firstLine - 1;
        } else {
            blocks[i].lastLine = (int)lines.size() - 1;
        }
    }

    for (auto& b : blocks) {
        if (b.firstLine < 0 || b.lastLine < b.firstLine ||
            b.lastLine >= (int)lines.size()) {
            throw Error{2, "internal: block " + std::to_string(b.id) +
                " has invalid line range [" + std::to_string(b.firstLine) +
                "," + std::to_string(b.lastLine) + "]"};
        }
    }

    return blocks;
}

// --- Edge construction ---

// Extracts the branch target label from an instruction.
// Branches store target in src; DBcc stores it in dst (dbf d0,label).
const std::string& getBranchTarget(const Instruction& inst) {
    static const std::string empty;
    // Branch target is in src operand (the label)
    if (inst.src && inst.src->mode == AddrMode::absL) {
        return inst.src->ref.label;
    }
    // DBcc: target is in dst operand (dbf d0,label)
    // Actually: DBcc Dn,label -> src=Dn, dst=label
    if (inst.dst && inst.dst->mode == AddrMode::absL) {
        return inst.dst->ref.label;
    }
    return empty;
}

// Constructs CFG edges from branch/jump targets in each block's terminator.
// For computed JMPs with unresolved targets, adds edges to orphan labeled blocks.
void buildEdges(std::vector<BasicBlock>& blocks,
                const std::vector<SourceLine>& lines) {
    // Build label -> block index map
    std::unordered_map<std::string_view, int> labelToBlock;
    for (auto& b : blocks) {
        if (!b.label.empty()) {
            labelToBlock[b.label] = b.id;
        }
        // Also map all labels on the first line
        if (b.firstLine < (int)lines.size()) {
            for (auto& l : lines[b.firstLine].labels) {
                labelToBlock[l] = b.id;
            }
        }
    }

    for (auto& b : blocks) {
        int lastIdx = b.lastLine;
        if (lastIdx < 0 || lastIdx >= (int)lines.size()) continue;
        auto& lastLine = lines[lastIdx];
        if (!lastLine.inst) continue;

        auto& inst = *lastLine.inst;
        auto m = inst.mnemonic;

        if (isExit(m)) {
            b.isExit = true;
            continue;
        }

        auto t = lastLine.timing;

        if (isCondBranch(m) || isDbcc(m)) {
            // Taken/loop-back edge (min = taken cost)
            auto& target = getBranchTarget(inst);
            auto it = labelToBlock.find(target);
            if (it != labelToBlock.end()) {
                b.successors.push_back({it->second, t.a});
            }
            // Fall-through edge (max = not-taken cost)
            if (b.id + 1 < (int)blocks.size()) {
                b.successors.push_back({b.id + 1, t.b});
            }
        } else if (m == Mnemonic::bra) {
            auto& target = getBranchTarget(inst);
            auto it = labelToBlock.find(target);
            if (it != labelToBlock.end()) {
                b.successors.push_back({it->second, t.a});
            } else {
                // External target (tail call): treat as exit
                b.isExit = true;
            }
        } else if (m == Mnemonic::jmp) {
            auto& target = getBranchTarget(inst);
            auto it = labelToBlock.find(target);
            if (it != labelToBlock.end()) {
                b.successors.push_back({it->second, t.a});
            } else if (!target.empty()) {
                // External target (tail call): treat as exit
                b.isExit = true;
            } else {
                // Computed jump: add edges to orphan labeled blocks
                for (auto& ob : blocks) {
                    if (ob.id > b.id && !ob.label.empty()) {
                        bool isOrphan = true;
                        for (auto& pb : blocks) {
                            for (auto& e : pb.successors) {
                                if (e.target == ob.id) { isOrphan = false; break; }
                            }
                            if (!isOrphan) break;
                        }
                        if (isOrphan) {
                            b.successors.push_back({ob.id, t.a});
                        }
                    }
                }
            }
        } else {
            // Fall-through (non-terminator at end of block)
            if (b.id + 1 < (int)blocks.size()) {
                b.successors.push_back({b.id + 1, 0});
            }
        }
    }
}

// --- Body cost computation ---

// Sums instruction timing for a basic block, excluding the terminator.
// Terminator cost is carried on edges so the DFS can distinguish taken/not-taken.
void computeBodyCost(BasicBlock& b, const std::vector<SourceLine>& lines) {
    b.body.a = 0;
    b.body.b = 0;

    for (int i = b.firstLine; i <= b.lastLine; i++) {
        if (i < 0 || i >= (int)lines.size()) continue;
        if (!lines[i].inst) continue;

        // Terminator cost goes into edges, not body
        if (i == b.lastLine && isBlockTerminator(lines[i].inst->mnemonic)) {
            continue;
        }

        auto t = lines[i].timing;
        b.body.a += t.a;
        b.body.b += t.b;
    }

    if (b.body.a < 0 || b.body.b < 0) {
        throw Error{2, "internal: block " + std::to_string(b.id) +
            " has negative body cost min=" + std::to_string(b.body.a) +
            " max=" + std::to_string(b.body.b)};
    }
}

// --- Path enumeration (DFS) ---

// Mutable state for DFS path enumeration, shared across recursive calls.
struct DFSState {
    const std::vector<BasicBlock>& blocks;
    const std::vector<SourceLine>& lines;
    int loopCount;
    std::vector<PathResult>& results;
    std::vector<int> path;
    std::vector<bool> visited;
    std::vector<LoopAnnotation> loops;
    std::vector<int> cycleGuard; // blocks in active cycles (counter for nesting)
};

// Recursively enumerates execution paths through the CFG.
// Back-edges are treated as loops: the body cost is multiplied by loopCount,
// then exit edges from cycle blocks are followed to continue the path.
void dfs(DFSState& state, int blockId, Timing cost) {
    if (blockId < 0 || blockId >= (int)state.blocks.size()) {
        throw Error{2, "internal: dfs blockId " + std::to_string(blockId) + " out of range"};
    }
    auto& b = state.blocks[blockId];
    state.path.push_back(blockId);
    state.visited[blockId] = true;

    Timing cur = cost + b.body;

    if (b.isExit) {
        // Add exit instruction cost
        int lastIdx = b.lastLine;
        if (lastIdx >= 0 && lastIdx < (int)state.lines.size() &&
            state.lines[lastIdx].inst) {
            cur += state.lines[lastIdx].timing;
        }
        state.results.push_back({state.path, state.loops, cur});
    } else {
        bool hasForwardEdge = false;
        size_t loopsBefore = state.loops.size();

        for (auto& edge : b.successors) {
            int target = edge.target;
            if (target < 0 || target >= (int)state.blocks.size()) continue;

            // Back-edge (loop): target is on the current DFS path
            if (state.visited[target]) {
                // Skip phantom back-edges to blocks in an active cycle
                if (state.cycleGuard[target] > 0) continue;
                if (state.loopCount > 0) {
                    // Find the target in the DFS path to identify the cycle
                    int targetPathIdx = -1;
                    for (int pi = (int)state.path.size() - 1; pi >= 0; pi--) {
                        if (state.path[pi] == target) {
                            targetPathIdx = pi;
                            break;
                        }
                    }

                    if (targetPathIdx >= 0) {
                        // Compute loop body cost by walking the actual DFS path
                        Timing loopBody;
                        for (int pi = targetPathIdx; pi < (int)state.path.size(); pi++) {
                            int bi = state.path[pi];
                            loopBody += state.blocks[bi].body;
                            if (pi + 1 < (int)state.path.size()) {
                                int nextBi = state.path[pi + 1];
                                for (auto& e : state.blocks[bi].successors) {
                                    if (e.target == nextBi) {
                                        loopBody += {e.cost, e.cost};
                                        break;
                                    }
                                }
                            }
                        }

                        int backEdgeCost = edge.cost;

                        int extraIters = std::max(0, state.loopCount - 1);
                        if (extraIters > 0) {
                            cur += (loopBody + backEdgeCost) * extraIters;
                        }

                        // Record loop annotation for display
                        state.loops.push_back({targetPathIdx,
                            (int)state.path.size() - 1, state.loopCount});

                        // Find exit paths from cycle blocks
                        std::vector<bool> inCycle(state.blocks.size(), false);
                        for (int pi = targetPathIdx; pi < (int)state.path.size(); pi++) {
                            inCycle[state.path[pi]] = true;
                            // Guard non-header cycle blocks to prevent phantom
                            // back-edges, but leave the header (back-edge target)
                            // accessible for legitimate outer loop detection
                            if (pi != targetPathIdx) {
                                state.cycleGuard[state.path[pi]]++;
                            }
                        }

                        for (int pi = targetPathIdx; pi < (int)state.path.size() - 1; pi++) {
                            int cycleBlockId = state.path[pi];
                            auto& cb = state.blocks[cycleBlockId];

                            for (auto& exitEdge : cb.successors) {
                                int exitTarget = exitEdge.target;
                                if (exitTarget < 0 || exitTarget >= (int)state.blocks.size()) continue;
                                if (inCycle[exitTarget] || state.visited[exitTarget]) continue;

                                hasForwardEdge = true;

                                // Cost to travel through back-edge and cycle to exit
                                Timing travel = {backEdgeCost, backEdgeCost};
                                for (int pj = targetPathIdx; ; pj++) {
                                    int bi = state.path[pj];
                                    travel += state.blocks[bi].body;
                                    if (bi == cycleBlockId) break;
                                    if (pj + 1 < (int)state.path.size()) {
                                        int nextBi = state.path[pj + 1];
                                        for (auto& e : state.blocks[bi].successors) {
                                            if (e.target == nextBi) {
                                                travel += {e.cost, e.cost};
                                                break;
                                            }
                                        }
                                    }
                                }

                                dfs(state, exitTarget,
                                    cur + travel + exitEdge.cost);
                            }
                        }

                        // Release cycle guard
                        for (int pi = targetPathIdx; pi < (int)state.path.size(); pi++) {
                            if (pi != targetPathIdx) {
                                state.cycleGuard[state.path[pi]]--;
                            }
                        }
                    }
                }
                continue;
            }

            hasForwardEdge = true;
            dfs(state, target, cur + edge.cost);
        }

        // Dead end or infinite loop (no forward edges found)
        if (!hasForwardEdge) {
            state.results.push_back({state.path, state.loops, cur});
        }

        state.loops.resize(loopsBefore);
    }

    state.path.pop_back();
    state.visited[blockId] = false;
}

// --- Branch size fixup ---
// Detect which branches can use byte displacement based on distance to target

// Determines byte vs word branch displacement after block layout.
// Branches without explicit size are checked against their target distance;
// byte displacement is used when the offset fits in -128..+127 (excluding 0 and -1).
void fixupBranchSizes(std::vector<SourceLine>& lines, TimingBase& timing) {
    // Build label-to-line-index map
    std::unordered_map<std::string_view, int> labelToLine;
    for (int i = 0; i < (int)lines.size(); i++) {
        for (auto& l : lines[i].labels) {
            labelToLine[l] = i;
        }
    }

    // Compute byte offsets (assume word branches initially)
    int offset = 0;
    for (auto& sl : lines) {
        sl.byteOffset = offset;
        if (sl.inst) {
            sl.instSize = computeInstSize(*sl.inst);
            offset += sl.instSize;
        }
    }

    // Check each branch for byte displacement
    for (auto& sl : lines) {
        if (!sl.inst) continue;
        auto& inst = *sl.inst;

        // Only fix branches without explicit size
        if (inst.size) continue;
        if (!isBranch(inst.mnemonic) && inst.mnemonic != Mnemonic::bsr) continue;

        // Get target label
        auto& target = getBranchTarget(inst);
        if (target.empty()) continue;

        auto it = labelToLine.find(target);
        if (it == labelToLine.end()) continue;

        int targetOffset = lines[it->second].byteOffset;
        int branchPC = sl.byteOffset + 2; // PC points to extension word
        int disp = targetOffset - branchPC;

        // Byte displacement: -128..+127, but 0 and -1 are reserved
        if (disp >= -128 && disp <= 127 && disp != 0 && disp != -1) {
            inst.size = OpSize::byte;
            sl.instSize = computeInstSize(inst);
        }
    }

    // Recompute byte offsets with corrected sizes
    offset = 0;
    for (auto& sl : lines) {
        sl.byteOffset = offset;
        offset += sl.instSize;
    }

    // Recompute timing with corrected sizes
    for (auto& sl : lines) {
        if (sl.inst) {
            sl.timing = timing.time(*sl.inst);
            // Collapse variable-timing instructions to a single estimate.
            // Exclude branches: their min/max represents taken vs not-taken paths.
            if (!isCondBranch(sl.inst->mnemonic) && !isDbcc(sl.inst->mnemonic)) {
                sl.timing = estimateTiming(sl.timing);
            }
        }
    }
}

} // anonymous namespace

// --- Public API ---

std::vector<FunctionResult> analyzeSource(std::vector<SourceLine>& lines,
                                           TimingBase& timing, int loopCount) {
    // Split into functions
    auto rawFuncs = splitFunctions(lines);

    std::vector<FunctionResult> results;

    for (auto& rf : rawFuncs) {
        FunctionResult fr;
        fr.name = rf.name;
        fr.lines = std::move(rf.lines);

        // Fix up branch sizes within the function
        fixupBranchSizes(fr.lines, timing);

        // Build blocks
        fr.blocks = buildBlocks(fr.lines);

        // Compute body costs
        for (auto& b : fr.blocks) {
            computeBodyCost(b, fr.lines);
        }

        // Build edges
        buildEdges(fr.blocks, fr.lines);

        // Enumerate paths via DFS
        if (!fr.blocks.empty()) {
            DFSState state{fr.blocks, fr.lines, loopCount, fr.paths, {}, {}, {}, {}};
            state.visited.resize(fr.blocks.size(), false);
            state.cycleGuard.resize(fr.blocks.size(), 0);
            dfs(state, 0, Timing{});
        }

        // Find min/max across all paths
        fr.minCycles = INT_MAX;
        fr.maxCycles = 0;
        for (auto& p : fr.paths) {
            fr.minCycles = std::min(fr.minCycles, p.cycles.min());
            fr.maxCycles = std::max(fr.maxCycles, p.cycles.max());
        }
        if (fr.paths.empty()) {
            fr.minCycles = fr.maxCycles = 0;
        }

        if (!fr.paths.empty() && fr.minCycles > fr.maxCycles) {
            throw Error{2, "internal: function '" + fr.name +
                "' has minCycles=" + std::to_string(fr.minCycles) +
                " > maxCycles=" + std::to_string(fr.maxCycles)};
        }

        results.push_back(std::move(fr));
    }

    return results;
}
