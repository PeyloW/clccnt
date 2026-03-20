#pragma once

#include "types.h"
#include <string>
#include <string_view>
#include <vector>

// One line of parsed assembly source with optional instruction and accumulated metadata.
struct SourceLine {
    int lineNum = 0;
    std::string raw;
    std::vector<std::string> labels;
    std::optional<Instruction> inst;
    Timing timing;
    int instSize = 0;
    int byteOffset = 0;
    bool paired = false;
    bool stalled = false;
};

// Parses a single assembly text line into an Instruction.
// Handles Motorola, GAS, and MIT syntax variants; throws Error on invalid input.
Instruction parseInstruction(std::string_view text);
// Parses multi-line assembly source, extracting labels, skipping directives and comments.
// Unparseable instruction lines are silently skipped.
std::vector<SourceLine> parseSource(std::string_view text);
// True for labels scoped to a function (starting with '.' or numeric).
bool isLocalLabel(std::string_view label);
