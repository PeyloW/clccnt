# MC68040 Timing Implementation -- Defect Report

Cross-reference of `src/timing_040.cpp` against `docs/MC68040_Timings.md`
(MC68040 User's Manual Section 10).

Reference cost per instruction = `max(ea_calc, N+M)` where Execute = `NL + M`.

---

## D01: ADDA/SUBA -- under-counts by 1-2 cycles across most modes

**Line 165:** `t.a = t.b = 1 + (inst.src ? eaCost(inst.src->mode) : 0);`

| Mode | Impl | Ref max | Delta |
|------|------|---------|-------|
| Dn   | 1    | 2       | -1    |
| (An) | 1    | 2       | -1    |
| (An)+ | 1   | 3       | -2    |
| -(An) | 1   | 3       | -2    |
| d(An) | 1   | 3       | -2    |
| abs  | 1    | 2       | -1    |
| d(An,Xn) | 3 | 5      | -2    |
| d(PC) | 3   | 4       | -1    |
| d(PC,Xn) | 5 | 6      | -1    |

Base cost of 1 is only correct for An and #imm. The ADDA/SUBA execute
value is 1L+2 (not 1L+0 like plain ADD), making the reference 2 or 3
depending on mode. CMPA is grouped here but has different timing -- see D24.

## D20: BFFFO memory -- under-counts by 2 cycles

**Line 288:** `timeBitField(inst, 6, 9)` -- memCost=9.

| Mode | Impl | Ref max | Delta |
|------|------|---------|-------|
| (An), d(An), abs | 9 | 11 | -2 |

Reference BFFFO (An): calc=9, exec=2L+9=11, max=11. The `memCost`
parameter should be 11 for BFFFO, not 9.

## D22: MOVEP memory-to-register -- uses ea_calc, ignores execute

**Lines 100-106:**
```
if (dst is disp):  // Dn -> d(An)  -- correct
    t = isLong ? 13 : 11;
else:              // d(An) -> Dn  -- wrong
    t = isLong ? 8 : 4;
```

| Instruction | Impl | Ref max | Delta |
|-------------|------|---------|-------|
| MOVEP.W d(An),Dn | 4 (L105) | 7 | -3 |
| MOVEP.L d(An),Dn | 8 (L105) | 10 | -2 |

The implementation uses only the ea_calc column value, ignoring the
execute column entirely. Register-to-memory direction is correct.

## D24: CMPA -- under-counts for address-update modes

CMPA is grouped with ADDA/SUBA at line 165 (base=1), but CMPA.L has
different execute values (1L+1 vs ADDA's 1L+2).

| Mode | Impl | CMPA.L ref max | Delta |
|------|------|----------------|-------|
| (An)+ | 1  | 2              | -1    |
| -(An) | 1  | 2              | -1    |
| d(An) | 1  | 2              | -1    |

A fix for ADDA/SUBA (D01) must separate CMPA to avoid over-counting it,
since CMPA's execute cost is 1 less than ADDA/SUBA for every mode.

---

## Summary Table

| ID | Instruction | Mode(s) | Impl (line) | Ref | Delta |
|----|-------------|---------|-------------|-----|-------|
| D01 | ADDA/SUBA | Dn, (An), abs | 1 (L165) | 2 | -1 |
| D01 | ADDA/SUBA | (An)+, -(An), d(An) | 1 (L165) | 3 | -2 |
| D01 | ADDA/SUBA | d(An,Xn) | 3 (L165) | 5 | -2 |
| D01 | ADDA/SUBA | d(PC) | 3 (L165) | 4 | -1 |
| D01 | ADDA/SUBA | d(PC,Xn) | 5 (L165) | 6 | -1 |
| D20 | BFFFO | mem (An), d(An), abs | 9 (L288) | 11 | -2 |
| D22 | MOVEP.W | d(An)->Dn | 4 (L105) | 7 | -3 |
| D22 | MOVEP.L | d(An)->Dn | 8 (L105) | 10 | -2 |
| D24 | CMPA | (An)+, -(An), d(An) | 1 (L165) | 2 | -1 |
