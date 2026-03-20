#include "analysis.h"
#include "error.h"
#include "m68k.h"
#include <algorithm>
#include <climits>
#include <span>
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
void computeBodyCost(BasicBlock& b, std::vector<SourceLine>& lines,
                     TimingBase& timing) {
    // Collect body instructions (excludes terminator) for span-based timing,
    // which allows CPUs like the 060 to model dual-issue pairing.
    std::vector<Instruction> body;
    std::vector<int> bodyLines;  // maps body index → lines index
    for (int i = b.firstLine; i <= b.lastLine; i++) {
        if (i < 0 || i >= (int)lines.size()) continue;
        if (!lines[i].inst) continue;
        if (i == b.lastLine && isBlockTerminator(lines[i].inst->mnemonic)) continue;
        body.push_back(*lines[i].inst);
        bodyLines.push_back(i);
    }

    b.body = timing.time(std::span<const Instruction>(body));

    // Mark paired/stalled instructions for display
    auto paired = timing.pairings(std::span<const Instruction>(body));
    auto stalled = timing.stalls(std::span<const Instruction>(body));
    for (size_t j = 0; j < bodyLines.size(); j++) {
        if (paired[j]) {
            lines[bodyLines[j]].paired = true;
            lines[bodyLines[j]].timing = {0, 0};
        } else if (stalled[j]) {
            lines[bodyLines[j]].stalled = true;
            lines[bodyLines[j]].timing += {1, 1};
        }
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
    int loopCount;
    std::vector<PathResult>& results;
    std::vector<int> path;
    std::vector<bool> visited;
    std::vector<LoopAnnotation> loops;
    std::vector<int> cycleGuard; // blocks in active cycles (counter for nesting)
};

// Recursively enumerates execution paths through the CFG.
// Back-edges are treated as loops; exit edges from cycle blocks are followed.
// Cost computation is deferred to computePathCost() after enumeration.
void dfs(DFSState& state, int blockId) {
    if (blockId < 0 || blockId >= (int)state.blocks.size()) {
        throw Error{2, "internal: dfs blockId " + std::to_string(blockId) + " out of range"};
    }
    auto& b = state.blocks[blockId];
    state.path.push_back(blockId);
    state.visited[blockId] = true;

    if (b.isExit) {
        state.results.push_back({state.path, state.loops, {}});
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
                        // Record loop annotation for display
                        state.loops.push_back({targetPathIdx,
                            (int)state.path.size() - 1, state.loopCount});

                        // Find exit paths from cycle blocks
                        std::vector<bool> inCycle(state.blocks.size(), false);
                        for (int pi = targetPathIdx; pi < (int)state.path.size(); pi++) {
                            inCycle[state.path[pi]] = true;
                            // Guard cycle blocks to prevent phantom back-edges.
                            // The header is guarded here to prevent exit-edge
                            // targets from creating false nested loops back to it.
                            state.cycleGuard[state.path[pi]]++;
                        }

                        for (int pi = targetPathIdx; pi < (int)state.path.size() - 1; pi++) {
                            int cycleBlockId = state.path[pi];
                            auto& cb = state.blocks[cycleBlockId];

                            for (auto& exitEdge : cb.successors) {
                                int exitTarget = exitEdge.target;
                                if (exitTarget < 0 || exitTarget >= (int)state.blocks.size()) continue;
                                if (inCycle[exitTarget] || state.visited[exitTarget]) continue;

                                hasForwardEdge = true;
                                dfs(state, exitTarget);
                            }
                        }

                        // Release cycle guard (including header)
                        for (int pi = targetPathIdx; pi < (int)state.path.size(); pi++) {
                            state.cycleGuard[state.path[pi]]--;
                        }
                    }
                }
                continue;
            }

            hasForwardEdge = true;
            dfs(state, target);
        }

        // Dead end or infinite loop (no forward edges found)
        if (!hasForwardEdge) {
            state.results.push_back({state.path, state.loops, {}});
        }

        state.loops.resize(loopsBefore);
    }

    state.path.pop_back();
    state.visited[blockId] = false;
}

// --- Path cost computation ---

// Finds the edge cost from one block to another, or -1 if no edge exists.
int findEdgeCost(const std::vector<BasicBlock>& blocks, int fromId, int toId) {
    for (auto& e : blocks[fromId].successors) {
        if (e.target == toId) return e.cost;
    }
    return -1;
}

// Recursively computes the cost of a path segment [from..to], evaluating
// loops from their annotations. For a loop with count N:
//   body + (body + back_edge) * (N-1) + continuation
// where continuation depends on the exit type (normal, mid-exit, or dead-end).
// skipLoop excludes a loop from matching (prevents infinite recursion when
// computing a loop's own body).
Timing computeSegment(const PathResult& p, int from, int to,
                      const std::vector<BasicBlock>& blocks,
                      int skipLoop = -1) {
    Timing cost;
    int i = from;

    while (i <= to) {
        // Find the widest loop starting at position i
        const LoopAnnotation* loop = nullptr;
        int loopIdx = -1;
        for (int li = 0; li < (int)p.loops.size(); li++) {
            if (li == skipLoop) continue;
            auto& la = p.loops[li];
            if (la.startIdx == i && la.endIdx <= to) {
                if (!loop || la.endIdx > loop->endIdx) {
                    loop = &la;
                    loopIdx = li;
                }
            }
        }

        if (loop) {
            // One iteration body (block bodies + inter-block edges, no latch edge)
            Timing body = computeSegment(p, loop->startIdx, loop->endIdx, blocks, loopIdx);

            // Back-edge: from latch back to header
            int latchId = p.blocks[loop->endIdx];
            int headerId = p.blocks[loop->startIdx];
            int back = std::max(0, findEdgeCost(blocks, latchId, headerId));

            // Extra iterations (each takes the back-edge)
            int extraIters = std::max(0, loop->count - 1);
            cost += body + (body + back) * extraIters;

            // Continuation: depends on how the loop exits
            if (loop->endIdx + 1 <= to) {
                int nextId = p.blocks[loop->endIdx + 1];
                int latchExit = findEdgeCost(blocks, latchId, nextId);

                if (latchExit >= 0) {
                    // Normal exit from latch
                    cost += {latchExit, latchExit};
                } else {
                    // Mid-exit: find which cycle block reaches the next block
                    for (int pi = loop->startIdx; pi < loop->endIdx; pi++) {
                        int midExit = findEdgeCost(blocks, p.blocks[pi], nextId);
                        if (midExit >= 0) {
                            // One extra partial iteration: back-edge + travel to exit block
                            Timing partial = computeSegment(p, loop->startIdx, pi, blocks, loopIdx);
                            cost += Timing{back, back} + partial + Timing{midExit, midExit};
                            break;
                        }
                    }
                }
            }

            i = loop->endIdx + 1;
        } else {
            // Plain block
            cost += blocks[p.blocks[i]].body;

            // Edge to next block (if not the last position in this segment)
            if (i < to) {
                int edgeCost = findEdgeCost(blocks, p.blocks[i], p.blocks[i + 1]);
                if (edgeCost > 0) {
                    cost += {edgeCost, edgeCost};
                }
            }

            i++;
        }
    }

    return cost;
}

// Computes total cycle cost for a path from its block sequence and loop annotations.
Timing computePathCost(const PathResult& p,
                       const std::vector<BasicBlock>& blocks,
                       const std::vector<SourceLine>& lines) {
    if (p.blocks.empty()) return {};

    Timing cost = computeSegment(p, 0, (int)p.blocks.size() - 1, blocks);

    // Add exit instruction cost (rts/rte/rtr) if the last block is an exit
    int lastBlockId = p.blocks.back();
    auto& lastBlock = blocks[lastBlockId];
    if (lastBlock.isExit) {
        int lastIdx = lastBlock.lastLine;
        if (lastIdx >= 0 && lastIdx < (int)lines.size() && lines[lastIdx].inst) {
            cost += lines[lastIdx].timing;
        }
    }

    return cost;
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
            computeBodyCost(b, fr.lines, timing);
        }

        // Build edges
        buildEdges(fr.blocks, fr.lines);

        // Enumerate paths via DFS
        if (!fr.blocks.empty()) {
            DFSState state{fr.blocks, loopCount, fr.paths, {}, {}, {}, {}};
            state.visited.resize(fr.blocks.size(), false);
            state.cycleGuard.resize(fr.blocks.size(), 0);
            dfs(state, 0);
        }

        // Compute cycle costs from path structure
        for (auto& p : fr.paths) {
            p.cycles = computePathCost(p, fr.blocks, fr.lines);
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
