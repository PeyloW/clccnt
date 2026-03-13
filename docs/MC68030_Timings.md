# MC68030 Instruction Timing Tables

Source: MC68030 User's Manual, Section 11.6

## Assumptions

All timings assume:
- Two-clock bus cycles, no wait states
- All memory operands long-word aligned
- 32-bit bus
- Data cache **not** enabled
- No exceptions (unless specified)
- Required ATC translations resident

## How to Read the Tables

Each entry shows: `Total (reads / prefetches / writes)`

- **I-Cache Case (CC):** Instruction is in cache, no overlap
- **No-Cache Case (NCC):** Instruction not in cache, cache disabled, no overlap

The columns used most often for bare-metal 68000-family work without a cache are the **NCC** values.

---

## 11.6.1 Fetch Effective Address (fea)

Added to instructions that dereference a source operand.

### Single EA Format

| Address Mode       | I-Cache | No-Cache |
|--------------------|---------|----------|
| Dn                 | 0(0/0/0) | 0(0/0/0) |
| An                 | 0(0/0/0) | 0(0/0/0) |
| (An)               | 3(1/0/0) | 3(1/0/0) |
| (An)+              | 3(1/0/0) | 3(1/0/0) |
| -(An)              | 4(1/0/0) | 4(1/0/0) |
| (d16,An) or (d16,PC) | 4(1/0/0) | 4(1/1/0) |
| (xxx).W            | 4(1/0/0) | 4(1/1/0) |
| (xxx).L            | 4(1/0/0) | 5(1/1/0) |
| #data.B            | 2(0/0/0) | 2(0/1/0) |
| #data.W            | 2(0/0/0) | 2(0/1/0) |
| #data.L            | 4(0/0/0) | 4(0/1/0) |

### Brief Format Extension Word

| Address Mode               | I-Cache | No-Cache |
|----------------------------|---------|----------|
| (d8,An,Xn) or (d8,PC,Xn)  | 6(1/0/0) | 6(1/1/0) |

### Full Format Extension Words

| Address Mode                          | I-Cache   | No-Cache  |
|---------------------------------------|-----------|-----------|
| (d16,An) or (d16,PC)                  | 6(1/0/0)  | 7(1/1/0)  |
| (d16,An,Xn) or (d16,PC,Xn)           | 6(1/0/0)  | 7(1/1/0)  |
| ([d16,An]) or ([d16,PC])              | 10(2/0/0) | 10(2/1/0) |
| ([d16,An],Xn) or ([d16,PC],Xn)       | 10(2/0/0) | 10(2/1/0) |
| ([d16,An],d16) or ([d16,PC],d16)      | 12(2/0/0) | 13(2/2/0) |
| ([d16,An],Xn,d16) or ([d16,PC],Xn,d16)| 12(2/0/0)| 13(2/2/0) |
| ([d16,An],d32) or ([d16,PC],d32)      | 12(2/0/0) | 14(2/2/0) |
| ([d16,An],Xn,d32) or ([d16,PC],Xn,d32)| 12(2/0/0)| 14(2/2/0) |
| (B)                                   | 6(1/0/0)  | 7(1/1/0)  |
| (d16,B)                               | 8(1/0/0)  | 10(1/1/0) |
| (d32,B)                               | 12(1/0/0) | 13(1/2/0) |
| ([B])                                 | 10(2/0/0) | 10(2/1/0) |
| ([B],I)                               | 10(2/0/0) | 10(2/1/0) |
| ([B],d16)                             | 12(2/0/0) | 13(2/1/0) |
| ([B],I,d16)                           | 12(2/0/0) | 13(2/1/0) |
| ([B],d32)                             | 12(2/0/0) | 14(2/2/0) |
| ([B],I,d32)                           | 12(2/0/0) | 14(2/2/0) |
| ([d16,B])                             | 12(2/0/0) | 13(2/1/0) |
| ([d16,B],I)                           | 12(2/0/0) | 13(2/1/0) |
| ([d16,B],d16)                         | 14(2/0/0) | 16(2/2/0) |
| ([d16,B],I,d16)                       | 14(2/0/0) | 16(2/2/0) |
| ([d16,B],d32)                         | 14(2/0/0) | 17(2/2/0) |
| ([d16,B],I,d32)                       | 14(2/0/0) | 17(2/2/0) |
| ([d32,B])                             | 16(2/0/0) | 17(2/2/0) |
| ([d32,B],I)                           | 16(2/0/0) | 17(2/2/0) |
| ([d32,B],d16)                         | 18(2/0/0) | 20(2/2/0) |
| ([d32,B],I,d16)                       | 18(2/0/0) | 20(2/2/0) |
| ([d32,B],d32)                         | 18(2/0/0) | 21(2/3/0) |
| ([d32,B],I,d32)                       | 18(2/0/0) | 21(2/3/0) |

> B = Base: 0, An, PC, Xn, An+Xn, PC+Xn. I = Index: 0, Xn. Xn cannot appear in both B and I. Scaling and size of Xn do not affect timing.

---

## 11.6.2 Fetch Immediate Effective Address (fiea)

Used for instructions with an immediate source and a destination EA.

### Single EA Format

| Address Mode              | I-Cache   | No-Cache  |
|---------------------------|-----------|-----------|
| % #data.W, Dn             | 2(0/0/0)  | 2(0/1/0)  |
| % #data.L, Dn             | 4(0/0/0)  | 4(0/1/0)  |
| #data.W, (An)             | 3(1/0/0)  | 4(1/1/0)  |
| #data.L, (An)             | 4(1/0/0)  | 5(1/1/0)  |
| #data.W, (An)+            | 5(1/0/0)  | 5(1/1/0)  |
| #data.L, (An)+            | 7(1/0/0)  | 7(1/1/0)  |
| #data.W, -(An)            | 4(1/0/0)  | 4(1/1/0)  |
| #data.L, -(An)            | 4(1/0/0)  | 6(1/1/0)  |
| #data.W, (d16,An)         | 4(1/0/0)  | 5(1/1/0)  |
| #data.L, (d16,An)         | 6(1/0/0)  | 8(1/2/0)  |
| #data.W, $xxx.W           | 6(1/0/0)  | 6(1/1/0)  |
| #data.L, $xxx.W           | 8(1/0/0)  | 8(1/2/0)  |
| #data.W, $xxx.L           | 6(1/0/0)  | 7(1/2/0)  |
| #data.L, $xxx.L           | 8(1/0/0)  | 9(1/2/0)  |
| #data.W, #data.L          | 6(0/0/0)  | 6(0/2/0)  |

### Brief Format Extension Word

| Address Mode                          | I-Cache   | No-Cache  |
|---------------------------------------|-----------|-----------|
| #data.W, (d8,An,Xn) or (d8,PC,Xn)   | 8(1/0/0)  | 8(1/2/0)  |
| #data.L, (d8,An,Xn) or (d8,PC,Xn)   | 10(1/0/0) | 10(1/2/0) |

> % = Total head for fiea includes the head time for the operation.
> Full-format extension word entries omitted for brevity; follow same pattern as fea with added immediate fetch overhead.

---

## 11.6.6 MOVE Instruction

### Single EA Destination Format

| Source, Destination   | I-Cache  | No-Cache |
|-----------------------|----------|----------|
| MOVE Rn, Dn           | 2(0/0/0) | 2(0/1/0) |
| MOVE Rn, An           | 2(0/0/0) | 2(0/1/0) |
| * MOVE EA, An         | 2(0/0/0) | 2(0/1/0) |
| * MOVE EA, Dn         | 2(0/0/0) | 2(0/1/0) |
| MOVE Rn, (An)         | 3(0/0/1) | 4(0/1/1) |
| * MOVE SRC, (An)      | 4(0/0/1) | 5(0/1/1) |
| MOVE Rn, (An)+        | 3(0/0/1) | 4(0/1/1) |
| * MOVE SRC, (An)+     | 4(0/0/1) | 5(0/1/1) |
| MOVE Rn, -(An)        | 4(0/0/1) | 4(0/1/1) |
| * MOVE SRC, -(An)     | 4(0/0/1) | 5(0/1/1) |
| * MOVE EA, (d16,An)   | 4(0/0/1) | 5(0/1/1) |
| * MOVE EA, xxx.W      | 4(0/0/1) | 5(0/1/1) |
| * MOVE EA, xxx.L      | 6(0/0/1) | 7(0/2/1) |

### Brief Format Extension Word

| Source, Destination        | I-Cache  | No-Cache |
|----------------------------|----------|----------|
| * MOVE EA, (d8,An,Xn)      | 6(0/0/1) | 7(0/1/1) |

> * = Add Fetch Effective Address Time. Rn = Data or Address Register. SRC = Memory or Immediate. EA = any Effective Address.

---

## 11.6.7 Special-Purpose MOVE Instructions

| Instruction          | I-Cache   | No-Cache  |
|----------------------|-----------|-----------|
| EXG Ry, Rx           | 4(0/0/0)  | 4(0/1/0)  |
| MOVEC Cr, Rn         | 6(0/0/0)  | 6(0/1/0)  |
| MOVEC Rn, Cr-A       | 6(0/0/0)  | 6(0/1/0)  |
| MOVEC Rn, Cr-B       | 12(0/0/0) | 12(0/1/0) |
| MOVE CCR, Dn         | 4(0/0/0)  | 4(0/1/0)  |
| * MOVE CCR, Mem      | 4(0/0/1)  | 5(0/1/1)  |
| MOVE Dn, CCR         | 4(0/0/0)  | 4(0/1/0)  |
| * MOVE EA, CCR       | 4(0/0/0)  | 4(0/1/0)  |
| MOVE SR, Dn          | 4(0/0/0)  | 4(0/1/0)  |
| * MOVE SR, Mem       | 4(0/0/1)  | 5(0/1/1)  |
| # MOVE EA, SR        | 8(0/0/0)  | 10(0/2/0) |
| % MOVEM EA, RL (n regs) | 8+4n(n/0/0) | 8+4n(n/1/0) |
| % MOVEM RL, EA (n regs) | 4+2n(0/0/n) | 4+2n(0/1/n) |
| MOVEP.W Dn, (d16,An) | 10(0/0/2) | 10(0/1/2) |
| MOVEP.W (d16,An), Dn | 10(2/0/0) | 10(2/1/0) |
| MOVEP.L Dn, (d16,An) | 14(0/0/4) | 14(0/1/4) |
| MOVEP.L (d16,An), Dn | 14(4/0/0) | 14(4/1/0) |
| % MOVES EA, Rn       | 7(1/0/0)  | 7(1/1/0)  |
| % MOVES Rn, EA       | 5(0/0/1)  | 6(0/1/1)  |
| MOVE USP, An         | 4(0/0/0)  | 4(0/1/0)  |
| MOVE An, USP         | 4(0/0/0)  | 4(0/1/0)  |
| SWAP Dn              | 4(0/0/0)  | 4(0/1/0)  |

> Cr-A: USP, VBR, CAAR, MSP, ISP. Cr-B: SFC, DFC, CACR.
> MOVEM EA,RL with wait states w: CC = (8+4n) if w≤2, else (8+4n)+(w-2)n. Tail=0 always.
> MOVEM RL,EA with wait states w: CC = (4+2n)+(n-1)w if w≤2, else (4+2n)+(n-1)w+(w-2).

---

## 11.6.8 Arithmetical/Logical Instructions

| Instruction        | I-Cache  | No-Cache |
|--------------------|----------|----------|
| ADD Rn, Dn         | 2(0/0/0) | 2(0/1/0) |
| ADDA.W Rn, An      | 4(0/0/0) | 4(0/1/0) |
| ADDA.L Rn, An      | 2(0/0/0) | 2(0/1/0) |
| * ADD EA, Dn       | 2(0/0/0) | 2(0/1/0) |
| * ADD.W EA, An     | 4(0/0/0) | 4(0/1/0) |
| * ADDA.L EA, An    | 2(0/0/0) | 2(0/1/0) |
| * ADD Dn, EA       | 3(0/0/1) | 4(0/1/1) |
| AND Dn, Dn         | 2(0/0/0) | 2(0/1/0) |
| * AND EA, Dn       | 2(0/0/0) | 2(0/1/0) |
| * AND Dn, EA       | 3(0/0/1) | 4(0/1/1) |
| EOR Dn, Dn         | 2(0/0/0) | 2(0/1/0) |
| * EOR Dn, EA       | 3(0/0/1) | 4(0/1/1) |
| OR Dn, Dn          | 2(0/0/0) | 2(0/1/0) |
| * OR EA, Dn        | 2(0/0/0) | 2(0/1/0) |
| * OR Dn, EA        | 3(0/0/1) | 4(0/1/1) |
| SUB Rn, Dn         | 2(0/0/0) | 2(0/1/0) |
| * SUB EA, Dn       | 2(0/0/0) | 2(0/1/0) |
| * SUB Dn, EA       | 3(0/0/1) | 4(0/1/1) |
| SUBA.W Rn, An      | 4(0/0/0) | 4(0/1/0) |
| SUBA.L Rn, An      | 2(0/0/0) | 2(0/1/0) |
| * SUBA.W EA, An    | 4(0/0/0) | 4(0/1/0) |
| * SUBA.L EA, An    | 2(0/0/0) | 2(0/1/0) |
| CMP Rn, Dn         | 2(0/0/0) | 2(0/1/0) |
| * CMP EA, Dn       | 2(0/0/0) | 2(0/1/0) |
| CMPA Rn, An        | 4(0/0/0) | 4(0/1/0) |
| * CMPA EA, An      | 4(0/0/0) | 4(0/1/0) |
| ** + CMP2 EA, Rn   | 20(1/0/0)| 20(1/1/0)|
| * + MULS.W EA, Dn  | 28(0/0/0)| 28(0/1/0)|
| ** + MULS.L EA, Dn | 44(0/0/0)| 44(0/1/0)|
| * + MULU.W EA, Dn  | 28(0/0/0)| 28(0/1/0)|
| ** + MULU.L EA, Dn | 44(0/0/0)| 44(0/1/0)|
| + DIVS.W Dn, Dn    | 56(0/0/0)| 56(0/1/0)|
| * + DIVS.W EA, Dn  | 56(0/0/0)| 56(0/1/0)|
| ** + DIVS.L Dn, Dn | 90(0/0/0)| 90(0/1/0)|
| ** + DIVS.L EA, Dn | 90(0/0/0)| 90(0/1/0)|
| + DIVU.W Dn, Dn    | 44(0/0/0)| 44(0/1/0)|
| * + DIVU.W EA, Dn  | 44(0/0/0)| 44(0/1/0)|
| ** + DIVU.L Dn, Dn | 78(0/0/0)| 78(0/1/0)|
| ** + DIVU.L EA, Dn | 78(0/0/0)| 78(0/1/0)|

> \* Add fea time. \*\* Add fiea time. + = Maximum (actual is data-dependent).

---

## 11.6.9 Immediate Arithmetical/Logical Instructions

The `%` entries include the immediate fetch in their head; add **fiea** time for non-`%` entries.

| Instruction           | I-Cache  | No-Cache |
|-----------------------|----------|----------|
| MOVEQ #data, Dn       | 2(0/0/0) | 2(0/1/0) |
| ADDQ #data, Rn        | 2(0/0/0) | 2(0/1/0) |
| * ADDQ #data, Mem     | 3(0/0/1) | 4(0/1/1) |
| SUBQ #data, Rn        | 2(0/0/0) | 2(0/1/0) |
| * SUBQ #data, Mem     | 3(0/0/1) | 4(0/1/1) |
| ** ADDI #data, Dn     | 2(0/0/0) | 2(0/1/0) |
| ** ADDI #data, Mem    | 3(0/0/1) | 4(0/1/1) |
| ** ANDI #data, Dn     | 2(0/0/0) | 2(0/1/0) |
| ** ANDI #data, Mem    | 3(0/0/1) | 4(0/1/1) |
| ** EORI #data, Dn     | 2(0/0/0) | 2(0/1/0) |
| ** EORI #data, Mem    | 3(0/0/1) | 4(0/1/1) |
| ** ORI #data, Dn      | 2(0/0/0) | 2(0/1/0) |
| ** ORI #data, Mem     | 3(0/0/1) | 4(0/1/1) |
| ** SUBI #data, Dn     | 2(0/0/0) | 2(0/1/0) |
| ** SUBI #data, Mem    | 3(0/0/1) | 4(0/1/1) |
| ** CMPI #data, Dn     | 2(0/0/0) | 2(0/1/0) |
| ** CMPI #data, Mem    | 2(0/0/0) | 2(0/1/0) |

> \* Add fea time. \*\* Add fiea time.
>
> **Key point for ANDI/ORI/EORI to Dn:** The base op cost is 2 cycles. The fiea overhead for `#data.L,Dn` is 4 cycles (NCC), giving a total of **6 cycles** for `ANDI.L #imm,Dn`. For `#data.W,Dn` the fiea overhead is 2 cycles, giving **4 cycles** total.

---

## 11.6.10 BCD and Extended Instructions

| Instruction                  | I-Cache   | No-Cache  |
|------------------------------|-----------|-----------|
| ABCD Dn, Dn                  | 4(0/0/0)  | 4(0/1/0)  |
| ABCD -(An), -(An)            | 13(2/0/1) | 14(2/1/1) |
| SBCD Dn, Dn                  | 4(0/0/0)  | 4(0/1/0)  |
| SBCD -(An), -(An)            | 13(2/0/1) | 14(2/1/1) |
| ADDX Dn, Dn                  | 2(0/0/0)  | 2(0/1/0)  |
| ADDX -(An), -(An)            | 9(2/0/1)  | 10(2/1/1) |
| SUBX Dn, Dn                  | 2(0/0/0)  | 2(0/1/0)  |
| SUBX -(An), -(An)            | 9(2/0/1)  | 10(2/1/1) |
| CMPM (An)+, (An)+            | 8(2/0/0)  | 8(2/1/0)  |
| PACK Dn, Dn, #data           | 6(0/0/0)  | 6(0/1/0)  |
| PACK -(An), -(An), #data     | 11(1/0/1) | 11(1/1/1) |
| UNPK Dn, Dn, #data           | 8(0/0/0)  | 8(0/1/0)  |
| UNPK -(An), -(An), #data     | 11(1/0/1) | 11(1/1/1) |

---

## 11.6.11 Single Operand Instructions

| Instruction      | I-Cache  | No-Cache |
|------------------|----------|----------|
| CLR Dn           | 2(0/0/0) | 2(0/1/0) |
| ** CLR Mem       | 3(0/0/1) | 4(0/1/1) |
| NEG Dn           | 2(0/0/0) | 2(0/1/0) |
| * NEG Mem        | 3(0/0/1) | 4(0/1/1) |
| NEGX Dn          | 2(0/0/0) | 2(0/1/0) |
| * NEGX Mem       | 3(0/0/1) | 4(0/1/1) |
| NOT Dn           | 2(0/0/0) | 2(0/1/0) |
| * NOT Mem        | 3(0/0/1) | 4(0/1/1) |
| EXT Dn           | 4(0/0/0) | 4(0/1/0) |
| NBCD Dn          | 6(0/0/0) | 6(0/1/0) |
| Scc Dn           | 4(0/0/0) | 4(0/1/0) |
| ** Scc Mem       | 5(0/0/1) | 5(0/1/1) |
| TAS Dn           | 4(0/0/0) | 4(0/1/0) |
| ** TAS Mem       | 12(1/0/1)| 12(1/1/1)|
| TST Dn           | 2(0/0/0) | 2(0/1/0) |
| * TST Mem        | 2(0/0/0) | 2(0/1/0) |

> \* Add fea time. \*\* Add cea time.

---

## 11.6.12 Shift/Rotate Instructions

The number of bits shifted does **not** affect execution time (except where noted).

| Instruction         | Notes                      | I-Cache  | No-Cache |
|---------------------|----------------------------|----------|----------|
| LSd #data, Dy       |                            | 4(0/0/0) | 4(0/1/0) |
| LSd Dx, Dy          | shift count ≤ data size    | 6(0/0/0) | 6(0/1/0) |
| LSd Dx, Dy          | shift count > data size    | 8(0/0/0) | 8(0/1/0) |
| * LSd Mem by 1      |                            | 4(0/0/1) | 4(0/1/1) |
| ASL #data, Dy       |                            | 6(0/0/0) | 6(0/1/0) |
| ASL Dx, Dy          |                            | 8(0/0/0) | 8(0/1/0) |
| * ASL Mem by 1      |                            | 6(0/0/1) | 6(0/1/1) |
| ASR #data, Dy       |                            | 4(0/0/0) | 4(0/1/0) |
| ASR Dx, Dy          | shift count ≤ data size    | 6(0/0/0) | 6(0/1/0) |
| ASR Dx, Dy          | shift count > data size    | 10(0/0/0)| 10(0/1/0)|
| * ASR Mem by 1      |                            | 4(0/0/1) | 4(0/1/1) |
| ROd #data, Dy       |                            | 6(0/0/0) | 6(0/1/0) |
| ROd Dx, Dy          |                            | 8(0/0/0) | 8(0/1/0) |
| * ROd Mem by 1      |                            | 6(0/0/1) | 6(0/1/1) |
| ROXd Dn             |                            | 12(0/0/0)| 12(0/1/0)|
| * ROXd Mem by 1     |                            | 4(0/0/0) | 4(0/1/0) |

> d = direction L or R. \* Add fea time.
>
> **Key asymmetry:** ASL #imm costs 6 cycles, ASR #imm costs only 4 cycles on the 68030.

---

## 11.6.13 Bit Manipulation Instructions

| Instruction         | I-Cache  | No-Cache |
|---------------------|----------|----------|
| BTST #data, Dn      | 4(0/0/0) | 4(0/1/0) |
| BTST Dn, Dn         | 4(0/0/0) | 4(0/1/0) |
| # BTST #data, Mem   | 4(0/0/0) | 4(0/1/0) |
| * BTST Dn, Mem      | 4(0/0/0) | 4(0/1/0) |
| BCHG #data, Dn      | 6(0/0/0) | 6(0/1/0) |
| BCHG Dn, Dn         | 6(0/0/0) | 6(0/1/0) |
| # BCHG #data, Mem   | 6(0/0/1) | 6(0/1/1) |
| * BCHG Dn, Mem      | 6(0/0/1) | 6(0/1/1) |
| BCLR #data, Dn      | 6(0/0/0) | 6(0/1/0) |
| BCLR Dn, Dn         | 6(0/0/0) | 6(0/1/0) |
| # BCLR #data, Mem   | 6(0/0/1) | 6(0/1/1) |
| * BCLR Dn, Mem      | 6(0/0/1) | 6(0/1/1) |
| BSET #data, Dn      | 6(0/0/0) | 6(0/1/0) |
| BSET Dn, Dn         | 6(0/0/0) | 6(0/1/0) |
| # BSET #data, Mem   | 6(0/0/1) | 6(0/1/1) |
| * BSET Dn, Mem      | 6(0/0/1) | 6(0/1/1) |

> \* Add fea time. # Add fiea time.

---

## 11.6.14 Bit Field Manipulation Instructions (68020+ only)

> **Note:** A bit field of 32 bits may span 5 bytes (two operand cycles) or 4 bytes (one operand cycle).

| Instruction           | Operand         | I-Cache   | No-Cache  |
|-----------------------|-----------------|-----------|-----------|
| BFTST Dn              |                 | 8(0/0/0)  | 8(0/1/0)  |
| * BFTST Mem           | < 5 bytes       | 10(1/0/0) | 10(1/1/0) |
| * BFTST Mem           | 5 bytes         | 14(2/0/0) | 14(2/1/0) |
| BFCHG Dn              |                 | 14(0/0/0) | 14(0/1/0) |
| * BFCHG Mem           | < 5 bytes       | 14(1/0/1) | 14(1/1/1) |
| * BFCHG Mem           | 5 bytes         | 22(2/0/2) | 22(2/1/2) |
| BFCLR Dn              |                 | 14(0/0/0) | 14(0/1/0) |
| * BFCLR Mem           | < 5 bytes       | 14(1/0/1) | 14(1/1/1) |
| * BFCLR Mem           | 5 bytes         | 22(2/0/2) | 22(2/1/2) |
| BFSET Dn              |                 | 14(0/0/0) | 14(0/1/0) |
| * BFSET Mem           | < 5 bytes       | 14(1/0/1) | 14(1/1/1) |
| * BFSET Mem           | 5 bytes         | 22(2/0/2) | 22(2/1/2) |
| BFEXTS Dn             |                 | 10(0/0/0) | 10(0/1/0) |
| * BFEXTS Mem          | < 5 bytes       | 12(1/0/0) | 12(1/1/0) |
| * BFEXTS Mem          | 5 bytes         | 18(2/0/0) | 18(2/1/0) |
| BFEXTU Dn             |                 | 10(0/0/0) | 10(0/1/0) |
| * BFEXTU Mem          | < 5 bytes       | 12(1/0/0) | 12(1/1/0) |
| * BFEXTU Mem          | 5 bytes         | 18(2/0/0) | 18(2/1/0) |
| BFINS Dn              |                 | 12(0/0/0) | 12(0/1/0) |
| * BFINS Mem           | < 5 bytes       | 12(1/0/1) | 12(1/1/1) |
| * BFINS Mem           | 5 bytes         | 18(2/0/2) | 18(2/1/2) |
| BFFFO Dn              |                 | 20(0/0/0) | 20(0/1/0) |
| * BFFFO Mem           | < 5 bytes       | 22(1/0/0) | 22(1/1/0) |
| * BFFFO Mem           | 5 bytes         | 28(2/0/0) | 28(2/1/0) |

> \* Add ciea (Calculate Immediate Effective Address) time.
>
> **Bit numbering:** BFEXTU/BFEXTS use MSB-relative offset. For a 32-bit register, offset 0 = bit 31, offset 16 = bit 15. To extract bits [5:2] (4 bits starting at bit 2): `BFEXTU Dn{#26:#4}, Dm`.

---

## 11.6.15 Conditional Branch Instructions

| Instruction                          | I-Cache   | No-Cache  |
|--------------------------------------|-----------|-----------|
| Bcc (Taken)                          | 6(0/0/0)  | 8(0/2/0)  |
| Bcc.B (Not Taken)                    | 4(0/0/0)  | 4(0/1/0)  |
| Bcc.W (Not Taken)                    | 6(0/0/0)  | 6(0/1/0)  |
| Bcc.L (Not Taken)                    | 6(0/0/0)  | 8(0/2/0)  |
| DBcc (cc=False, Count Not Expired)   | 6(0/0/0)  | 8(0/2/0)  |
| DBcc (cc=False, Count Expired)       | 10(0/0/0) | 13(0/3/0) |
| DBcc (cc=True)                       | 6(0/0/0)  | 8(0/1/0)  |

> **DBRA hot-path** (cc=False, not expired): **8 cycles NCC**. This is the cost when the loop continues.

---

## 11.6.16 Control Instructions

| Instruction              | I-Cache   | No-Cache  |
|--------------------------|-----------|-----------|
| ANDI to SR               | 12(0/0/0) | 14(0/2/0) |
| EORI to SR               | 12(0/0/0) | 14(0/2/0) |
| ORI to SR                | 12(0/0/0) | 14(0/2/0) |
| ANDI to CCR              | 12(0/0/0) | 14(0/2/0) |
| EORI to CCR              | 12(0/0/0) | 14(0/2/0) |
| ORI to CCR               | 12(0/0/0) | 14(0/2/0) |
| BSR                      | 6(0/0/1)  | 9(0/2/1)  |
| ## CAS (Successful)      | 13(1/0/1) | 13(1/1/1) |
| ## CAS (Unsuccessful)    | 11(1/0/0) | 11(1/1/0) |
| + CAS2 (Successful)      | 24(2/0/2) | 26(2/2/2) |
| + CAS2 (Unsuccessful)    | 24(2/0/0) | 24(2/2/0) |
| CHK Dn,Dn (No Exc.)      | 8(0/0/0)  | 8(0/1/0)  |
| + CHK Dn,Dn (Exc. Taken) | 28(1/0/4) | 30(1/3/4) |
| # + CHK2 Mem,Rn (No Exc.)| 18(1/0/0) | 18(1/1/0) |
| # + CHK2 Mem,Rn (Exc.)   | 40(2/0/4) | 42(2/3/4) |
| % JMP                    | 4(0/0/0)  | 6(0/2/0)  |
| % JSR                    | 4(0/0/1)  | 7(0/2/1)  |
| ** LEA                   | 2(0/0/0)  | 2(0/1/0)  |
| LINK.W                   | 4(0/0/1)  | 5(0/1/1)  |
| LINK.L                   | 6(0/0/1)  | 7(0/2/1)  |
| NOP                      | 2(0/0/0)  | 2(0/1/0)  |
| ** PEA                   | 4(0/0/1)  | 4(0/1/1)  |
| RTD                      | 10(1/0/0) | 12(1/2/0) |
| RTR                      | 12(2/0/0) | 14(2/2/0) |
| RTS                      | 9(1/0/0)  | 11(1/2/0) |
| UNLK                     | 5(1/0/0)  | 5(1/1/0)  |

> \* Add fea. \*\* Add cea. # Add fiea. ## Add ciea. % Add Jump EA time. + = Maximum.

---

## Quick Reference: Common Inner-Loop Instructions (NCC)

| Instruction                  | Cycles (NCC) | Notes |
|------------------------------|:---:|---|
| MOVE.L Dn, Dn                | 2   | |
| MOVE.W Dn, Dn                | 2   | |
| MOVEQ #imm, Dn               | 2   | |
| MOVE.W (An)+, Dn             | 4+3 = **6** | fea (An)+ = 3 |
| MOVE.W Dn, (An)              | 4   | |
| MOVE.W Dn, (An)+             | 4   | |
| ADD Dn, Dn                   | 2   | |
| ADDA.L Dn, An                | 2   | |
| ADDA.W Dn, An                | 4   | |
| AND Dn, Dn                   | 2   | |
| ANDI.W #imm, Dn              | 2+2 = **4** | fiea #W,Dn = 2 |
| ANDI.L #imm, Dn              | 2+4 = **6** | fiea #L,Dn = 4 |
| ASL #imm, Dn                 | 6   | Note: ASL ≠ ASR cost |
| ASR #imm, Dn                 | 4   | Faster than ASL |
| LSL #imm, Dn                 | 4   | |
| LSR #imm, Dn                 | 4   | |
| MULS.W Dn, Dn                | 28  | max; data-dependent |
| MULU.W Dn, Dn                | 28  | max; data-dependent |
| MULS.W (An)+, Dn             | 28+3 = **31** | fea (An)+ = 3 |
| DBRA (not expired)           | 8   | hot-path loop branch |
| DBRA (expired, fall through) | 13  | |
| Bcc (taken)                  | 8   | |
| Bcc.B (not taken)            | 4   | |
| NOP                          | 2   | |
| BFEXTU Dn{#o:#w}, Dn        | 10  | 68020+ only |
| BFEXTS Dn{#o:#w}, Dn        | 10  | 68020+ only |
| LEA (An), An                 | 2+0 = **2** | cea Dn/An = 0 |
| LEA (d16,An), An             | 2+2 = **4** | cea (d16,An) = 2 NCC |
| RTS                          | 11  | |
| JSR (An)                     | 7+2 = **9** | JEA (An) = 2 |

---

## Notes on Timing Model

**Total cost for a memory instruction** = base op cycles + EA time.

Example: `MULS.W (d8,An,Xn), Dn`
- Base `MULS.W EA,Dn` = 28 (NCC)
- fea `(d8,An,Xn)` = 6 (NCC)
- **Total = 34 cycles**

**Pipeline overlap:** The 68030 has a two-stage instruction pipeline. In a tight loop the CPU can overlap instruction fetch with execution, which is why some instructions show lower I-Cache Case totals. Without cache the CPU must stall waiting for instruction words; the NCC values reflect those stalls.

**DBRA loop body cost** = sum of all NCC instruction costs, with DBRA hot-path counted as 8 cycles per iteration.
