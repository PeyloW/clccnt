# MC68060 User's Manual — Section 10: Instruction Execution Timing

## Timing Notation

```
C(r/w)
```

- **C** = Total processor clock cycles, including all operand fetches/stores and internal CPU cycles
- **r/w** = Operand reads (r) and writes (w); a read-modify-write cycle is denoted `(1/1)`

Variable-time instructions are listed as `<= k(r/w)` where k is the maximum.

---

## 10.1 Superscalar Operand Execution Pipelines

The MC68060 has three structures in the Operand Execution Pipeline (OEP):
- **pOEP** — Primary OEP
- **sOEP** — Secondary OEP
- Monolithic register file (Dn, An)

Each OEP contains:
- **AGU** — Address Generation Unit (Base, Index → Address_result)
- **IEE** — Integer Execute Engine (A, B → Execute_result)

### Instruction Classifications

**Standard instructions** (max 1 extension word, max 1 memory access, resources fully specified by opword):

| Class | Description |
|---|---|
| `pOEP \| sOEP` | May execute in either OEP; all standard single-cycle instructions |
| `pOEP-only` | Primary OEP only; all multi-cycle standard instructions |

**Non-standard instructions:**

| Class | Description |
|---|---|
| `pOEP-until-last` | Multi-standard-op decomposition; sOEP may dispatch a `pOEP\|sOEP` instruction during the last cycle of pOEP execution |
| `pOEP-only` | Primary OEP only |
| `pOEP-but-allows-sOEP` | Must execute in pOEP, but allows `pOEP\|sOEP` instructions to dispatch to sOEP simultaneously |

### Table 10-1. Superscalar OEP Dispatch Test 2 Algorithm

| pOEP Contents | sOEP Contents | Result |
|---|---|---|
| pOEP \| sOEP | pOEP \| sOEP | **Test 2 succeeds** |
| pOEP \| sOEP | pOEP-only | Test 2 fails |
| pOEP \| sOEP | pOEP-until-last | Test 2 fails |
| pOEP \| sOEP | pOEP-but-allows-sOEP | Test 2 fails |
| pOEP-only | (any) | Test 2 fails |
| pOEP-until-last | pOEP \| sOEP | **Test 2 succeeds** |
| pOEP-until-last | pOEP-only | Test 2 fails |
| pOEP-until-last | pOEP-until-last | Test 2 fails |
| pOEP-until-last | pOEP-but-allows-sOEP | Test 2 fails |
| pOEP-but-allows-sOEP | pOEP \| sOEP | **Test 2 succeeds** |
| pOEP-but-allows-sOEP | pOEP-only | Test 2 fails |
| pOEP-but-allows-sOEP | pOEP-until-last | Test 2 fails |
| pOEP-but-allows-sOEP | pOEP-but-allows-sOEP | Test 2 fails |

### Dispatch Tests 3–6 Summary

- **Test 3**: sOEP does not support `(bd,An,Xi∗SF)` or any PC-relative modes
- **Test 4**: Only one operand data memory reference allowed across the pOEP+sOEP pair
- **Test 5**: No register conflict on sOEP.AGU resources (Base, Index) vs pOEP Address_result or Execute_result
- **Test 6**: No register conflict on sOEP.IEE resources (A, B) vs pOEP Execute_result
  - Exception 1: `MOVE.L,Rx` in pOEP — data bypassed if Rx needed as sOEP.A or .B
  - Exception 2: `<op>.l,Dx` / `mov.l Dx,<mem>` — pOEP result sourced directly to sOEP memory write

---

## 10.2 Timing Assumptions

1. Timings are for individual instructions; superscalar dispatch is not assumed. In paired dispatch, total time = pOEP instruction time.
2. OEP is pre-loaded with opword + all required extensions (no IFP stall).
3. No sequence-related pipeline stalls assumed. **Change/use penalties not included:**
   - Base register (An) load then use: up to **2-cycle stall**
   - Index register (Xi.l∗2, Xi.l∗8, Xi.w): up to **3-cycle stall**
   - Index register (Xi.l∗1, Xi.l∗4): up to **2-cycle stall**
   - Optimized zero-stall instructions for base (and Xi.l∗{1,4}): `lea`, `mov.l&imm,Rn`, `movq`, `clr.l Dn`, `any op(An)+`, `any op–(An)`
   - Load An from memory then use in address: **1-cycle stall**
4. All memory accesses hit in ATC and operand data cache (no miss stalls). Branch targets hit in instruction cache.
5. Aligned accesses: 16-bit on mod-2, 32-bit on mod-4, 64-bit on mod-8, 96-bit on mod-4.
   - Misaligned read: +1 cycle; misaligned write or RMW: +2 cycles
6. Pipeline-synchronizing instructions wait for: IFP quiet, data cache quiet, push/write buffers empty, all prior instructions complete. Assumes these conditions already met for listed timings.
   - Pipeline-sync instructions: `andi_to_sr`, `bkpt`, `cas`, `cinv`, `cpush`, `eori_to_sr`, `halt`, `lpstop`, `move_to_sr`, `movec`, `nop`, `ori_to_sr`, `pflush`, `plpa`, `reset`, `rte`, `stop`, `tas`
7. Variable-time instructions listed as `<= k(r/w)`.

---

## 10.3 Cache and ATC Performance Degradation Times

### 10.3.1 Instruction ATC Miss
Assumes single "C-index" level table search; memory response time = `w-x-y-z`:
- U-bit already set: `10+3*w (3/0)`
- U-bit must be set by MC68060: `18+5*w (4/1)`

### 10.3.2 Data ATC Miss
Assumes single "C-index" level; memory response time = `w-x-y-z`:
- U-bit and M-bit in proper state: `8+3*w (3/0)`
- M-bit only, or U-bit+M-bit must be set: `14+4*w (3/1)`
- U-bit only must be set: `16+5*w (4/1)`

### 10.3.3 Instruction Cache Miss
Conservative estimate (IFP buffer empty, fully exposed miss; normally hidden by prefetch buffer):
- Line Fill: `w+x+y+z`

### 10.3.4 Data Cache Miss
Memory response time = `w-x-y-z`:
- Copyback mode (line fill): `2+w` {+ possible pipeline stall during x+y+z if subsequent instruction makes a data reference before line fill completes}
- Noncachable mode (read): `2+w`
- Noncacheable mode (write), precise mode or write buffer disabled: `3+w`

---

## 10.4 Effective Address Calculation Times

### Table 10-5. Effective Address Calculation Times

| Addressing Mode | Calc Time |
|---|---|
| Dn — Data Register Direct | 0(0/0) |
| An — Address Register Direct | 0(0/0) |
| (An) — Address Register Indirect | 0(0/0) |
| (An)+ — Indirect with Postincrement | 0(0/0) |
| –(An) — Indirect with Predecrement | 0(0/0) |
| (d16,An) — Indirect with Displacement | 0(0/0) |
| (d8,An,Xi∗SF) — Indirect with Index + Byte Displacement | 0(0/0) |
| (bd,An,Xi∗SF) — Indirect with Index + Base (16/32-bit) Displacement | 1(0/0) |
| ([bd,An,Xn],od) — Memory Indirect Preindexed | 3(1/0) |
| ([bd,An],Xn,od) — Memory Indirect Postindexed | 3(1/0) |
| (xxx).W — Absolute Short | 0(0/0) |
| (xxx).L — Absolute Long | 0(0/0) |
| (d16,PC) — PC with Displacement | 0(0/0) |
| (d8,PC,Xi∗SF) — PC with Index + Byte Displacement | 0(0/0) |
| (bd,PC,Xi∗SF) — PC with Index + Base (16/32-bit) Displacement | 1(0/0) |
| #\<data\> — Immediate | 0(0/0) |
| ([bd,PC,Xn],od) — PC Memory Indirect Preindexed | 3(1/0) |
| ([bd,PC],Xn,od) — PC Memory Indirect Postindexed | 3(1/0) |

Notes:
- Xi size and scale factor do not affect indexed mode calculation time
- `(xxx).WL` denotes either `.W` or `.L` absolute
- Memory indirect EA adds **3 cycles** total (1 for full-format EA + 2 for pointer fetch)
- Memory-to-memory instructions (e.g., MOVE) perform two EA calculations

---

## 10.5 MOVE Instruction Execution Times

For memory indirect addressing, add `2(1/0)` to entries in Table 10-6 / Table 10-7.

### Table 10-6. Move Byte and Word Execution Times

| Source \ Dest | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF) | (xxx).WL |
|---|---|---|---|---|---|---|---|---|---|
| Dn | 1(0/0) | 1(0/0) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 1(0/1) |
| An | 1(0/0) | 1(0/0) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 1(0/1) |
| (An) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (An)+ | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| –(An) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (d16,An) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (d8,An,Xi∗SF) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (bd,An,Xi∗SF) | 2(1/0) | 2(1/0) | 3(1/1) | 3(1/1) | 3(1/1) | 3(1/1) | 3(1/1) | 4(1/1) | 3(1/1) |
| (xxx).W | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (xxx).L | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (d16,PC) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (d8,PC,Xi∗SF) | 1(1/0) | 1(1/0) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| (bd,PC,Xi∗SF) | 2(1/0) | 2(1/0) | 3(1/1) | 3(1/1) | 3(1/1) | 3(1/1) | 3(1/1) | 4(1/1) | 3(1/1) |
| #\<data\> | 1(0/0) | 1(0/0) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 2(0/1) | 3(0/1) | 2(0/1) |

### Table 10-7. Move Long Execution Times

Identical to Table 10-6 (byte/word); all MOVE.L timings are the same.

### Table 10-8. MOVE16 Execution Times

Note: Assumes cache misses for both read and write accesses. If read hits in operand data cache: `11(1/1)`. Operand read/write refers to a line-sized transfer.

| Source \ Dest | (Ax) | (Ax)+ | (xxx).L |
|---|---|---|---|
| (Ay) | — | — | 18(1/1) |
| (Ay)+ | — | 18(1/1) | 18(1/1) |
| (xxx).L | 18(1/1) | 18(1/1) | — |

---

## 10.6 Standard Instruction Execution Times

Add EA calculation time (Table 10-5) to all entries. `<ea>` = any effective address; `<M>` = memory operand.

**Note 1:** For `op <ea>,An`, add 1 cycle if `<ea>` is `(Ay)+` or `–(Ay)` and Ay = An.  
**Note 2:** Word divides have conditional exit points.  
**Note 3 (DIVS/DIVU/MULS/MULU Long):** Add 1 cycle to EA calc time for all modes except Rn, (An), (An)+, –(An), (d16,An), (d16,PC).

### Table 10-9. Standard Instruction Execution Times

| Instruction | Size | op \<ea\>,An¹ | op \<ea\>,Dn | op Dn,\<M\> |
|---|---|---|---|---|
| ADD | Byte, Word | 1(1/0) | 1(1/0) | 1(1/1) |
| ADD | Long | 1(1/0) | 1(1/0) | 1(1/1) |
| AND | Byte, Word | — | 1(1/0) | 1(1/1) |
| AND | Long | — | 1(1/0) | 1(1/1) |
| CMP | Byte, Word | 1(1/0) | 1(1/0) | — |
| CMP | Long | 1(1/0) | 1(1/0) | — |
| DIVS | Word | — | <=22(1/0)² | — |
| DIVS | Long³ | — | 38(1/0) | — |
| DIVU | Word | — | <=22(1/0)² | — |
| DIVU | Long³ | — | 38(1/0) | — |
| EOR | Byte, Word | — | 1(1/0) | 1(1/1) |
| EOR | Long | — | 1(1/0) | 1(1/1) |
| MULS | Word | — | 2(1/0) | — |
| MULS | Long³ | — | 2(1/0) | — |
| MULU | Word | — | 2(1/0) | — |
| MULU | Long³ | — | 2(1/0) | — |
| OR | Byte, Word | — | 1(1/0) | 1(1/1) |
| OR | Long | — | 1(1/0) | 1(1/1) |
| SUB | Byte, Word | 1(1/0) | 1(1/0) | 1(1/1) |
| SUB | Long | 1(1/0) | 1(1/0) | 1(1/1) |

---

## 10.7 Immediate Instruction Execution Times

Note¹: Add `2(1/0)` to the `(bd,An,Xi∗SF)` time for memory indirect addressing.

### Table 10-10. Immediate Instruction Execution Times

| Instruction | Size | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)¹ | (xxx).WL |
|---|---|---|---|---|---|---|---|---|---|---|
| ADDI | Byte, Word | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| ADDI | Long | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| ADDQ | Byte, Word | 1(0/0) | 1(0/0) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 1(1/1) |
| ADDQ | Long | 1(0/0) | 1(0/0) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 1(1/1) |
| ANDI | Byte, Word | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| ANDI | Long | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| CMPI | Byte, Word | 1(0/0) | — | 1(1/0) | 1(1/0) | 1(1/0) | 2(1/0) | 2(1/0) | 3(1/0) | 2(1/0) |
| CMPI | Long | 1(0/0) | — | 1(1/0) | 1(1/0) | 1(1/0) | 2(1/0) | 2(1/0) | 3(1/0) | 2(1/0) |
| EORI | Byte, Word | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| EORI | Long | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| MOVEQ | Long | 1(0/0) | — | — | — | — | — | — | — | — |
| ORI | Byte, Word | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| ORI | Long | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| SUBI | Byte, Word | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| SUBI | Long | 1(0/0) | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| SUBQ | Byte, Word | 1(0/0) | 1(0/0) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 1(1/1) |
| SUBQ | Long | 1(0/0) | 1(0/0) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 1(1/1) |

---

## 10.8 Single-Operand Instruction Execution Times

Add EA calculation time where indicated (Note²).  
Note¹ (CAS/TAS): Add `(1 + EA calc time)` for all modes except Rn, (An), (An)+, –(An), (d16,An).

### Table 10-11. Single-Operand Instruction Execution Times

| Instruction | Size | Register | Memory² |
|---|---|---|---|
| CAS | Byte, Word¹ | — | 19(1/1) |
| CAS | Long¹ | — | 19(1/1) |
| NBCD | Byte | 1(0/0) | 1(1/1) |
| NEG | Byte, Word | 1(0/0) | 1(1/1) |
| NEG | Long | 1(0/0) | 1(1/1) |
| NEGX | Byte, Word | 1(0/0) | 1(1/1) |
| NEGX | Long | 1(0/0) | 1(1/1) |
| NOT | Byte, Word | 1(0/0) | 1(1/1) |
| NOT | Long | 1(0/0) | 1(1/1) |
| Scc | Byte → False | 1(0/0) | 1(1/1) |
| Scc | Byte → True | 1(0/0) | 1(1/1) |
| TAS | Byte¹ | 1(0/0) | 17(1/1) |
| TST | Byte, Word | 1(0/0) | 1(1/0) |
| TST | Long | 1(0/0) | 1(1/0) |

### Table 10-12. CLR Execution Times

Note¹: Add `2(1/0)` to `(bd,An,Xi∗SF)` time for memory indirect.

| Size | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)¹ | (xxx).WL |
|---|---|---|---|---|---|---|---|---|---|
| Byte, Word | 1(0/0) | — | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 1(0/1) |
| Long | 1(0/0) | — | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 1(0/1) |

---

## 10.9 Shift/Rotate Execution Times

Add EA calculation time for Memory column (word-size only for memory shifts).

### Table 10-13. Shift/Rotate Execution Times

| Instruction | Size | Register | Memory¹ |
|---|---|---|---|
| ASL, ASR | Byte, Word | 1(0/0) | 1(1/1) |
| ASL, ASR | Long | 1(0/0) | — |
| LSL, LSR | Byte, Word | 1(0/0) | 1(1/1) |
| LSL, LSR | Long | 1(0/0) | — |
| ROL, ROR | Byte, Word | 1(0/0) | 1(1/1) |
| ROL, ROR | Long | 1(0/0) | — |
| ROXL, ROXR | Byte, Word | 1(0/0) | 1(1/1) |
| ROXL, ROXR | Long | 1(0/0) | — |

---

## 10.10 Bit Manipulation and Bit Field Execution Times

### Table 10-14. Bit Manipulation — Dynamic Bit Count

Add EA calculation time for Memory column.

| Instruction | Size | Register | Memory¹ |
|---|---|---|---|
| BCHG | Byte | — | 1(1/1) |
| BCHG | Long | 1(0/0) | — |
| BCLR | Byte | — | 1(1/1) |
| BCLR | Long | 1(0/0) | — |
| BSET | Byte | — | 1(1/1) |
| BSET | Long | 1(0/0) | — |
| BTST | Byte | — | 1(1/0) |
| BTST | Long | 1(0/0) | — |

### Table 10-15. Bit Manipulation — Static Bit Count

Note¹: Add `2(1/0)` to `(bd,An,Xi∗SF)` time for memory indirect.

| Instruction | Size | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)¹ | (xxx).WL |
|---|---|---|---|---|---|---|---|---|---|---|
| BCHG | Byte | — | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| BCHG | Long | 1(0/0) | — | — | — | — | — | — | — | — |
| BCLR | Byte | — | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| BCLR | Long | 1(0/0) | — | — | — | — | — | — | — | — |
| BSET | Byte | — | — | 1(1/1) | 1(1/1) | 1(1/1) | 2(1/1) | 2(1/1) | 3(1/1) | 2(1/1) |
| BSET | Long | 1(0/0) | — | — | — | — | — | — | — | — |
| BTST | Byte | — | — | 1(1/0) | 1(1/0) | 1(1/0) | 2(1/0) | 2(1/0) | 3(1/0) | 2(1/0) |
| BTST | Long | 1(0/0) | — | — | — | — | — | — | — | — |

### Table 10-16. Bit Field Execution Times

Notes: Offset/width type (static or dynamic) does not affect execution time. Add `2(1/0)` to `(bd,An,Xi∗SF)` for memory indirect.

| Instruction | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)² | (xxx).WL |
|---|---|---|---|---|---|---|---|---|---|
| BFCHG (< 5 Bytes) | 8(0/0) | — | 8(2/1) | — | — | 8(2/1) | 9(2/1) | 10(2/1) | 9(2/1) |
| BFCHG (= 5 Bytes) | 12(0/0) | — | 12(4/2) | — | — | 12(4/2) | 13(4/2) | 14(4/2) | 13(4/2) |
| BFCLR (< 5 Bytes) | 8(0/0) | — | 8(2/1) | — | — | 8(2/1) | 9(2/1) | 10(2/1) | 9(2/1) |
| BFCLR (= 5 Bytes) | 12(0/0) | — | 12(4/2) | — | — | 12(4/2) | 13(4/2) | 14(4/2) | 13(4/2) |
| BFEXTS (< 5 Bytes) | 6(0/0) | — | 6(1/0) | — | — | 6(1/0) | 7(1/0) | 8(1/0) | 7(1/0) |
| BFEXTS (= 5 Bytes) | 8(0/0) | — | 8(2/0) | — | — | 8(2/0) | 9(2/0) | 10(2/0) | 9(2/0) |
| BFEXTU (< 5 Bytes) | 6(0/0) | — | 6(1/0) | — | — | 6(1/0) | 7(1/0) | 8(1/0) | 7(1/0) |
| BFEXTU (= 5 Bytes) | 8(0/0) | — | 8(2/0) | — | — | 8(2/0) | 9(2/0) | 10(2/0) | 9(2/0) |
| BFFFO (< 5 Bytes) | 9(0/0) | — | 9(1/0) | — | — | 9(1/0) | 10(1/0) | 11(1/0) | 10(1/0) |
| BFFFO (= 5 Bytes) | 11(0/0) | — | 11(2/0) | — | — | 11(2/0) | 12(2/0) | 13(2/0) | 12(2/0) |
| BFINS (< 5 Bytes) | 6(0/0) | — | 6(1/1) | — | — | 6(1/1) | 7(1/1) | 8(1/1) | 7(1/1) |
| BFINS (= 5 Bytes) | 6(0/0) | — | 6(2/2) | — | — | 6(2/2) | 7(2/2) | 8(2/2) | 7(2/2) |
| BFSET (< 5 Bytes) | 8(0/0) | — | 8(2/1) | — | — | 8(2/1) | 9(2/1) | 10(2/1) | 9(2/1) |
| BFSET (= 5 Bytes) | 12(0/0) | — | 12(4/2) | — | — | 12(4/2) | 13(4/2) | 14(4/2) | 13(4/2) |
| BFTST (< 5 Bytes) | 6(0/0) | — | 6(1/0) | — | — | 6(1/0) | 7(1/0) | 8(1/0) | 7(1/0) |
| BFTST (= 5 Bytes) | 8(0/0) | — | 8(2/0) | — | — | 8(2/0) | 9(2/0) | 10(2/0) | 9(2/0) |

---

## 10.11 Branch Instruction Execution Times

Add EA calculation time for JMP/JSR (Table 10-18).

### Table 10-17. Branch Execution Times

| Instruction | Not Pred, Fwd, Taken | Not Pred, Fwd, Not Taken | Not Pred, Bwd, Taken | Not Pred, Bwd, Not Taken | Pred Correct Taken | Pred Correct Not Taken | Pred Incorrect |
|---|---|---|---|---|---|---|---|
| Bcc | 7(0/0) | 1(0/0) | 3(0/0) | 7(0/0) | 0(0/0) | 1(0/0) | 7(0/0) |
| BRA | 3(0/0) | — | 3(0/0) | — | 0(0/0) | — | — |
| BSR | 3(0/1) | — | 3(0/1) | — | 1(0/1) | — | — |
| DBcc | 3(0/0) | 8(0/0) | 3(0/0) | 8(0/0) | 2(0/0) | 2(0/0) | 8(0/0) |
| DBRA | 3(0/0) | 7(0/0) | 3(0/0) | 7(0/0) | 1(0/0) | 1(0/0) | 7(0/0) |
| FBcc | 8(0/0) | 2(0/0) | 8(0/0) | 2(0/0) | 2(0/0) | 2(0/0) | 8(0/0) |

Notes on Bcc prediction:
- A Bcc is `pOEP-but-allows-sOEP` if: not predicted + forward direction, OR predicted as not-taken
- Instruction folding allows 1 or 2 instructions to execute simultaneously with a predicted-taken Bcc (also BRA, JMP)

### Table 10-18. JMP / JSR Execution Times

Add EA calculation time for each entry.

| Instruction | Not Pred, Fwd, Taken | Not Pred, Bwd, Taken | Pred Correct Taken |
|---|---|---|---|
| JMP (d16,PC) | 3(0/0) | 3(0/0) | 0(0/0) |
| JMP xxx.WL | 3(0/0) | 3(0/0) | 0(0/0) |
| Remaining JMP | 5(0/0) | 5(0/0) | 5(0/0) |
| JSR (d16,PC) | 3(0/1) | 3(0/1) | 1(0/1) |
| JSR xxx.WL | 3(0/1) | 3(0/1) | 1(0/1) |
| Remaining JSR | 5(0/1) | 5(0/1) | 5(0/1) |

### Table 10-19. Return Instruction Execution Times

| Instruction | Execution Time |
|---|---|
| RTD | 7(1/0) |
| RTE | 17(3/0) |
| RTR | 8(2/0) |
| RTS | 7(1/0) |

---

## 10.12 LEA, PEA, and MOVEM Execution Times

Note¹: Add `2(1/0)` to `(bd,{An,PC},Xi∗SF)` for memory indirect.  
Note²: `n` = number of registers being moved.

### Table 10-20. LEA, PEA, and MOVEM Instruction Execution Times

| Instruction | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)¹ | (xxx).WL | (d16,PC) | (d8,PC,Xi∗SF) | (bd,PC,Xi∗SF)¹ |
|---|---|---|---|---|---|---|---|---|---|---|
| LEA | 1(0/0) | — | — | 1(0/0) | 1(0/0) | 2(0/0) | 1(0/0) | 1(0/0) | 1(0/0) | 2(0/0) |
| PEA | 1(0/1) | — | — | 2(0/1) | 2(0/1) | 3(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 2(0/1) |
| MOVEM Mem→Reg | n²(n/0) | n(n/0) | — | n(n/0) | 1+n(n/0) | 2+n(n/0) | 1+n(n/0) | n(n/0) | 1+n(n/0) | 2+n(n/0) |
| MOVEM Reg→Mem | n(0/n) | — | n(0/n) | n(0/n) | 1+n(0/n) | 2+n(0/n) | 1+n(0/n) | — | — | — |

---

## 10.13 Multiprecision Instruction Execution Times

Note: `<ea>y,<ea>x` = `(Ay)+,(Ax)+` for CMPM; `–(Ay),–(Ax)` for all others.

### Table 10-21. Multiprecision Instruction Execution Times

| Instruction | Size | op Dy,Dx | op \<ea\>y,\<ea\>x |
|---|---|---|---|
| ADDX | Byte, Word | 1(0/0) | 2(2/1) |
| ADDX | Long | 1(0/0) | 2(2/1) |
| CMPM | Byte, Word | — | 2(2/0) |
| CMPM | Long | — | 2(2/0) |
| SUBX | Byte, Word | 1(0/0) | 2(2/1) |
| SUBX | Long | 1(0/0) | 2(2/1) |
| ABCD | Byte | 1(0/0) | 2(2/1) |
| SBCD | Byte | 1(0/0) | 2(2/1) |

---

## 10.14 Status Register, MOVES, and Miscellaneous Instruction Execution Times

### Table 10-22. Status Register (SR) Instruction Execution Times

| Instruction | Execution Time |
|---|---|
| ANDI to SR | 12(0/0) |
| EORI to SR | 12(0/0) |
| MOVE from SR | 1(0/1)¹ |
| MOVE to SR | 12(1/0)¹ |
| ORI to SR | 5(0/0) |

Note¹: Add EA calculation time.

### Table 10-23. MOVES Execution Times

Note¹: Add `2(1/0)` to `(bd,An,Xi∗SF)` for memory indirect.

| MOVES Function | Size | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xi∗SF) | (bd,An,Xi∗SF)¹ | (xxx).WL |
|---|---|---|---|---|---|---|---|---|
| Source\<SFC\> → Rn | Byte, Word | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) | 2(1/0) | 3(1/0) | 2(1/0) |
| Source\<SFC\> → Rn | Long | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) | 2(1/0) | 3(1/0) | 2(1/0) |
| Rn → Dest\<DFC\> | Byte, Word | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 3(0/1) | 2(0/1) |
| Rn → Dest\<DFC\> | Long | 1(0/1) | 1(0/1) | 1(0/1) | 1(0/1) | 2(0/1) | 3(0/1) | 2(0/1) |

### Table 10-24. Miscellaneous Instruction Execution Times

Note¹: Add EA calculation time. Note²: CPUSH operand write refers to line-sized transfers.

| Instruction | Size | Register | Memory | Reg → Dest | Source → Reg |
|---|---|---|---|---|---|
| ANDI to CCR | Byte | 1(0/0) | — | — | — |
| CHK | Word | 2(0/0) | 2(1/0)¹ | — | — |
| CHK | Long | 2(0/0) | 2(1/0)¹ | — | — |
| CINVA | — | — | <=17(0/0) | — | — |
| CINVL | — | — | <=18(0/0) | — | — |
| CINVP | — | — | <=274(0/0) | — | — |
| CPUSHA | — | — | <=5394(0/512)² | — | — |
| CPUSHL | — | — | <=26(0/1)² | — | — |
| CPUSHP | — | — | <=2838(0/256)² | — | — |
| EORI to CCR | Byte | 1(0/0) | — | — | — |
| EXG | Long | 1(0/0) | — | — | — |
| EXT | Word | 1(0/0) | — | — | — |
| EXT | Long | 1(0/0) | — | — | — |
| EXTB | Long | 1(0/0) | — | — | — |
| LINK | Word | 2(0/1) | — | — | — |
| LINK | Long | 2(0/1) | — | — | — |
| LPSTOP | Word | 15(0/1) | — | — | — |
| MOVE from CCR | Word | 1(0/0) | 1(0/1)¹ | — | — |
| MOVE to CCR | Word | 1(0/0) | 1(1/0)¹ | — | — |
| MOVE from USP | Long | 1(0/0) | — | — | — |
| MOVE to USP | Long | 2(0/0) | — | — | — |
| MOVEC (SFC,DFC,USP,VBR,PCR) | Long | — | — | 12(0/0) | 11(0/0) |
| MOVEC (CACR,TC,TTR,BUSCR,URP,SRP) | Long | — | — | 15(0/0) | 14(0/0) |
| NOP | — | 9(0/0) | — | — | — |
| ORI to CCR | Byte | 1(0/0) | — | — | — |
| PACK | — | 2(0/0) | 2(1/1) | — | — |
| PFLUSH | — | 18(0/0) | — | — | — |
| PFLUSHN | — | 18(0/0) | — | — | — |
| PFLUSHAN | — | 33(0/0) | — | — | — |
| PFLUSHA | — | 33(0/0) | — | — | — |
| PLPA (ATC hit) | — | 15(0/0) | — | — | — |
| PLPA (ATC miss) | — | 28(0/0) | — | — | — |
| RESET | — | 520(0/0) | — | — | — |
| STOP | Word | 8(0/0) | — | — | — |
| SWAP | Word | 1(0/0) | — | — | — |
| TRAPF | — | 1(0/0) | — | — | — |
| TRAPcc | — | 1(0/0) | — | — | — |
| TRAPV | — | 1(0/0) | — | — | — |
| UNLK | — | 1(1/0) | — | — | — |
| UNPK | — | 2(0/0) | 2(1/1) | — | — |

---

## 10.15 FPU Instruction Execution Times

**General notes for all FPU instructions:**
- If external operand format is **byte, word, or long**: add **3 cycles**
- If external operand format is **extended precision** (except FMOVEM): add **2 cycles**
- Add `2(1/0)` to `(bd,An,Xi∗SF)` / `(bd,PC,Xi∗SF)` times for memory indirect
- Add `1(0/0)` cycle if `<ea>` specifies a double precision immediate operand
- `n` = number of FP registers being moved

### Table 10-25. Floating-Point Instruction Execution Times

| Instruction | FPn | Dn | (An) | (An)+ | –(An) | (d16,An) / (d16,PC) / (d8,An,Xi∗SF) / (d8,PC,Xi∗SF) | (bd,An,Xi∗SF) / (bd,PC,Xi∗SF) | (xxx).WL | #\<imm\> |
|---|---|---|---|---|---|---|---|---|---|
| FABS | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FDABS | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FSABS | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FADD | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FDADD | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FSADD | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FCMP | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FDIV | 37(0/0) | 39(0/0) | 37(1/0) | 37(1/0) | 37(1/0) | 37(1/0) / 38(1/0) | 39(1/0) | 38(1/0) | 38(0/0) |
| FDDIV | 37(0/0) | 39(0/0) | 37(1/0) | 37(1/0) | 37(1/0) | 37(1/0) / 38(1/0) | 39(1/0) | 38(1/0) | 38(0/0) |
| FSDIV | 37(0/0) | 39(0/0) | 37(1/0) | 37(1/0) | 37(1/0) | 37(1/0) / 38(1/0) | 39(1/0) | 38(1/0) | 38(0/0) |
| FMOVE \<ea\>,FPx | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 1(0/0) |
| FDMOVE \<ea\>,FPx | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 1(0/0) |
| FSMOVE \<ea\>,FPx | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 1(0/0) |
| FMOVE FPy,\<ea\> | — | 3(0/0) | 1(0/1) | 1(0/1) | 1(0/1) | 1(1/0) / 2(0/1) | 3(0/1) | 2(0/1) | — |
| FMOVE \<ea\>,FPCR | — | 8(0/0) | 6(1/0) | 6(1/0) | 6(1/0) | 6(1/0) / 7(1/0) | 8(1/0) | 7(1/0) | 7(0/0) |
| FMOVE FPCR,\<ea\> | — | 4(0/0) | 2(0/1) | 2(0/1) | 2(0/1) | 2(1/0) / 3(0/1) | 4(0/1) | 3(0/1) | — |
| FINT | 3(0/0) | 4(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 3(1/0) | 5(1/0) | 3(1/0) | 3(0/0) |
| FINTRZ | 3(0/0) | 4(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 3(1/0) | 5(1/0) | 3(1/0) | 3(0/0) |
| FSGLDIV | 37(0/0) | 39(0/0) | 37(1/0) | 37(1/0) | 37(1/0) | 37(1/0) / 38(1/0) | 39(1/0) | 38(1/0) | 38(0/0) |
| FSGLMUL | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FMOVEM \<ea\>,FPx | — | — | 1+3n(3n/0) | 1+3n(3n/0) | — | 1+3n(3n/0) / 2+3n(3n/0) | 3+3n(3n/0) | 2+3n(3n/0) | — |
| FMOVEM FPy,\<ea\> | — | — | 1+3n(0/3n) | — | 1+3n(0/3n) | 1+3n(0/3n) / 2+3n(0/3n) | 3+3n(0/3n) | 2+3n(0/3n) | — |
| FMUL | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FDMUL | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FSMUL | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 4(0/0) |
| FNEG | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FDNEG | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FSNEG | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 2(0/0) |
| FSUB | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 3(0/0) |
| FDSUB | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 3(0/0) |
| FSSUB | 3(0/0) | 5(0/0) | 3(1/0) | 3(1/0) | 3(1/0) | 3(1/0) / 4(1/0) | 5(1/0) | 4(1/0) | 3(0/0) |
| FTST | 1(0/0) | 3(0/0) | 1(1/0) | 1(1/0) | 1(1/0) | 1(1/0) / 2(1/0) | 3(1/0) | 2(1/0) | 1(0/0) |
| FSQRT | 68(0/0) | 70(0/0) | 68(1/0) | 68(1/0) | 68(1/0) | 68(1/0) / 69(1/0) | 70(1/0) | 69(1/0) | 69(0/0) |
| FSSQRT | 68(0/0) | 70(0/0) | 68(1/0) | 68(1/0) | 68(1/0) | 68(1/0) / 69(1/0) | 70(1/0) | 69(1/0) | 69(0/0) |
| FDSQRT | 68(0/0) | 70(0/0) | 68(1/0) | 68(1/0) | 68(1/0) | 68(1/0) / 69(1/0) | 70(1/0) | 69(1/0) | 69(0/0) |
| FSAVE | — | — | 3(0/3) | — | — | — | — | — | — |
| FRESTORE | — | — | 6(3/0) | — | — | — | — | — | — |
| FMOVEM \<ea\>,FPxR (register list) | — | — | 7(n/0) | — | — | — | — | — | — |
| FMOVEM FPxR,\<ea\> (register list) | — | — | 5(0/n) | — | — | — | — | — | — |

---

## 10.16 Exception Processing Times

Timing includes: OEP time for faulting instruction + exception frame stacking + vector fetch + first instruction fetch of handler.

### Table 10-26. Exception Processing Times

| Exception | Execution Time |
|---|---|
| CPU Reset¹ | 45(2/0) |
| Bus Error | 19(1/4) |
| Address Error | 19(1/3) |
| Illegal Instruction | 19(1/2) |
| Integer Divide By Zero² | 20(1/3) |
| CHK Instruction² | 20(1/3) |
| TRAPV, TRAPcc Instructions | 19(1/3) |
| Privilege Violation | 19(1/2) |
| Trace | 19(1/3) |
| Line A Emulator | 19(1/2) |
| Line F Emulator | 19(1/2) |
| Unimplemented EA | 19(1/2) |
| Unimplemented Integer | 19(1/2) |
| Format Error | 23(1/2) |
| Nonsupported FP | 19(1/3) |
| Interrupt³ | 23(1/2) |
| TRAP Instructions | 19(1/2) |
| FP Branch on Unordered Condition | 21(1/3) |
| FP Inexact Result⁴ | 19(1/3) |
| FP Divide By Zero⁴ | 19(1/3) |
| FP Underflow⁴ | 19(1/3) |
| FP Operand Error⁴ | 19(1/3) |
| FP Overflow⁴ | 19(1/3) |
| FP Signaling NAN⁴ | 19(1/3) |
| FP Unimplemented Data Type | 19(1/3) |

Notes:
1. Time from RSTI negated until first instruction enters OEP
2. Add EA calculation time
3. Assumes autovector or external vector with zero wait states
4. Add instruction execution time minus 1 if a post-exception fault occurs

---

## Appendix: Superscalar Classification Tables

### Table 10-2. MC68060 Integer Instruction Superscalar Classification

| Mnemonic | Instruction | Classification |
|---|---|---|
| ABCD | Add Decimal with Extend | pOEP-only |
| ADD | Add | pOEP \| sOEP |
| ADDA | Add Address | pOEP \| sOEP |
| ADDI,Dx | Add Immediate | pOEP \| sOEP |
| ADDI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining ADDI | " | pOEP-until-last |
| ADDQ | Add Quick | pOEP \| sOEP |
| ADDX | Add Extended | pOEP-only |
| AND | AND Logical | pOEP \| sOEP |
| ANDI,Dx | AND Immediate | pOEP \| sOEP |
| ANDI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining ANDI | " | pOEP-until-last |
| ANDI to CCR | AND Immediate to CCR | pOEP-only |
| ASL | Arithmetic Shift Left | pOEP \| sOEP |
| ASR | Arithmetic Shift Right | pOEP \| sOEP |
| Bcc | Branch Conditionally | pOEP-only¹ |
| BCHG Dy, | Test a Bit and Change | pOEP-only |
| BCHG #\<imm\>, | " | pOEP-until-last |
| BCLR Dy, | Test a Bit and Clear | pOEP-only |
| BCLR #\<imm\>, | " | pOEP-until-last |
| BFCHG | Test Bit Field and Change | pOEP-only |
| BFCLR | Test Bit Field and Clear | pOEP-only |
| BFEXTS | Extract Bit Field Signed | pOEP-only |
| BFEXTU | Extract Bit Field Unsigned | pOEP-only |
| BFFFO | Find First One in Bit Field | pOEP-only |
| BFINS | Insert Bit Field | pOEP-only |
| BFSET | Set Bit Field | pOEP-only |
| BFTST | Test Bit Field | pOEP-only |
| BKPT | Breakpoint | pOEP-only |
| BRA | Branch Always | pOEP-only |
| BSET Dy, | Test a Bit and Set | pOEP-only |
| BSET #\<imm\>, | " | pOEP-until-last |
| BSR | Branch to Subroutine | pOEP-only |
| BTST Dy, | Test a Bit | pOEP-only |
| BTST #\<imm\>, | " | pOEP-until-last |
| CAS | Compare and Swap with Operand | pOEP-only |
| CHK | Check Register Against Bounds | pOEP-only |
| CLR | Clear an Operand | pOEP \| sOEP |
| CMP | Compare | pOEP \| sOEP |
| CMPA | Compare Address | pOEP \| sOEP |
| CMPI,Dx | Compare Immediate | pOEP \| sOEP |
| CMPI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining CMPI | " | pOEP-until-last |
| CMPM | Compare Memory | pOEP-until-last |
| DBcc | Test Condition, Decrement and Branch | pOEP-only |
| DIVS.L | Signed Divide Long | pOEP-only |
| DIVS.W | Signed Divide Word | pOEP-only |
| DIVU.L | Unsigned Long Divide | pOEP-only |
| DIVU.W | Unsigned Divide Word | pOEP-only |
| EOR | Exclusive OR Logical | pOEP \| sOEP |
| EORI,Dx | Exclusive OR Immediate | pOEP \| sOEP |
| EORI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining EORI | " | pOEP-until-last |
| EORI to CCR | EOR Immediate to CCR | pOEP-only |
| EXG | Exchange Registers | pOEP-only |
| EXT | Sign Extend | pOEP \| sOEP |
| EXTB.L | Sign Extend Byte to Long | pOEP \| sOEP |
| ILLEGAL | Take Illegal Instruction Trap | pOEP \| sOEP |
| JMP | Jump | pOEP-only |
| JSR | Jump to Subroutine | pOEP-only |
| LEA | Load Effective Address | pOEP \| sOEP |
| LINK | Link and Allocate | pOEP-until-last |
| LSL | Logical Shift Left | pOEP \| sOEP |
| LSR | Logical Shift Right | pOEP \| sOEP |
| MOVE,Rx | Move Data from Source to Destination | pOEP \| sOEP |
| MOVE Ry, | " | pOEP \| sOEP |
| MOVE \<mem\>y,\<mem\>x | " | pOEP-until-last |
| MOVE #\<imm\>,\<mem\>x | " | pOEP-until-last |
| MOVEA | Move Address | pOEP \| sOEP |
| MOVE from CCR | Move from Condition Codes | pOEP-only |
| MOVE to CCR | Move to Condition Codes | pOEP \| sOEP |
| MOVE16 | Move 16 Byte Block | pOEP-only |
| MOVEM | Move Multiple Registers | pOEP-only |
| MOVEQ | Move Quick | pOEP \| sOEP |
| MULS.L | Signed Multiply Long | pOEP-only |
| MULS.W | Signed Multiply Word | pOEP-only |
| MULU.L | Unsigned Multiply Long | pOEP-only |
| MULU.W | Unsigned Multiply Word | pOEP-only |
| NBCD | Negate Decimal with Extend | pOEP-only |
| NEG | Negate | pOEP \| sOEP |
| NEGX | Negate with Extend | pOEP-only |
| NOP | No Operation | pOEP-only |
| NOT | Logical Complement | pOEP \| sOEP |
| OR | Inclusive OR Logical | pOEP \| sOEP |
| ORI,Dx | Inclusive OR Immediate | pOEP \| sOEP |
| ORI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining ORI | " | pOEP-until-last |
| ORI to CCR | OR Immediate to CCR | pOEP-only |
| PACK | Pack BCD Digit | pOEP-only |
| PEA | Push Effective Address | pOEP-only |
| ROL | Rotate without Extend Left | pOEP \| sOEP |
| ROR | Rotate without Extend Right | pOEP \| sOEP |
| ROXL | Rotate with Extend Left | pOEP-only |
| ROXR | Rotate with Extend Right | pOEP-only |
| RTD | Return and Deallocate Parameters | pOEP-only |
| RTR | Return and Restore Condition Codes | pOEP-only |
| RTS | Return from Subroutine | pOEP-only |
| SBCD | Subtract Decimal with Extend | pOEP-only |
| Scc | Set According to Condition | pOEP-but-allows-sOEP |
| SUB | Subtract | pOEP \| sOEP |
| SUBA | Subtract Address | pOEP \| sOEP |
| SUBI,Dx | Subtract Immediate | pOEP \| sOEP |
| SUBI,–(Ax)+ | " | pOEP \| sOEP |
| Remaining SUBI | " | pOEP-until-last |
| SUBQ | Subtract Quick | pOEP \| sOEP |
| SUBX | Subtract with Extend | pOEP-only |
| SWAP | Swap Register Halves | pOEP-only |
| TAS | Test and Set an Operand | pOEP-only |
| TRAP | Trap | pOEP \| sOEP |
| TRAPF | Trap on False | pOEP \| sOEP |
| remaining TRAPcc | Trap on Condition | pOEP-only |
| TRAPV | Trap on Overflow | pOEP-only |
| TST | Test an Operand | pOEP \| sOEP |
| UNLK | Unlink | pOEP-only |
| UNPK | Unpack BCD Digit | pOEP-only |

Note¹: Bcc is `pOEP-but-allows-sOEP` if: not predicted from branch cache and forward direction, OR predicted as not-taken.

### Table 10-3. Privileged Instruction Superscalar Classification

All privileged instructions are **pOEP-only**: ANDI to SR, CINV, CPUSH, EORI to SR, MOVE from SR, MOVE to SR, MOVE USP, MOVEC, MOVES, ORI to SR, PFLUSH, PLPA, RESET, RTE, STOP.

### Table 10-4. Floating-Point Instruction Superscalar Classification

Note¹: The following variants are **pOEP-only** (not pOEP-but-allows-sOEP): `F<op>Dm,FPn`, `F<op>&imm,FPn`, `F<op>.x<mem>,FPn`

| Mnemonic | Classification |
|---|---|
| FABS, FDABS, FSABS | pOEP-but-allows-sOEP¹ |
| FADD, FDADD, FSADD | pOEP-but-allows-sOEP¹ |
| FBcc | pOEP-only |
| FCMP | pOEP-but-allows-sOEP¹ |
| FDIV, FDDIV, FSDIV, FSGLDIV | pOEP-but-allows-sOEP¹ |
| FINT, FINTRZ | pOEP-but-allows-sOEP¹ |
| FMOVE, FDMOVE, FSMOVE (data reg) | pOEP-but-allows-sOEP¹ |
| FMOVE (system control register) | pOEP-only |
| FMOVEM | pOEP-only |
| FMUL, FDMUL, FSMUL, FSGLMUL | pOEP-but-allows-sOEP¹ |
| FNEG, FDNEG, FSNEG | pOEP-but-allows-sOEP¹ |
| FNOP | pOEP-only |
| FSQRT, FSSQRT, FDSQRT | pOEP-but-allows-sOEP¹ |
| FSUB, FDSUB, FSSUB | pOEP-but-allows-sOEP¹ |
| FTST | pOEP-but-allows-sOEP¹ |
