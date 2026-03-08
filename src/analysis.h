#pragma once

#include "parse.h"
#include "timing.h"
#include <string>
#include <vector>

// A CFG edge with target block index and transition cost (branch taken/not-taken cycles).
struct BlockEdge {
    int target = -1;
    int cost = 0;
};

// A straight-line sequence of instructions in the CFG.
// Body timing excludes the block terminator; terminator cost is on the edges.
struct BasicBlock {
    int id = 0;
    std::string label;
    int firstLine = 0;
    int lastLine = 0;
    std::vector<BlockEdge> successors;
    bool isExit = false;
    Timing body;
};

// Loop annotation for path display: marks a contiguous range of blocks as a loop body.
struct LoopAnnotation {
    int startIdx;   // index in PathResult::blocks where loop body starts
    int endIdx;     // index in PathResult::blocks where loop body ends (inclusive)
    int count;      // number of iterations
};

// One complete execution path through a function with its min/max cycle cost.
// Min and max differ when the path contains variable-timing instructions (e.g., MULU).
struct PathResult {
    std::vector<int> blocks;
    std::vector<LoopAnnotation> loops;
    Timing cycles;
};

// Complete analysis output for one function: CFG, all enumerated paths, and min/max cycles.
struct FunctionResult {
    std::string name;
    std::vector<SourceLine> lines;
    std::vector<BasicBlock> blocks;
    std::vector<PathResult> paths;
    int minCycles = 0;
    int maxCycles = 0;
};

// Builds a CFG for each function and enumerates all execution paths via DFS.
// Back-edges are treated as loops with loopCount iterations.
std::vector<FunctionResult> analyzeSource(std::vector<SourceLine>& lines,
                                          TimingBase& timing, int loopCount);
