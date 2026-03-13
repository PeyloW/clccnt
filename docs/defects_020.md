# MC68020/030 Timing Defect Report

Cross-reference of `src/timing_020.cpp` against `docs/MC68030_Timings.md` (Section 11.6 NCC values).

| # | Severity | Area | Instructions Affected | Error |
|---|----------|------|-----------------------|-------|
| 1 | BUG | fiea decomposition | ADDI/SUBI/ANDI/ORI/EORI/CMPI to (An), -(An), d(An) | +1 to +2 cycles overcounted |
| 2 | BUG | Bit test/set Dn imm | BTST/BCHG/BCLR/BSET #data,Dn | +2 cycles overcounted (double-counted imm) |
| 3 | BUG | Bit field mem <5B | BFTST Mem<5B | +2 cycles overcounted |
| 4 | BUG | Bit field mem | BFEXTU/BFEXTS Mem<5B and Mem=5B | -2 / -4 cycles undercounted |
| 5 | BUG | Bit field mem | BFFFO Mem<5B and Mem=5B | -2 / -4 cycles undercounted |
| 6 | BUG | Bit field mem 5B | BFCHG/BFCLR/BFSET Mem=5B | -2 cycles undercounted |
| 7 | LOW | ROXd Mem | ROXd Mem by 1 | Decomposition wrong (adds write), total coincidentally correct on 32-bit bus |

---

## Defect 1: fiea decomposition does not match reference for several modes

**Root cause:** `fiea(dest, sz)` (line 47) computes `fieaImm(sz) + feaTable[dest]`, assuming independent costs. The reference fiea table (Section 11.6.2) shows pipeline overlap that makes the actual cost less than this sum for certain modes.

**Affected instructions:** ADDI, SUBI, ANDI, ORI, EORI, CMPI with memory destination (via `timeImmAlu`, line 670). Also ADD/SUB/AND/OR/EOR/CMP with `#imm` source routed through `timeImmAlu` (lines 198-199, 214-215).

| Destination | Reference NCC | Implementation | Delta |
|-------------|:---:|:---:|:---:|
| #W, (An) | 4 | 5 | **+1** |
| #L, (An) | 5 | 7 | **+2** |
| #W, -(An) | 4 | 6 | **+2** |
| #L, -(An) | 6 | 8 | **+2** |
| #W, d(An) | 5 | 6 | **+1** |

**Affected modes only:** (An), -(An), and d(An) destinations. Other modes ((An)+, d(An,Xn), xxx.W, xxx.L, Dn) happen to sum correctly.

**Suggested fix:** Replace the additive `fiea()` with a dedicated fiea lookup table matching the reference, or add per-mode head corrections.

---

## Defect 2: BTST/BCHG/BCLR/BSET #data,Dn double-counts the immediate fetch

The base costs already include 1 prefetch for the immediate word. The code then adds `fieaImm(OpSize::word)` which adds another prefetch.

**Line 864-868** (`timeBtst`): base `{2, 0, 1, 0}` + `fieaImm` `{0,0,1,0}` = `{2,0,2,0}` -> 6 NCC.
**Line 887-890** (`timeBitOp`): base `{4, 0, 1, 0}` + `fieaImm` `{0,0,1,0}` = `{4,0,2,0}` -> 8 NCC.

| Instruction | Reference | Implementation | Delta |
|-------------|:---------:|:--------------:|:-----:|
| BTST #data, Dn | 4 | 6 | **+2** |
| BCHG #data, Dn | 6 | 8 | **+2** |
| BCLR #data, Dn | 6 | 8 | **+2** |
| BSET #data, Dn | 6 | 8 | **+2** |

**Suggested fix:** For the Dn case with immediate source, use the base cost directly without adding `fieaImm()`.

---

## Defect 3: BFTST Mem <5 bytes overcounts by 2

**Reference:** `BFTST Mem` (<5B) = 10(1/1/0) NCC. Correct BusCost: `{6, 1, 1, 0}`.

**Line 311:** memSmall is `{8, 1, 1, 0}` (head 8 instead of 6) -> 12 NCC.

| Operand | Reference | Implementation | Delta |
|---------|:---------:|:--------------:|:-----:|
| BFTST Mem <5B | 10 | 12 | **+2** |

**Suggested fix:** Change memSmall from `{8, 1, 1, 0}` to `{6, 1, 1, 0}` on line 311.

---

## Defect 4: BFEXTU/BFEXTS Mem undercounts

**Reference:** Mem <5B = 12(1/1/0), Mem 5B = 18(2/1/0). Correct: `{8,1,1,0}` and `{12,2,1,0}`.

**Line 314:** memSmall is `{6, 1, 1, 0}` (head 6 instead of 8), memLarge is `{8, 2, 1, 0}` (head 8 instead of 12).

| Operand | Reference | Implementation | Delta |
|---------|:---------:|:--------------:|:-----:|
| BFEXTU Mem <5B | 12 | 10 | **-2** |
| BFEXTU Mem =5B | 18 | 14 | **-4** |

**Suggested fix:** Change line 314 to `{8, 0, 1, 0}, {8, 1, 1, 0}, {12, 2, 1, 0}`.

---

## Defect 5: BFFFO Mem undercounts

**Reference:** Mem <5B = 22(1/1/0), Mem 5B = 28(2/1/0). Correct: `{18,1,1,0}` and `{22,2,1,0}`.

**Line 317:** memSmall is `{16, 1, 1, 0}` (head 16 instead of 18), memLarge is `{18, 2, 1, 0}` (head 18 instead of 22).

| Operand | Reference | Implementation | Delta |
|---------|:---------:|:--------------:|:-----:|
| BFFFO Mem <5B | 22 | 20 | **-2** |
| BFFFO Mem =5B | 28 | 24 | **-4** |

**Suggested fix:** Change line 317 to `{18, 0, 1, 0}, {18, 1, 1, 0}, {22, 2, 1, 0}`.

---

## Defect 6: BFCHG/BFCLR/BFSET Mem 5 bytes undercounts

**Reference:** Mem 5B = 22(2/1/2). Correct: `{12, 2, 1, 2}`.

**Line 320:** memLarge is `{10, 2, 1, 2}` (head 10 instead of 12) -> 20 NCC.

| Operand | Reference | Implementation | Delta |
|---------|:---------:|:--------------:|:-----:|
| BFCHG Mem =5B | 22 | 20 | **-2** |

**Suggested fix:** Change memLarge from `{10, 2, 1, 2}` to `{12, 2, 1, 2}` on line 320.

---

## Defect 7 (Low): ROXd Mem by 1 -- wrong decomposition, correct total

**Reference (line 333):** `ROXd Mem by 1` = 4(0/1/0) -- 0 reads, 1 prefetch, **0 writes**.

**Line 786:** `base = {0, 0, 1, 1}` -- has writes=1 where reference shows 0.

Total is coincidentally correct on 32-bit bus: `{0,0,1,1}` -> 0+2*(0+1+1) = 4. But the decomposition has a phantom write that would produce wrong results on a different bus width.
