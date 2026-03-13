# MC68000 Timing Defects Report

Cross-reference of `src/timing_000.cpp` against `docs/MC68000_Timings.md`.

Note: The 68000 uses `roundToBus()` with busCycles=4, rounding all final values up
to multiples of 4. Defects where the raw difference is masked by rounding (e.g.
ANDI.L/CMPI.L #imm,Dn: ref=14 rounds to 16, matching the implementation) are excluded.

---

## Confirmed Defects

### D1. CMPI.L #imm,\<mem\> — base is 20, should be 12

**Implementation:** `immMem[1]` = 20 (line 71), so `20 + eaFetch(dst)` via `timeImmediate()` line 145
**Reference (S4):** CMPI Long, `op #,M` = **12(3/0)+**

The `immMem` array `{12, 20}` includes write-back cycles, but CMPI is compare-only and does not write to memory.

Example: `CMPI.L #imm,(An)` = 20 + 8 = **28**, should be 12 + 8 = **20**. Overestimates by 8 cycles.

---

### D2. CMPI.B/W #imm,\<mem\> — base is 12, should be 8

**Implementation:** `immMem[0]` = 12 (line 71), so `12 + eaFetch(dst)` via `timeImmediate()` line 145
**Reference (S4):** CMPI Byte/Word, `op #,M` = **8(2/0)+**

Same root cause as D1.

Example: `CMPI.W #imm,(An)` = 12 + 4 = **16**, should be 8 + 4 = **12**. Overestimates by 4 cycles.

---

### D3. TAS \<mem\> — base is 10, should be 14

**Implementation:** `tasMemBase` = 10 (line 94), so `10 + eaFetch(op, byte)` via line 446
**Reference (S5):** TAS Byte Memory = **14(2/1)+**

The reference shows 14 as the base before adding EA calculation time. The implementation uses 10 instead, consistently underestimating by 4 cycles for all TAS memory modes.

Example: `TAS d(An)` = 10 + 8 = **18**, should be 14 + 8 = **22**.

---

### D4. ADDQ/SUBQ.W #n,An — returns 8, should be 4

**Implementation:** Always returns `quickAn` = 8 regardless of size (line 152)
**Reference (S4):**
- ADDQ/SUBQ Byte/Word to An = **4(1/0)**
- ADDQ/SUBQ Long to An = **8(1/0)**

The implementation ignores the operation size and always returns 8 for An destinations. For word size, the reference states 4(1/0). Overestimates by 4 cycles.

---

## Intentional Simplifications

### S1. BCHG/BCLR/BSET.L register — always uses maximum timing

**Implementation (lines 626-635):** Uses fixed values: BCHG/BSET dynamic=8, static=12; BCLR dynamic=10, static=14.
**Reference (S7):** These are marked with `*` meaning "maximum value." The actual minimum is 2 clocks less, depending on the bit position tested.

The implementation always reports the maximum. Real range: BCHG/BSET dynamic 6-8, BCLR dynamic 8-10, and +4 for static (immediate bit number) variants.

---

## Summary

| # | Instruction | Impl | Ref | Delta | Lines |
|---|-------------|------|-----|-------|-------|
| D1 | CMPI.L #imm,\<mem\> | 20 + eaFetch | 12 + eaFetch | +8 | 71, 145 |
| D2 | CMPI.B/W #imm,\<mem\> | 12 + eaFetch | 8 + eaFetch | +4 | 71, 145 |
| D3 | TAS \<mem\> | 10 + eaFetch | 14 + eaFetch | -4 | 94, 446 |
| D4 | ADDQ/SUBQ.W #n,An | 8 | 4(1/0) | +4 | 74, 152 |
| S1 | BCHG/BCLR/BSET.L Dn | max value | min is 2 less | 0 to +2 | 626-635 |

## Root Causes

**D1/D2:** The `immMem` array `{12, 20}` includes write-back cycles, but CMPI is read-only. CMPI needs its own base constants of `{8, 12}`.

**D3:** The `tasMemBase = 10` constant is wrong. TAS uses an indivisible read-modify-write bus cycle. The reference shows 14(2/1) as the base. Should be 14.

**D4:** The `quickAn = 8` constant is used for all sizes, but byte/word to An is 4(1/0). Needs a size check.
