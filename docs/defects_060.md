# MC68060 Timing Implementation — Defect Report

Reference: `docs/MC68060_Timings.md` (MC68060UM Section 10)
Implementation: `src/timing_060.cpp`

---

## Timing Defects

### D1. PEA (xxx).W and (xxx).L: over-counted by 1 cycle

- **Line:** 155–158
- **Reference:** Table 10-20, PEA row, `(xxx).WL` column: **1(0/1)**
- **Implementation:** `hasExtWord(absW)` and `hasExtWord(absL)` both return true, so returns **2**

### D2. PEA d(PC): over-counted by 1 cycle

- **Line:** 155–158 (same code path as D1)
- **Reference:** Table 10-20, PEA row, `(d16,PC)` column: **1(0/1)**
- **Implementation:** `hasExtWord(pcDisp)` returns true, so returns **2**

PEA reference pattern: (An)=1, d(An)=2, d(An,Xn)=2, abs.WL=1, d(PC)=1, d(PC,Xn)=2.
The `hasExtWord` predicate is too broad — it catches `absW`, `absL`, and `pcDisp` which do not cost +1 for PEA.

### D3. MOVEM d(An): over-counted by 1 cycle

- **Line:** 470
- **Reference:** Table 10-20, MOVEM row, `(d16,An)` column: **n(n/0)** mem->reg, **n(0/n)** reg->mem
- **Implementation:** `hasExtWord(disp)` returns true, so returns **n+1**

### D4. MOVEM d(PC): over-counted by 1 cycle

- **Line:** 470 (same code path as D3)
- **Reference:** Table 10-20, MOVEM row, `(d16,PC)` column: **n(n/0)**
- **Implementation:** `hasExtWord(pcDisp)` returns true, so returns **n+1**

MOVEM reference pattern: (An)=n, (An)+=n, d(An)=n, d(An,Xn)=n+1, abs.WL=n+1, d(PC)=n, d(PC,Xn)=n+1.

### D5. Bit field instructions with d(An): over-counted by 1 cycle

- **Line:** 508
- **Reference:** Table 10-16, all bit field instructions, `(d16,An)` column has the **same** timing as `(An)` — no +1
- **Implementation:** `hasExtWord(disp)` returns true, so adds +1
- **Affected instructions:** BFTST, BFEXTU, BFEXTS, BFFFO, BFCHG, BFCLR, BFSET, BFINS

### D6. Bit field instructions with d(PC): likely over-counted by 1 cycle

- **Line:** 508 (same code path as D5)
- **Reference:** Table 10-16 does not list d(PC) explicitly, but EA calculation time for d(PC) is 0(0/0) per Table 10-5, same as d(An). Following the pattern, d(PC) should have base timing.
- **Implementation:** `hasExtWord(pcDisp)` returns true, so adds +1

Bit field reference pattern: (An)=base, d(An)=base, d(An,Xn)=base+1, abs.WL=base+1.

---

## Root Cause (D1–D6)

The `hasExtWord()` predicate returns true for six modes: `disp`, `index`, `absW`, `absL`, `pcDisp`, `pcIndex`. On the MC68060, the +1 cycle penalty varies by instruction class:

| Instruction class | d(An) +1? | d(An,Xn) +1? | abs.WL +1? | d(PC) +1? | d(PC,Xn) +1? |
|---|---|---|---|---|---|
| PEA | Yes | Yes | **No** | **No** | Yes |
| MOVEM | **No** | Yes | Yes | **No** | Yes |
| Bit field | **No** | Yes | Yes | **No** (likely) | Yes (likely) |

Using `hasExtWord()` as a uniform +1 predicate does not match the per-instruction patterns in the reference.

**Suggested fix:** For **PEA**, the +1 applies to `disp`, `index`, and `pcIndex` only. For **MOVEM** and **bit fields**, the +1 applies to `index`, `absW`, `absL`, and `pcIndex` only.

---

## Superscalar Pairing Defects

### D7. TRAP not marked as pairable

- **Line:** 62–63 (falls through to `default: return false`)
- **Reference:** Table 10-2: `TRAP` = **pOEP | sOEP**

### D8. TRAPF not marked as pairable

- **Line:** 62–63 (falls through to `default: return false`)
- **Reference:** Table 10-2: `TRAPF` = **pOEP | sOEP**
- **Note:** TRAPF is sometimes used as a multi-word NOP in optimized code, so this affects real-world pairing analysis.

### D9. ILLEGAL not marked as pairable

- **Line:** 62–63 (falls through to `default: return false`)
- **Reference:** Table 10-2: `ILLEGAL` = **pOEP | sOEP**
