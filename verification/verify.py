#!/usr/bin/env python3
"""Verification script for clccnt timing values."""

import json
import subprocess
import sys
from pathlib import Path


def fmt_range(lo, hi):
    return str(lo) if lo == hi else f"{lo}-{hi}"


def run_test(binary, instructions, cpu):
    """Run clccnt and return (min, max) cycle count."""
    asm = instructions.replace(";", "\n") + "\n"
    try:
        result = subprocess.run(
            [binary, "-c", cpu],
            input=asm, capture_output=True, text=True, timeout=5
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None
    if result.returncode != 0:
        return None

    # Parse last non-empty line: "  <name>                  <value>"
    # Value is either "N" or "N-M"
    lines = result.stdout.strip().splitlines()
    if not lines:
        return None
    token = lines[-1].strip().split()[-1]
    if "-" in token and not token.startswith("-"):
        parts = token.split("-", 1)
        return (int(parts[0]), int(parts[1]))
    return (int(token), int(token))


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "./build/clccnt"
    script_dir = Path(__file__).parent
    with open(script_dir / "testcases.json") as f:
        tests = json.load(f)

    passed = 0
    total = len(tests)

    for test in tests:
        name = test["name"]
        expected = test["exp"]
        failed_cpus = []

        for cpu, exp in expected.items():
            if exp is None:
                continue
            result = run_test(binary, test["insn"], cpu)
            if result is None:
                failed_cpus.append(f"{cpu}:error")
                continue
            if result[0] != exp[0] or result[1] != exp[1]:
                failed_cpus.append(
                    f"{cpu}:{fmt_range(*result)}!={fmt_range(*exp)}")

        if failed_cpus:
            print(f"FAIL: {name} [{', '.join(failed_cpus)}]")
        else:
            passed += 1

    print(f"PASSED {passed} of {total}")
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
