# MC68040 Instruction Timings — Section 10

Source: Motorola M68040 User's Manual, Section 10.

---

## Timing Model Overview

The MC68040 integer pipeline has four stages relevant to instruction timing:
**\<ea\> Calculate → \<ea\> Fetch → Execute → Write-Back**

### Notation

- **`<ea> Calculate`** — clocks spent in the effective-address calculation stage.
- **`Execute`** — expressed as **`NL + M`**:
  - `N` = *lead time*: the instruction can absorb N stall clocks without adding latency.
  - `M` = *base time*: minimum clocks in the execute stage.
  - Total worst-case = N + M clocks.
  - Example: `2L + 1` → total 3 clocks; up to 2 stall clocks are free.
- **`—`** — addressing mode not valid for this instruction/variant.
- Write-back times are system-dependent and **not** listed.

### Pipeline Assumptions

1. All timings in **BCLK cycles** with BR = An or suppressed.  
   For BR = PC, add **+1** to both `<ea> Calculate` and `Execute` unless noted.
2. All memory accesses **hit in cache** (no ATC misses).  
   Exception: CAS, CAS2, TAS force external bus accesses (zero-wait-state assumed).  
   Cache-miss penalty ≈ memory-access clocks added per operand fetch.
3. All accesses **aligned**: long-word operands on long-word boundaries, etc.
4. **FPU assumed idle** for integer-unit support timings of FP instructions.

### \<ea\> Fetch Memory Accesses by Addressing Mode

| Addressing Mode          | Fetch Operand | Send EA to Execute |
|--------------------------|:-------------:|:------------------:|
| Dn                       | 0             | 0                  |
| An                       | 0             | 0                  |
| (An)                     | 1             | 0                  |
| (An)+                    | 1             | 0                  |
| −(An)                    | 1             | 0                  |
| (d16,An)                 | 1             | 0                  |
| (d16,PC)                 | 1             | 0                  |
| (xxx).W / (xxx).L        | 1             | 0                  |
| #\<xxx\>                 | 0             | 0                  |
| (d8,An,Xn)               | 1             | 0                  |
| (d8,PC,Xn)               | 1             | 0                  |
| (BR,Xn)                  | 1             | 0                  |
| (bd,BR,Xn)               | 1             | 0                  |
| ([bd,BR,Xn])             | 2             | 1                  |
| ([bd,BR,Xn],od)          | 2             | 1                  |
| ([bd,BR],Xn)             | 2             | 1                  |
| ([bd,BR],Xn,od)          | 2             | 1                  |

Instructions using **brief or full extension word formats** interlock the `<ea> Calculate`
and `Execute` stages. If such an instruction stalls more than NL clocks, the `<ea> Calculate`
time increases by the excess.

---

## 10.3 CINV and CPUSH Timings

### CINV

| Instruction | Execution Time |
|-------------|---------------|
| CINVL       | 9 + Idle      |
| CINVP       | 266 + Idle    |
| CINVA       | 9 + Idle      |

*Idle* = clocks for all pending writes and instruction prefetches to complete.

### CPUSH (Best and Worst Case)

| Instruction    | Best Case | Worst Case               |
|----------------|:---------:|--------------------------|
| CPUSHL         | 6         | 6 + Line + Idle          |
| CPUSHP / CPUSHA| 267       | 11 + 256 × Line + Idle   |

*Line* = clocks for a cache-line transfer in the user's system.

---

## 10.4 MOVE Instruction Timing

Each cell shows `<ea> Calculate` / `Execute` for that source→destination combination.

### Destinations: Dn, (An), (An)+

| Source           | Dn calc | Dn exec | (An) calc | (An) exec | (An)+ calc | (An)+ exec |
|------------------|:-------:|:-------:|:---------:|:---------:|:----------:|:----------:|
| Dn               | 1       | 1       | 1         | 1         | 1          | 1          |
| (An)             | 1       | 1       | 1         | 1         | 2          | 1L+1       |
| (An)+            | 1       | 1       | 2         | 1L+1      | 2          | 1L+1       |
| −(An)            | 1       | 1       | 2         | 1L+1      | 2          | 1L+1       |
| (d16,An)         | 1       | 1       | 2         | 1L+1      | 2          | 1L+1       |
| (d16,PC)         | 3       | 2L+1    | 3         | 2L+1      | 3          | 2L+1       |
| (xxx).W/(xxx).L  | 1       | 1       | 1         | 1         | 2          | 1L+1       |
| #\<xxx\>         | 1       | 1       | 1         | 1         | 2          | 1L+1       |
| (d8,An,Xn)       | 3       | 3       | 4         | 4         | 5          | 5          |
| (d8,PC,Xn)       | 5       | 1L+4    | 5         | 1L+4      | 6          | 1L+5       |
| (b16,BR,Xn)      | 7       | 1L+6    | 7         | 1L+6      | 8          | 1L+7       |
| ([bd,BR,Xn])     | 10      | 1L+9    | 10        | 1L+9      | 11         | 1L+10      |
| ([bd,BR,Xn],od)  | 11      | 1L+10   | 11        | 1L+10     | 12         | 1L+11      |
| ([bd,BR],Xn)     | 11      | 3L+8    | 11        | 3L+8      | 12         | 3L+9       |
| ([bd,BR],Xn,od)  | 12      | 3L+9    | 12        | 3L+9      | 13         | 3L+10      |

### Destinations: −(An), (d16,An), (xxx).W/(xxx).L

| Source           | −(An) calc | −(An) exec | (d16,An) calc | (d16,An) exec | abs calc | abs exec |
|------------------|:----------:|:----------:|:-------------:|:-------------:|:--------:|:--------:|
| Dn               | 1          | 1          | 1             | 1             | 1        | 1        |
| (An)             | 2          | 1L+1       | 2             | 1L+1          | 1        | 1        |
| (An)+            | 2          | 1L+1       | 2             | 1L+1          | 2        | 1L+1     |
| −(An)            | 2          | 1L+1       | 2             | 1L+1          | 2        | 1L+1     |
| (d16,An)         | 2          | 1L+1       | 2             | 1L+1          | 2        | 1L+1     |
| (d16,PC)         | 3          | 2L+1       | 4             | 3L+1          | 4        | 3L+1     |
| (xxx).W/(xxx).L  | 2          | 1L+1       | 2             | 1L+1          | 2        | 1L+1     |
| #\<xxx\>         | 2          | 1L+1       | 2             | 1L+1          | 2        | 1L+1     |
| (d8,An,Xn)       | 5          | 5          | 5             | 5             | 5        | 5        |
| (d8,PC,Xn)       | 6          | 1L+5       | 6             | 1L+5          | 6        | 1L+5     |
| (b16,BR,Xn)      | 8          | 1L+7       | 8             | 1L+7          | 8        | 1L+7     |
| ([bd,BR,Xn])     | 11         | 1L+10      | 11            | 1L+10         | 11       | 1L+10    |
| ([bd,BR,Xn],od)  | 12         | 1L+11      | 12            | 1L+11         | 12       | 1L+11    |
| ([bd,BR],Xn)     | 12         | 3L+9       | 12            | 3L+9          | 12       | 3L+9     |
| ([bd,BR],Xn,od)  | 13         | 3L+10      | 13            | 3L+10         | 13       | 3L+10    |

### Destinations: (d8,An,Xn), (b16,An,Xn), ([bd,An,Xn])

| Source           | (d8,An,Xn) c | (d8,An,Xn) x | (b16,An,Xn) c | (b16,An,Xn) x | ([bd,An,Xn]) c | ([bd,An,Xn]) x |
|------------------|:------------:|:------------:|:-------------:|:-------------:|:--------------:|:--------------:|
| Dn               | 3            | 3            | 7             | 1L+6          | 10             | 1L+9           |
| (An)             | 4            | 4            | 7             | 1L+6          | 10             | 1L+9           |
| (An)+            | 4            | 4            | 7             | 1L+6          | 10             | 1L+9           |
| −(An)            | 4            | 4            | 7             | 1L+6          | 10             | 1L+9           |
| (d16,An)         | 4            | 4            | 7             | 1L+6          | 10             | 1L+9           |
| (d16,PC)         | 8            | 4L+4         | 10            | 4L+6          | 13             | 4L+9           |
| (xxx).W/(xxx).L  | 4            | 4            | 7             | 1L+6          | 10             | 1L+9           |
| #\<xxx\>         | 3            | 3            | 7             | 1L+6          | 10             | 1L+9           |
| (d8,An,Xn)       | 8            | 8            | 10            | 10            | 13             | 13             |
| (d8,PC,Xn)       | 9            | 1L+8         | 11            | 1L+10         | 14             | 1L+13          |
| (b16,BR,Xn)      | 11           | 1L+10        | 13            | 1L+12         | 16             | 1L+15          |
| ([bd,BR,Xn])     | 14           | 1L+13        | 16            | 1L+15         | 19             | 1L+18          |
| ([bd,BR,Xn],od)  | 15           | 1L+14        | 17            | 1L+16         | 20             | 1L+19          |
| ([bd,BR],Xn)     | 15           | 3L+12        | 17            | 3L+14         | 20             | 3L+17          |
| ([bd,BR],Xn,od)  | 16           | 3L+13        | 18            | 3L+15         | 21             | 3L+18          |

### Destinations: ([bd,An,Xn],od), ([bd,An],Xn), ([bd,An],Xn,od)

| Source           | (od) c | (od) x | ([bd,An],Xn) c | ([bd,An],Xn) x | (Xn,od) c | (Xn,od) x |
|------------------|:------:|:------:|:--------------:|:--------------:|:---------:|:---------:|
| Dn               | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| (An)             | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| (An)+            | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| −(An)            | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| (d16,An)         | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| (d16,PC)         | 14     | 4L+10  | 14             | 6L+8           | 15        | 6L+9      |
| (xxx).W/(xxx).L  | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| #\<xxx\>         | 11     | 1L+10  | 11             | 3L+8           | 12        | 3L+9      |
| (d8,An,Xn)       | 14     | 14     | 14             | 14             | 15        | 15        |
| (d8,PC,Xn)       | 15     | 1L+14  | 15             | 1L+14          | 16        | 1L+15     |
| (b16,BR,Xn)      | 17     | 1L+16  | 17             | 1L+16          | 18        | 1L+17     |
| ([bd,BR,Xn])     | 20     | 1L+19  | 20             | 1L+19          | 21        | 1L+20     |
| ([bd,BR,Xn],od)  | 21     | 1L+20  | 21             | 1L+20          | 22        | 1L+21     |
| ([bd,BR],Xn)     | 21     | 3L+18  | 21             | 3L+18          | 22        | 3L+19     |
| ([bd,BR],Xn,od)  | 22     | 3L+19  | 22             | 3L+19          | 23        | 3L+20     |

---

## 10.5 Miscellaneous Integer Unit Timings

Notes:
- **a** — Minimum times; instruction interlocks \<ea\> calc/execute and synchronizes processor.
- **b** — Typical times; instruction interlocks \<ea\> calc/execute and synchronizes processor.
- **c** — Instruction interlocks \<ea\> calc/execute stages.
- **d** — Successive in-line MOVE16 instructions each add 8 clocks to both stages.
- **e** — Typical: 3-level table search, no descriptor writes, no entries cached, 4-clock memory.
- **f** — Interlocks stages; for exception taken also synchronizes processor (times are minimum).

| Instruction         | Condition               | \<ea\> Calc | Execute   |
|---------------------|-------------------------|:-----------:|:---------:|
| ABCD                | Dy,Dx                   | 1           | 3         |
| ABCD                | −(Ay),−(Ax)             | 3           | 1L+3      |
| ADDX                | Dy,Dx                   | 1           | 1         |
| ADDX                | −(Ay),−(Ax)             | 3           | 1L+2      |
| ANDI #,CCR          | —                       | 1           | 4         |
| ANDI #,SR **a**     | —                       | 9           | 1L+8      |
| Bcc                 | Branch Taken            | 2           | 2         |
| Bcc                 | Branch Not Taken        | 3           | 3         |
| BRA                 | Branch Taken            | 2           | 2         |
| BRA                 | Branch Not Taken        | 3           | 3         |
| BSR \<offset\>      | —                       | 2           | 1L+1      |
| CAS2 **b**          | True                    | 56          | 6L+49     |
| CAS2 **b**          | False                   | 51          | 6L+44     |
| CMPM                | —                       | 3           | 1L+2      |
| DBcc **c**          | False, Count > −1       | 3           | 3         |
| DBcc **c**          | False, Count = −1       | 4           | 4         |
| DBcc **c**          | True                    | 4           | 4         |
| EORI #,CCR          | —                       | 1           | 4         |
| EORI #,SR **a**     | —                       | 9           | 1L+8      |
| EXG                 | Dy,Dx                   | 1           | 1         |
| EXG                 | Ay,Ax                   | 2           | 1L+1      |
| EXG                 | Dy,Ax                   | 1           | 1         |
| EXT                 | Word                    | 1           | 2         |
| EXT                 | Long Word               | 1           | 1         |
| EXTB                | Long Word               | 1           | 1         |
| ILLEGAL **a**       | A-Line / F-Line         | 16          | 16        |
| LINK                | —                       | 3           | 2L+1      |
| MOVE USP            | USP,An                  | 3           | 2L+1      |
| MOVE USP **a**      | An,USP                  | 7           | 1L+6      |
| MOVE16 **c,d**      | (Ax)+,(Ay)+             | 6           | 1L+7      |
| MOVE16 **c,d**      | xxx.L,(An)              | 4           | 7         |
| MOVE16 **c,d**      | xxx.L,(An)+             | 5           | 8         |
| MOVE16 **c,d**      | (An),xxx.L              | 4           | 7         |
| MOVE16 **c,d**      | (An)+,xxx.L             | 4           | 7         |
| MOVEC **b**         | Rn,Rc                   | 7           | 1L+6      |
| MOVEC **b**         | Rc,Rn                   | 11          | 1L+10     |
| MOVEP **c**         | MOVEP.W Dn,d16(An)      | 11          | 2L+9      |
| MOVEP **c**         | MOVEP.L Dn,d16(An)      | 13          | 2L+11     |
| MOVEP **c**         | MOVEP.W d16(An),Dn      | 4           | 2L+5      |
| MOVEP **c**         | MOVEP.L d16(An),Dn      | 8           | 2L+8      |
| MOVEQ               | —                       | 1           | 1         |
| NOP **a**           | —                       | 8           | 1L+7      |
| ORI #,CCR           | —                       | 1           | 4         |
| ORI #,SR **a**      | —                       | 9           | 1L+8      |
| PACK                | Dx,Dy,#                 | 1           | 3         |
| PACK                | −(Ay),−(Ax),#           | 3           | 2L+3      |
| PFLUSH **b**        | —                       | 11          | 1L+10     |
| PFLUSHA **b**       | —                       | 11          | 1L+10     |
| PFLUSHAN **b**      | —                       | 27          | 1L+26     |
| PFLUSHN(An) **b**   | —                       | 11          | 1L+10     |
| PTESTR/PTESTW **e** | —                       | 25          | 11L+14    |
| RESET **a**         | —                       | 521         | 521       |
| RTD **c**           | —                       | 6           | 1L+5      |
| RTE **a**           | Stack Format $0         | 2           | 13        |
| RTE **a**           | Stack Format $1         | 4           | 23        |
| RTE **a**           | Stack Format $2         | 2           | 14        |
| RTE **a**           | Stack Format $3         | 3           | 20        |
| RTE **a**           | Stack Format $4         | 2           | 15        |
| RTE **a**           | Stack Format $7         | 4           | 23        |
| RTR **c**           | —                       | 7           | 1L+6      |
| RTS **c**           | —                       | 5           | 5         |
| SBCD                | Dy,Dx                   | 1           | 3         |
| SBCD                | −(Ay),−(Ax)             | 3           | 1L+3      |
| SUBX                | Dy,Dx                   | 1           | 1         |
| SUBX                | −(Ay),−(Ax)             | 3           | 1L+2      |
| SWAP                | —                       | 1           | 2         |
| TRAP# **a**         | —                       | 16          | 16        |
| TRAPcc **f**        | Taken                   | 19          | 19        |
| TRAPcc **f**        | Not Taken               | 5           | 5         |
| TRAPV **f**         | Taken                   | 19          | 19        |
| TRAPV **f**         | Not Taken               | 5           | 5         |
| UNLK                | —                       | 2           | 1L+1      |
| UNPK                | Dx,Dy,#                 | 1           | 4         |
| UNPK                | −(Ay),−(Ax),#           | 3           | 2L+4      |

---

## 10.6 Integer Unit Instruction Timings

### ADD, AND, EOR, OR, SUB, TST

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| An               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 1           | 1       |
| −(An)            | 1           | 1       |
| (d16,An)         | 1           | 1       |
| (d16,PC)         | 3           | 2L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| #\<xxx\>         | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (d8,PC,Xn)       | 5           | 1L+4    |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 10          | 1L+9    |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+8    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### ADDA (and SUBA)

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 2       |
| An               | 1           | 1       |
| (An)             | 1           | 2       |
| (An)+            | 2           | 1L+2    |
| −(An)            | 2           | 1L+2    |
| (d16,An)         | 2           | 1L+2    |
| (d16,PC)         | 3           | 2L+2    |
| (xxx).W/(xxx).L  | 1           | 2       |
| #\<xxx\>         | 1           | 1       |
| (d8,An,Xn)       | 4           | 5       |
| (d8,PC,Xn)       | 5           | 1L+5    |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+12   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+11   |

### ADDI, ANDI, EORI, ORI, SUBI

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 2           | 1L+1    |
| −(An)            | 2           | 1L+1    |
| (d16,An)         | 2           | 1L+1    |
| (xxx).W/(xxx).L  | 2           | 1L+1    |
| (d8,An,Xn)       | 3           | 3       |
| (BR,Xn)          | 7           | 1L+6    |
| (bd,BR,Xn)       | 8           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### ADDQ, SUBQ

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| An               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 2           | 1L+1    |
| −(An)            | 2           | 1L+1    |
| (d16,An)         | 2           | 1L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (BR,Xn)          | 7           | 1L+6    |
| (bd,BR,Xn)       | 8           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+9    |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+8    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### ASL (memory operand)

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 3 / 4 * |
| (An)             | 1           | 3       |
| (An)+            | 1           | 3       |
| −(An)            | 1           | 3       |
| (d16,An)         | 1           | 3       |
| (xxx).W/(xxx).L  | 1           | 3       |
| (d8,An,Xn)       | 3           | 5       |
| (BR,Xn)          | 7           | 1L+8    |
| (bd,BR,Xn)       | 8           | 1L+9    |
| ([bd,BR,Xn])     | 10          | 1L+11   |
| ([bd,BR,Xn],od)  | 11          | 1L+12   |
| ([bd,BR],Xn)     | 11          | 3L+10   |
| ([bd,BR],Xn,od)  | 12          | 3L+11   |

\* `3` = immediate shift count; `4` = register-specified shift count.

### ASR, LSL, LSR

| Addressing Mode  | \<ea\> Calc | Execute  |
|------------------|:-----------:|:--------:|
| Dn               | 1           | 2 / 3 *  |
| (An)             | 1           | 2        |
| (An)+            | 1           | 2        |
| −(An)            | 1           | 2        |
| (d16,An)         | 1           | 2        |
| (xxx).W/(xxx).L  | 1           | 2        |
| (d8,An,Xn)       | 3           | 4        |
| (BR,Xn)          | 7           | 1L+7     |
| (bd,BR,Xn)       | 8           | 1L+8     |
| ([bd,BR,Xn])     | 10          | 1L+10    |
| ([bd,BR,Xn],od)  | 11          | 1L+11    |
| ([bd,BR],Xn)     | 11          | 3L+9     |
| ([bd,BR],Xn,od)  | 12          | 3L+10    |

\* `2` = immediate shift count; `3` = register-specified shift count.

### BCHG, BCLR, BSET

Note: T1 applies to `#<xxx>` bit number; T2 applies to Dn bit number.

| Addressing Mode  | \<ea\> Calc | Execute      |
|------------------|:-----------:|:------------:|
| Dn               | 1           | 3 / 4        |
| (An)             | 1           | 3 / 4        |
| (An)+            | 1           | 3 / 4        |
| −(An)            | 1           | 3 / 4        |
| (d16,An)         | 2 / 1       | 1L+3 / 1L+4  |
| (xxx).W/(xxx).L  | 2 / 1       | 1L+3 / 1L+4  |
| (d8,An,Xn)       | 3           | 5 / 6        |
| (BR,Xn)          | 7           | 1L+8 / 1L+9  |
| (bd,BR,Xn)       | 8           | 1L+9 / 1L+10 |
| ([bd,BR,Xn])     | 10          | 1L+11 / 1L+12|
| ([bd,BR,Xn],od)  | 11          | 1L+12 / 1L+13|
| ([bd,BR],Xn)     | 11          | 3L+10 / 3L+11|
| ([bd,BR],Xn,od)  | 12          | 3L+11 / 3L+12|

Format: `#<xxx> bit / Dn bit`.

### BFCHG, BFCLR, BFSET *(interlocks stages)*

Note: If bit field spans long-word boundary, add 10 to `<ea> calc` and 9 to `execute`.  
"e" notation: immediate width+offset / register width or offset.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 3 / 4 e     | 6 / 7 e |
| (An)             | 9           | 2L+8    |
| (d16,An)         | 9           | 2L+8    |
| (xxx).W/(xxx).L  | 9           | 2L+8    |
| (d8,An,Xn)       | 10          | 11      |
| (BR,Xn)          | 13          | 1L+13   |
| (bd,BR,Xn)       | 14          | 1L+14   |
| ([bd,BR,Xn])     | 16          | 1L+16   |
| ([bd,BR,Xn],od)  | 17          | 1L+17   |
| ([bd,BR],Xn)     | 17          | 3L+15   |
| ([bd,BR],Xn,od)  | 18          | 3L+16   |

### BFEXTS, BFEXTU *(interlocks stages)*

Note: If bit field spans long-word boundary, add 2 to `execute`.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1 / 2 e     | 4 / 5 e |
| (An)             | 9           | 2L+7    |
| (d16,An)         | 9           | 2L+7    |
| (d16,PC)         | 10          | 3L+7    |
| (xxx).W/(xxx).L  | 9           | 2L+7    |
| (d8,An,Xn)       | 10          | 10      |
| (d8,PC,Xn)       | 11          | 1L+10   |
| (BR,Xn)          | 13          | 1L+12   |
| (bd,BR,Xn)       | 14          | 1L+13   |
| ([bd,BR,Xn])     | 16          | 1L+15   |
| ([bd,BR,Xn],od)  | 17          | 1L+16   |
| ([bd,BR],Xn)     | 17          | 3L+14   |
| ([bd,BR],Xn,od)  | 18          | 3L+15   |

### BFFFO *(interlocks stages)*

Note: If bit field spans long-word boundary, add 2 to `execute`.  
"d" notation: immediate / register specified.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 3 / 4 d     | 6 / 7 d |
| (An)             | 9           | 2L+9    |
| (d16,An)         | 9           | 2L+9    |
| (d16,PC)         | 10          | 3L+9    |
| (xxx).W/(xxx).L  | 9           | 2L+9    |
| (d8,An,Xn)       | 10          | 12      |
| (d8,PC,Xn)       | 11          | 1L+12   |
| (BR,Xn)          | 13          | 1L+14   |
| (bd,BR,Xn)       | 14          | 1L+15   |
| ([bd,BR,Xn])     | 16          | 1L+17   |
| ([bd,BR,Xn],od)  | 17          | 1L+18   |
| ([bd,BR],Xn)     | 17          | 3L+16   |
| ([bd,BR],Xn,od)  | 18          | 3L+17   |

### BFINS *(interlocks stages)*

Note: If bit field spans long-word boundary, add 7 to both `<ea> calc` and `execute`.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 2 / 3 d     | 5 / 6 d |
| (An)             | 9           | 2L+7    |
| (d16,An)         | 9           | 2L+7    |
| (xxx).W/(xxx).L  | 9           | 2L+7    |
| (d8,An,Xn)       | 10          | 10      |
| (BR,Xn)          | 13          | 1L+12   |
| (bd,BR,Xn)       | 14          | 1L+13   |
| ([bd,BR,Xn])     | 16          | 1L+15   |
| ([bd,BR,Xn],od)  | 17          | 1L+16   |
| ([bd,BR],Xn)     | 17          | 3L+14   |
| ([bd,BR],Xn,od)  | 18          | 3L+15   |

### BFTST *(interlocks stages)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1 / 2 d     | 3 / 4 d |
| (An)             | 9           | 2L+7    |
| (d16,An)         | 9           | 2L+7    |
| (d16,PC)         | 10          | 3L+7    |
| (xxx).W/(xxx).L  | 9           | 2L+7    |
| (d8,An,Xn)       | 10          | 10      |
| (d8,PC,Xn)       | 11          | 1L+10   |
| (BR,Xn)          | 13          | 1L+12   |
| (bd,BR,Xn)       | 14          | 1L+13   |
| ([bd,BR,Xn])     | 16          | 1L+15   |
| ([bd,BR,Xn],od)  | 17          | 1L+16   |
| ([bd,BR],Xn)     | 17          | 3L+14   |
| ([bd,BR],Xn,od)  | 18          | 3L+15   |

### BTST

Note: T1 = `#<xxx>` bit number; T2 = Dn bit number.

| Addressing Mode  | \<ea\> Calc | Execute     |
|------------------|:-----------:|:-----------:|
| Dn               | 1           | 1 / 2       |
| (An)             | 1           | 1 / 2       |
| (An)+            | 1           | 1 / 2       |
| −(An)            | 1           | 1 / 2       |
| (d16,An)         | 2 / 1       | 1L+1 / 2    |
| (d16,PC)         | 3           | 2L+1 / 2L+2 |
| (xxx).W/(xxx).L  | 2 / 1       | 1L+1 / 2    |
| (d8,An,Xn)       | 3           | 3 / 4       |
| (d8,PC,Xn)       | 5           | 1L+4 / 1L+5 |
| (BR,Xn)          | 7 / 6       | 1L+6 / 1L+7 |
| (bd,BR,Xn)       | 8 / 7       | 1L+7 / 1L+8 |
| ([bd,BR,Xn])     | 10 / 9      | 1L+9 / 1L+10|
| ([bd,BR,Xn],od)  | 11 / 10     | 1L+10 / 1L+11|
| ([bd,BR],Xn)     | 11 / 10     | 3L+8 / 3L+9 |
| ([bd,BR],Xn,od)  | 12 / 11     | 3L+9 / 3L+10|

### CAS *(typical; interlocks stages)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 36          | 6L+31   |
| (An)+            | 37          | 5L+31   |
| −(An)            | 37          | 5L+31   |
| (d16,An)         | 37          | 5L+31   |
| (xxx).W/(xxx).L  | 36          | 5L+31   |
| (d8,An,Xn)       | 36          | 36      |
| (BR,Xn)          | 36          | 1L+35   |
| (bd,BR,Xn)       | 37          | 1L+36   |
| ([bd,BR,Xn])     | 42          | 40      |
| ([bd,BR,Xn],od)  | 42          | 1L+41   |
| ([bd,BR],Xn)     | 42          | 3L+38   |
| ([bd,BR],Xn,od)  | 42          | 3L+39   |

### CHK \<ea\>, Dn *(interlocks stages; times for Dn within bounds)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 8           | 1L+7    |
| #\<xxx\>         | 8           | 1L+7    |
| (An)             | 9           | 2L+7    |
| (An)+            | 9           | 2L+7    |
| −(An)            | 9           | 2L+7    |
| (d16,An)         | 9           | 2L+7    |
| (d16,PC)         | 10          | 3L+7    |
| (xxx).W/(xxx).L  | 9           | 2L+7    |
| (d8,An,Xn)       | 10          | 10      |
| (d8,PC,Xn)       | 11          | 1L+10   |
| (BR,Xn)          | 12          | 1L+11   |
| (bd,BR,Xn)       | 13          | 1L+12   |
| ([bd,BR,Xn])     | 16          | 1L+15   |
| ([bd,BR,Xn],od)  | 17          | 1L+16   |
| ([bd,BR],Xn)     | 17          | 3L+14   |
| ([bd,BR],Xn,od)  | 18          | 3L+15   |

### CHK2 \<ea\>, Rn *(typical; interlocks stages; Dn within bounds, UB > LB)*

Add 3 to both columns if UB < LB. Add 1 to both columns if Rn = An.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 11          | 2L+9    |
| (d16,An)         | 11          | 2L+9    |
| (d16,PC)         | 12          | 3L+9    |
| (xxx).W/(xxx).L  | 11          | 2L+9    |
| (d8,An,Xn)       | 13          | 1L+12   |
| (d8,PC,Xn)       | 14          | 2L+12   |
| (BR,Xn)          | 15          | 2L+13   |
| (bd,BR,Xn)       | 16          | 2L+14   |
| ([bd,BR,Xn])     | 19          | 2L+17   |
| ([bd,BR,Xn],od)  | 20          | 2L+18   |
| ([bd,BR],Xn)     | 20          | 4L+16   |
| ([bd,BR],Xn,od)  | 21          | 4L+17   |

### CLR

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 1           | 1       |
| −(An)            | 1           | 1       |
| (d16,An)         | 1           | 1       |
| (xxx).W/(xxx).L  | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 9           | 1L+8    |
| ([bd,BR,Xn],od)  | 10          | 1L+9    |
| ([bd,BR],Xn)     | 10          | 3L+7    |
| ([bd,BR],Xn,od)  | 11          | 3L+8    |

### CMP

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| An               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 1           | 1       |
| −(An)            | 1           | 1       |
| (d16,An)         | 1           | 1       |
| (d16,PC)         | 3           | 2L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| #\<xxx\>         | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (d8,PC,Xn)       | 5           | 1L+4    |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 9           | 1L+8    |
| ([bd,BR,Xn],od)  | 10          | 1L+9    |
| ([bd,BR],Xn)     | 10          | 3L+7    |
| ([bd,BR],Xn,od)  | 11          | 3L+8    |

### CMPA.L

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| An               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 2           | 1L+1    |
| −(An)            | 2           | 1L+1    |
| (d16,An)         | 2           | 1L+1    |
| (d16,PC)         | 3           | 2L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| #\<xxx\>         | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (d8,PC,Xn)       | 5           | 1L+4    |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 9           | 1L+8    |
| ([bd,BR,Xn],od)  | 10          | 1L+9    |
| ([bd,BR],Xn)     | 10          | 3L+7    |
| ([bd,BR],Xn,od)  | 11          | 3L+8    |

### CMPI

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 2           | 1L+1    |
| −(An)            | 2           | 1L+1    |
| (d16,An)         | 2           | 1L+1    |
| (d16,PC)         | 3           | 2L+1    |
| (xxx).W/(xxx).L  | 2           | 1L+1    |
| (d8,An,Xn)       | 3           | 3       |
| (d8,PC,Xn)       | 5           | 2L+4    |
| (BR,Xn)          | 6           | 2L+5    |
| (bd,BR,Xn)       | 7           | 2L+6    |
| ([bd,BR,Xn])     | 9           | 2L+8    |
| ([bd,BR,Xn],od)  | 10          | 2L+9    |
| ([bd,BR],Xn)     | 10          | 4L+7    |
| ([bd,BR],Xn,od)  | 11          | 4L+8    |

### CMP2 *(typical)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 13          | 2L+11   |
| (An)+            | 0           | 0       |
| −(An)            | 0           | 0       |
| (d16,An)         | 13          | 2L+11   |
| (d16,PC)         | 14          | 3L+11   |
| (xxx).W/(xxx).L  | 13          | 2L+11   |
| (d8,An,Xn)       | 15          | 1L+14   |
| (d8,PC,Xn)       | 16          | 2L+14   |
| (BR,Xn)          | 17          | 2L+15   |
| (bd,BR,Xn)       | 18          | 2L+16   |
| ([bd,BR,Xn])     | 21          | 2L+19   |
| ([bd,BR,Xn],od)  | 22          | 2L+20   |
| ([bd,BR],Xn)     | 22          | 4L+18   |
| ([bd,BR],Xn,od)  | 23          | 4L+19   |

### DIVS.W, DIVU.W *(interlocks stages)*

DIV/0 exception processing ≈ 16 + `<ea> calc` additional clocks.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 8           | 27      |
| (An)             | 8           | 27      |
| (An)+            | 8           | 27      |
| −(An)            | 8           | 27      |
| (d16,An)         | 8           | 27      |
| (d16,PC)         | 11          | 3L+27   |
| (xxx).W/(xxx).L  | 8           | 27      |
| #\<xxx\>         | 8           | 27      |
| (d8,An,Xn)       | 11          | 30      |
| (d8,PC,Xn)       | 12          | 1L+30   |
| (BR,Xn)          | 13          | 1L+31   |
| (bd,BR,Xn)       | 14          | 1L+32   |
| ([bd,BR,Xn])     | 17          | 1L+35   |
| ([bd,BR,Xn],od)  | 18          | 1L+36   |
| ([bd,BR],Xn)     | 18          | 3L+34   |
| ([bd,BR],Xn,od)  | 19          | 3L+35   |

### DIVS.L, DIVU.L, DIVSL.L, DIVUL.L *(interlocks stages)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 9           | 44      |
| (An)             | 9           | 44      |
| (An)+            | 9           | 44      |
| −(An)            | 9           | 44      |
| (d16,An)         | 11          | 2L+44   |
| (d16,PC)         | 12          | 3L+44   |
| (xxx).W/(xxx).L  | 11          | 2L+44   |
| #\<xxx\>         | 10          | 1L+44   |
| (d8,An,Xn)       | 12          | 47      |
| (d8,PC,Xn)       | 13          | 1L+47   |
| (BR,Xn)          | 14          | 1L+48   |
| (bd,BR,Xn)       | 15          | 1L+49   |
| ([bd,BR,Xn])     | 18          | 1L+52   |
| ([bd,BR,Xn],od)  | 19          | 1L+53   |
| ([bd,BR],Xn)     | 19          | 3L+51   |
| ([bd,BR],Xn,od)  | 20          | 3L+52   |

### JMP

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 3           | 2L+1    |
| (d16,An)         | 4           | 3L+1    |
| (d16,PC)         | 6           | 5L+1    |
| (xxx).W/(xxx).L  | 3           | 2L+1    |
| (d8,An,Xn)       | 6           | 6       |
| (d8,PC,Xn)       | 7           | 1L+6    |
| (BR,Xn)          | 8           | 1L+7    |
| (bd,BR,Xn)       | 9           | 1L+8    |
| ([bd,BR,Xn])     | 12          | 1L+11   |
| ([bd,BR,Xn],od)  | 12          | 1L+11   |
| ([bd,BR],Xn)     | 13          | 3L+10   |
| ([bd,BR],Xn,od)  | 14          | 3L+11   |

### JSR

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 3           | 2L+1    |
| (d16,An)         | 4           | 3L+1    |
| (d16,PC)         | 6           | 5L+1    |
| (xxx).W/(xxx).L  | 3           | 2L+1    |
| (d8,An,Xn)       | 6           | 6       |
| (d8,PC,Xn)       | 7           | 1L+6    |
| (BR,Xn)          | 8           | 1L+7    |
| (bd,BR,Xn)       | 9           | 1L+8    |
| ([bd,BR,Xn])     | 12          | 1L+11   |
| ([bd,BR,Xn],od)  | 13          | 1L+12   |
| ([bd,BR],Xn)     | 13          | 3L+10   |
| ([bd,BR],Xn,od)  | 14          | 3L+11   |

### LEA

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 1           | 1       |
| (d16,An)         | 2           | 1L+1    |
| (d16,PC)         | 4           | 3L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| (d8,An,Xn)       | 4           | 4       |
| (d8,PC,Xn)       | 5           | 1L+4    |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 9           | 1L+8    |
| ([bd,BR,Xn],od)  | 10          | 1L+9    |
| ([bd,BR],Xn)     | 10          | 3L+7    |
| ([bd,BR],Xn,od)  | 11          | 3L+8    |

### MOVE from CCR

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 2       |
| (An)             | 1           | 2       |
| (An)+            | 1           | 2       |
| −(An)            | 1           | 2       |
| (d16,An)         | 1           | 2       |
| (xxx).W/(xxx).L  | 1           | 2       |
| (d8,An,Xn)       | 3           | 4       |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### MOVE to CCR

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 2       |
| (An)             | 1           | 2       |
| (An)+            | 1           | 2       |
| −(An)            | 1           | 2       |
| (d16,An)         | 1           | 2       |
| (d16,PC)         | 3           | 2L+2    |
| (xxx).W/(xxx).L  | 1           | 2       |
| #\<xxx\>         | 1           | 2       |
| (d8,An,Xn)       | 3           | 4       |
| (d8,PC,Xn)       | 4           | 1L+4    |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### MOVE from SR *(interlocks stages)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 2           | 1L+2    |
| (An)             | 2           | 1L+2    |
| (An)+            | 2           | 1L+2    |
| −(An)            | 2           | 1L+2    |
| (d16,An)         | 2           | 1L+2    |
| (xxx).W/(xxx).L  | 2           | 1L+2    |
| (d8,An,Xn)       | 4           | 5       |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### MOVE to SR *(minimum times; interlocks and synchronizes)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 9           | 1L+8    |
| (An)             | 10          | 2L+8    |
| (An)+            | 10          | 2L+8    |
| −(An)            | 10          | 2L+8    |
| (d16,An)         | 10          | 2L+8    |
| (d16,PC)         | 11          | 3L+8    |
| (xxx).W/(xxx).L  | 10          | 2L+8    |
| #\<xxx\>         | 9           | 1L+8    |
| (d8,An,Xn)       | 11          | 11      |
| (d8,PC,Xn)       | 12          | 1L+11   |
| (bd,BR,Xn)       | 14          | 1L+13   |
| ([bd,BR,Xn])     | 17          | 1L+16   |
| ([bd,BR,Xn],od)  | 18          | 1L+17   |
| ([bd,BR],Xn)     | 18          | 3L+15   |
| ([bd,BR],Xn,od)  | 19          | 3L+16   |

### MOVEA.L

Add 1 clock to `execute` for MOVEA.W (except Dn and #\<xxx\>).

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| An               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 1           | 1       |
| −(An)            | 1           | 1       |
| (d16,An)         | 1           | 1       |
| (d16,PC)         | 3           | 2L+1    |
| (xxx).W/(xxx).L  | 1           | 1       |
| #\<xxx\>         | 1           | 1       |
| (d8,An,Xn)       | 4           | 4       |
| (d8,PC,Xn)       | 5           | 1L+4    |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 10          | 1L+9    |
| ([bd,BR,Xn],od)  | 11          | 1L+10   |
| ([bd,BR],Xn)     | 11          | 3L+8    |
| ([bd,BR],Xn,od)  | 12          | 3L+9    |

### MOVEM \<list\>,\<ea\> *(interlocks stages)*

D' = number of data registers in list; A' = number of address registers in list.  
(If no data registers, use D'=1.)  
For MOVEM.W, add N−2 and N clocks to `<ea> calc` and `execute` respectively for N address registers.

| Addressing Mode  | \<ea\> Calc      | Execute               |
|------------------|:----------------:|:---------------------:|
| (An)             | 2+D'+A'          | 1L+1+D'+A'            |
| −(An)            | 2+D'+A'          | 1L+1+D'+A'            |
| (d16,An)         | 2+D'+A'          | 1L+1+D'+A'            |
| (xxx).W/(xxx).L  | 2+D'+A'          | 1L+1+D'+A'            |
| (d8,An,Xn)       | 9+D'+A'          | 2L+7+D'+A'            |
| (BR,Xn)          | 11+D'+A'         | 3L+8+D'+A'            |
| (bd,BR,Xn)       | 12+D'+A'         | 3L+9+D'+A'            |
| ([bd,BR,Xn])     | 15+D'+A'         | 3L+12+D'+A'           |
| ([bd,BR,Xn],od)  | 16+D'+A'         | 3L+13+D'+A'           |
| ([bd,BR],Xn)     | 16+D'+A'         | 5L+11+D'+A'           |
| ([bd,BR],Xn,od)  | 17+D'+A'         | 5L+12+D'+A'           |

### MOVEM.L \<ea\>,\<list\> *(interlocks stages)*

D' = number of data registers; A' = number of address registers (A = A' for base timing).

| Addressing Mode  | \<ea\> Calc   | Execute             |
|------------------|:-------------:|:-------------------:|
| (An)             | 3+D'+A        | 1L+2+D'+A'          |
| (An)+            | 3+D'+A        | 1L+2+D'+A'          |
| (d16,An)         | 3+D'+A        | 1L+2+D'+A'          |
| (d16,PC)         | 4+D'+A        | 2L+2+D'+A'          |
| (xxx).W/(xxx).L  | 3+D'+A        | 1L+2+D'+A'          |
| (d8,An,Xn)       | 10+D'+A       | 2L+8+D'+A'          |
| (d8,PC,Xn)       | 11+D'+A       | 3L+8+D'+A'          |
| (BR,Xn)          | 12+D'+A       | 3L+9+D'+A'          |
| (bd,BR,Xn)       | 13+D'+A       | 3L+10+D'+A'         |
| ([bd,BR,Xn])     | 16+D'+A       | 3L+13+D'+A'         |
| ([bd,BR,Xn],od)  | 17+D'+A       | 3L+14+D'+A'         |
| ([bd,BR],Xn)     | 17+D'+A       | 5L+12+D'+A'         |
| ([bd,BR],Xn,od)  | 18+D'+A       | 5L+13+D'+A'         |

### MULS.W / MULS.L

Format: `word / long-word` operand size.

| Addressing Mode  | \<ea\> Calc | Execute          |
|------------------|:-----------:|:----------------:|
| Dn               | 1           | 16 / 20          |
| (An)             | 1           | 16 / 20          |
| (An)+            | 1           | 16 / 20          |
| −(An)            | 1           | 16 / 20          |
| (d16,An)         | 1 / 2       | 16 / 20          |
| (d16,PC)         | 3           | 2L+16 / 2L+20    |
| (xxx).W/(xxx).L  | 1 / 2       | 16 / 20          |
| #\<xxx\>         | 1           | 16 / 20          |
| (d8,An,Xn)       | 3           | 18 / 22          |
| (d8,PC,Xn)       | 5           | 1L+19 / 1L+23    |
| (BR,Xn)          | 6           | 1L+20 / 1L+24    |
| (bd,BR,Xn)       | 7           | 1L+21 / 1L+25    |
| ([bd,BR,Xn])     | 9           | 1L+23 / 1L+27    |
| ([bd,BR,Xn],od)  | 10          | 1L+24 / 1L+28    |
| ([bd,BR],Xn)     | 10          | 3L+22 / 3L+26    |
| ([bd,BR],Xn,od)  | 11          | 3L+23 / 3L+27    |

### MULU.W / MULU.L

| Addressing Mode  | \<ea\> Calc | Execute          |
|------------------|:-----------:|:----------------:|
| Dn               | 1           | 14 / 20          |
| (An)             | 1           | 14 / 20          |
| (An)+            | 1           | 14 / 20          |
| −(An)            | 1           | 14 / 20          |
| (d16,An)         | 1 / 2       | 14 / 20          |
| (d16,PC)         | 3           | 14 / 20          |
| (xxx).W/(xxx).L  | 1 / 2       | 14 / 20          |
| #\<xxx\>         | 1           | 14 / 20          |
| (d8,An,Xn)       | 3           | 16 / 22          |
| (d8,PC,Xn)       | 5           | 1L+17 / 1L+23    |
| (BR,Xn)          | 6           | 1L+18 / 1L+24    |
| (bd,BR,Xn)       | 7           | 1L+19 / 1L+25    |
| ([bd,BR,Xn])     | 9           | 1L+21 / 1L+27    |
| ([bd,BR,Xn],od)  | 10          | 1L+22 / 1L+28    |
| ([bd,BR],Xn)     | 10          | 3L+20 / 3L+26    |
| ([bd,BR],Xn,od)  | 11          | 3L+21 / 3L+27    |

### NBCD

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 3       |
| (An)             | 1           | 2       |
| (An)+            | 1           | 2       |
| −(An)            | 1           | 2       |
| (d16,An)         | 1           | 2       |
| (xxx).W/(xxx).L  | 1           | 2       |
| (d8,An,Xn)       | 3           | 4       |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 9           | 1L+9    |
| ([bd,BR,Xn],od)  | 10          | 1L+10   |
| ([bd,BR],Xn)     | 10          | 3L+8    |
| ([bd,BR],Xn,od)  | 11          | 3L+9    |

### NEG, NEGX, NOT

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 1       |
| (An)             | 1           | 1       |
| (An)+            | 1           | 1       |
| −(An)            | 1           | 1       |
| (d16,An)         | 1           | 1       |
| (xxx).W/(xxx).L  | 1           | 1       |
| (d8,An,Xn)       | 3           | 3       |
| (BR,Xn)          | 6           | 1L+5    |
| (bd,BR,Xn)       | 7           | 1L+6    |
| ([bd,BR,Xn])     | 9           | 1L+8    |
| ([bd,BR,Xn],od)  | 10          | 1L+9    |
| ([bd,BR],Xn)     | 10          | 3L+7    |
| ([bd,BR],Xn,od)  | 11          | 3L+8    |

### PEA

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 2           | 1L+1    |
| (d16,An)         | 2           | 1L+1    |
| (d16,PC)         | 4           | 3L+1    |
| (xxx).W/(xxx).L  | 2           | 1L+1    |
| (d8,An,Xn)       | 4           | 1L+3    |
| (d8,PC,Xn)       | 6           | 2L+4    |
| (BR,Xn)          | 7           | 2L+5    |
| (bd,BR,Xn)       | 8           | 2L+6    |
| ([bd,BR,Xn])     | 10          | 2L+8    |
| ([bd,BR,Xn],od)  | 11          | 2L+9    |
| ([bd,BR],Xn)     | 11          | 4L+7    |
| ([bd,BR],Xn,od)  | 12          | 4L+8    |

### ROL, ROR

\* `3` = immediate count; `4` = register count (Dn mode only).

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 3 / 4 * |
| (An)             | 1           | 3       |
| (An)+            | 1           | 3       |
| −(An)            | 1           | 3       |
| (d16,An)         | 1           | 3       |
| (xxx).W/(xxx).L  | 1           | 3       |
| (d8,An,Xn)       | 3           | 5       |
| (BR,Xn)          | 6           | 1L+7    |
| (bd,BR,Xn)       | 7           | 1L+8    |
| ([bd,BR,Xn])     | 9           | 1L+10   |
| ([bd,BR,Xn],od)  | 10          | 1L+11   |
| ([bd,BR],Xn)     | 10          | 3L+9    |
| ([bd,BR],Xn,od)  | 11          | 3L+10   |

### ROXL, ROXR

\* `5` = immediate count; `6` = register count.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 5 / 6 * |
| (An)             | 1           | 2       |
| (An)+            | 1           | 2       |
| −(An)            | 1           | 2       |
| (d16,An)         | 1           | 2       |
| (xxx).W/(xxx).L  | 1           | 2       |
| (d8,An,Xn)       | 3           | 4       |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 9           | 1L+9    |
| ([bd,BR,Xn],od)  | 10          | 1L+10   |
| ([bd,BR],Xn)     | 10          | 3L+8    |
| ([bd,BR],Xn,od)  | 11          | 3L+9    |

### Scc

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 2       |
| (An)             | 1           | 2       |
| (An)+            | 1           | 2       |
| −(An)            | 1           | 2       |
| (d16,An)         | 1           | 2       |
| (xxx).W/(xxx).L  | 1           | 2       |
| (d8,An,Xn)       | 4           | 5       |
| (BR,Xn)          | 6           | 1L+6    |
| (bd,BR,Xn)       | 7           | 1L+7    |
| ([bd,BR,Xn])     | 10          | 1L+10   |
| ([bd,BR,Xn],od)  | 11          | 1L+11   |
| ([bd,BR],Xn)     | 11          | 3L+9    |
| ([bd,BR],Xn,od)  | 12          | 3L+10   |

### SUBA — see ADDA table above

### TAS *(typical; interlocks and synchronizes)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 1           | 2       |
| (An)             | 26          | 2L+24   |
| (An)+            | 26          | 2L+24   |
| −(An)            | 26          | 2L+24   |
| (d16,An)         | 26          | 2L+24   |
| (xxx).W/(xxx).L  | 26          | 2L+24   |
| (d8,An,Xn)       | 27          | 27      |
| (BR,Xn)          | 30          | 1L+28   |
| (bd,BR,Xn)       | 31          | 1L+29   |
| ([bd,BR,Xn])     | 33          | 33      |
| ([bd,BR,Xn],od)  | 35          | 34      |
| ([bd,BR],Xn)     | 34          | 3L+31   |
| ([bd,BR],Xn,od)  | 36          | 3L+32   |

### TST — see ADD/AND/EOR/OR/SUB/TST table above

### MOVES \<ea\>,An *(typical; interlocks and synchronizes)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 28          | 4L+24   |
| (An)+            | 28          | 4L+24   |
| −(An)            | 17          | 2L+15   |
| (d16,An)         | 29          | 4L+24   |
| (xxx).W/(xxx).L  | 17          | 2L+15   |
| (d8,An,Xn)       | 29          | 1L+27   |
| (BR,Xn)          | 21          | 2L+19   |
| (bd,BR,Xn)       | 22          | 2L+20   |
| ([bd,BR,Xn])     | 35          | 2L+32   |
| ([bd,BR,Xn],od)  | 31          | 2L+29   |
| ([bd,BR],Xn)     | 36          | 4L+31   |
| ([bd,BR],Xn,od)  | 32          | 4L+28   |

### MOVES \<ea\>,Dn *(typical; interlocks and synchronizes)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 20          | 4L+19   |
| (An)+            | 20          | 4L+19   |
| −(An)            | 11          | 12      |
| (d16,An)         | 21          | 4L+19   |
| (xxx).W/(xxx).L  | 11          | 4L+10   |
| (d8,An,Xn)       | 21          | 1L+22   |
| (BR,Xn)          | 15          | 2L+14   |
| (bd,BR,Xn)       | 16          | 2L+15   |
| ([bd,BR,Xn])     | 26          | 2L+27   |
| ([bd,BR,Xn],od)  | 23          | 2L+24   |
| ([bd,BR],Xn)     | 27          | 4L+26   |
| ([bd,BR],Xn,od)  | 24          | 4L+23   |

### MOVES Rn,\<ea\> *(typical; interlocks and synchronizes)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 13          | 4L+9    |
| (An)+            | 13          | 4L+9    |
| −(An)            | 11          | 2L+9    |
| (d16,An)         | 14          | 4L+9    |
| (xxx).W/(xxx).L  | 11          | 2L+9    |
| (d8,An,Xn)       | 14          | 1L+12   |
| (BR,Xn)          | 15          | 2L+13   |
| (bd,BR,Xn)       | 16          | 2L+14   |
| ([bd,BR,Xn])     | 21          | 2L+17   |
| ([bd,BR,Xn],od)  | 20          | 2L+18   |
| ([bd,BR],Xn)     | 21          | 4L+16   |
| ([bd,BR],Xn,od)  | 21          | 4L+17   |

---

## 10.7 Floating-Point Unit Instruction Timings

The integer pipeline handles EA calculation and operand transfer; the FPU handles
computation. Integer-unit support times below assume **FPU is idle**. Times in
parentheses are the total stage occupancy (even though data can be passed early).

### 10.7.1 Miscellaneous Integer-Unit Support (FPU Branch/Trap)

| Instruction | Condition  | \<ea\> Calc | Execute |
|-------------|------------|:-----------:|:-------:|
| FBcc        | Taken      | 7           | 7       |
| FBcc        | Not Taken  | 6           | 6       |
| FDBcc       | cc True    | 9           | 1L+7    |
| FDBcc       | cc False   | 11          | 1L+9    |
| FNOP        | FPU Idle   | 6           | 6       |
| FTRAPcc     | Not Taken  | 6           | 1L+5    |

### 10.7.2 Integer-Unit Support: FABS, FADD, FCMP, FDIV, FMOVE, FMUL, FNEG, FSQRT, FSUB, FTST \<ea\>,FPn

For BR = PC, add 1 to both columns. All times assume idle FPU.

#### Byte, Word, Long Word, Single Precision sources

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 2           | 1L+2    |
| (An)             | 2           | 2       |
| (An)+            | 2           | 2       |
| −(An)            | 2           | 2       |
| (d16,An)         | 2           | 2       |
| (d16,PC)         | 4           | 2L+2    |
| (xxx).W/(xxx).L  | 3           | 1L+2    |
| #\<xxx\> (B/W/L) | 5           | 3L+2    |
| #\<xxx\> (L/S)   | 3           | 1L+2    |
| (d8,An,Xn)       | 5           | 5       |
| (d8,PC,Xn)       | 6           | 1L+5    |
| (An,Xn)          | 7           | 1L+6    |
| (bd,An,Xn)       | 8           | 1L+7    |
| ([bd,An,Xn])     | 11          | 1L+10   |
| ([bd,An,Xn],od)  | 12          | 1L+11   |
| ([bd,An],Xn)     | 12          | 3L+9    |
| ([bd,An],Xn,od)  | 13          | 3L+10   |

#### Double Precision sources

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 2           | 2       |
| (An)+            | 2           | 2       |
| −(An)            | 2           | 2       |
| (d16,An)         | 2           | 2       |
| (d16,PC)         | 4           | 1L+3    |
| (xxx).W/(xxx).L  | 3           | 1L+2    |
| #\<xxx\>         | 4           | 2L+2    |
| (d8,An,Xn)       | 5           | 5       |
| (d8,PC,Xn)       | 6           | 6       |
| (An,Xn)          | 7           | 1L+6    |
| (bd,An,Xn)       | 8           | 1L+7    |
| ([bd,An,Xn])     | 11          | 1L+10   |
| ([bd,An,Xn],od)  | 12          | 1L+11   |
| ([bd,An],Xn)     | 12          | 3L+9    |
| ([bd,An],Xn,od)  | 13          | 3L+10   |

#### Extended Precision sources

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| FPn              | 2           | 1L+2    |
| (An)             | 3           | 3       |
| (An)+            | 3           | 3       |
| −(An)            | 3           | 3       |
| (d16,An)         | 3           | 3       |
| (d16,PC)         | 5           | 1L+4    |
| (xxx).W/(xxx).L  | 4           | 1L+3    |
| #\<xxx\>         | 5           | 2L+3    |
| (d8,An,Xn)       | 6           | 6       |
| (d8,PC,Xn)       | 7           | 7       |
| (An,Xn)          | 8           | 1L+7    |
| (bd,An,Xn)       | 9           | 1L+8    |
| ([bd,An,Xn])     | 12          | 1L+11   |
| ([bd,An,Xn],od)  | 13          | 1L+12   |
| ([bd,An],Xn)     | 13          | 3L+10   |
| ([bd,An],Xn,od)  | 14          | 3L+11   |

### FMOVE FPn,\<ea\> *(idle FPU)*

| Addressing Mode  | B/W/L calc | B/W/L exec | S/D calc | S/D exec | X calc | X exec |
|------------------|:----------:|:----------:|:--------:|:--------:|:------:|:------:|
| Dn               | 9          | 9L+3       | 2        | 1L+3     | —      | —      |
| (An)             | 8          | 9L+2       | 2        | 1L+2     | 4      | 1L+3   |
| (An)+            | 8          | 9L+2       | 2        | 1L+2     | 4      | 1L+3   |
| −(An)            | 8          | 9L+2       | 2        | 1L+2     | 4      | 1L+3   |
| (d16,An)         | 8          | 9L+2       | 2        | 1L+2     | 4      | 1L+3   |
| (xxx).W/(xxx).L  | 8          | 9L+2       | 3        | 1L+2     | 4      | 1L+3   |
| (d8,An,Xn)       | 8          | 6L+5       | 5        | 5        | 6      | 6      |
| (An,Xn)          | 7          | 4L+6       | 7        | 1L+6     | 8      | 1L+7   |
| (bd,An,Xn)       | 8          | 4L+7       | 8        | 1L+7     | 9      | 1L+8   |
| ([bd,An,Xn])     | 11         | 1L+10      | 11       | 1L+10    | 12     | 1L+11  |
| ([bd,An,Xn],od)  | 12         | 1L+11      | 12       | 1L+11    | 13     | 1L+12  |
| ([bd,An],Xn)     | 12         | 3L+9       | 12       | 3L+9     | 13     | 3L+10  |
| ([bd,An],Xn,od)  | 13         | 3L+10      | 13       | 3L+10    | 14     | 3L+11  |

### FMOVE/FMOVEM to/from 1 Control Register *(idle FPU)*

Same as FMOVE \<ea\>,FPCR.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 2           | 1L+2    |
| An               | 2           | 1L+2    |
| (An)             | 4           | 2L+3    |
| (An)+            | 4           | 2L+3    |
| −(An)            | 5           | 3L+3    |
| (d16,An)         | 4           | 2L+3    |
| (d16,PC)         | 5           | 4L+3    |
| (xxx).W/(xxx).L  | 4           | 2L+3    |
| #\<xxx\>         | 4           | 2L+3    |
| (d8,An,Xn)       | 5           | 6       |
| (d8,PC,Xn)       | 6           | 1L+6    |
| (An,Xn)          | 7           | 1L+7    |
| (bd,An,Xn)       | 8           | 1L+8    |
| ([bd,An,Xn])     | 11          | 1L+11   |
| ([bd,An,Xn],od)  | 12          | 1L+13   |
| ([bd,An],Xn)     | 12          | 3L+10   |
| ([bd,An],Xn,od)  | 13          | 3L+12   |

### FMOVEM \<list\>,\<ea\> and \<ea\>,\<list\> *(idle FPU)*

Add 3 clocks to both columns per additional FP register beyond the first.  
Add 1 clock to both columns for dynamic register list.

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| (An)             | 17          | 2L+15   |
| (An)+            | 17          | 2L+15   |
| −(An)            | 16          | 1L+15   |
| (d16,An)         | 17          | 2L+15   |
| (xxx).W/(xxx).L  | 19          | 3L+15   |
| #\<xxx\>         | 19          | 1L+17   |
| (d8,An,Xn)       | 19          | 18      |
| (d8,PC,Xn)       | 20          | 1L+18   |
| (An,Xn)          | 20          | 1L+19   |
| (bd,An,Xn)       | 21          | 1L+20   |
| ([bd,An,Xn])     | 25          | 1L+23   |
| ([bd,An,Xn],od)  | 25          | 1L+24   |
| ([bd,An],Xn)     | 26          | 3L+22   |
| ([bd,An],Xn,od)  | 26          | 3L+23   |

### FScc *(idle FPU)*

| Addressing Mode  | \<ea\> Calc | Execute |
|------------------|:-----------:|:-------:|
| Dn               | 5           | 6       |
| (An)             | 4           | 5       |
| (An)+            | 6           | 2L+5    |
| −(An)            | 6           | 2L+5    |
| (d16,An)         | 4           | 5       |
| (xxx).W/(xxx).L  | 4           | 5       |
| (d8,An,Xn)       | 7           | 8       |
| (An,Xn)          | 9           | 1L+9    |
| (bd,An,Xn)       | 10          | 1L+10   |
| ([bd,An,Xn])     | 13          | 1L+13   |
| ([bd,An,Xn],od)  | 14          | 1L+14   |
| ([bd,An],Xn)     | 14          | 3L+12   |
| ([bd,An],Xn,od)  | 15          | 3L+13   |

### FSAVE \<ea\> *(idle FPU)*

| Addressing Mode  | Idle/Null c | Idle/Null x | Short c | Short x | Long c | Long x |
|------------------|:-----------:|:-----------:|:-------:|:-------:|:------:|:------:|
| (An)             | 12          | 1L+11       | 33      | 1L+32   | 50     | 1L+49  |
| −(An)            | 11          | 11          | 32      | 32      | 49     | 49     |
| (d16,An)         | 12          | 1L+11       | 33      | 1L+32   | 50     | 1L+49  |
| (xxx).W/(xxx).L  | 13          | 1L+11       | 34      | 1L+32   | 51     | 1L+49  |
| (d8,An,Xn)       | 13          | 13          | 34      | 34      | 51     | 51     |
| (An,Xn)          | 16          | 1L+14       | 37      | 1L+35   | 54     | 1L+52  |
| (bd,An,Xn)       | 17          | 1L+15       | 38      | 1L+36   | 55     | 1L+53  |
| ([bd,An,Xn])     | 19          | 1L+18       | 40      | 1L+39   | 57     | 1L+56  |
| ([bd,An,Xn],od)  | 21          | 1L+19       | 42      | 1L+40   | 59     | 1L+57  |
| ([bd,An],Xn)     | 20          | 3L+17       | 41      | 3L+38   | 58     | 3L+55  |
| ([bd,An],Xn,od)  | 22          | 3L+18       | 46      | 3L+42   | 65     | 3L+61  |

### FRESTORE \<ea\> *(idle FPU)*

| Addressing Mode  | Idle/Null c | Idle/Null x | Short c | Short x | Long c | Long x |
|------------------|:-----------:|:-----------:|:-------:|:-------:|:------:|:------:|
| (An)             | 13          | 1L+12       | 26      | 1L+25   | 40     | 1L+39  |
| (An)+            | 13          | 1L+12       | 26      | 1L+25   | 40     | 1L+39  |
| (d16,An)         | 13          | 1L+12       | 26      | 1L+25   | 40     | 1L+39  |
| (xxx).W/(xxx).L  | 14          | 1L+12       | 27      | 1L+25   | 41     | 1L+39  |
| (d8,An,Xn)       | 14          | 14          | 27      | 27      | 41     | 41     |
| (An,Xn)          | 16          | 1L+14       | 29      | 1L+27   | 43     | 1L+41  |
| (bd,An,Xn)       | 17          | 1L+15       | 30      | 1L+28   | 44     | 1L+42  |
| ([bd,An,Xn])     | 20          | 1L+19       | 33      | 1L+32   | 47     | 1L+46  |
| ([bd,An,Xn],od)  | 21          | 1L+19       | 34      | 1L+32   | 48     | 1L+46  |
| ([bd,An],Xn)     | 21          | 3L+18       | 34      | 3L+31   | 48     | 3L+45  |
| ([bd,An],Xn,od)  | 22          | 3L+19       | 35      | 3L+31   | 49     | 3L+45  |

---

## 10.7.3 Timings in the Floating-Point Unit

These are FPU-internal pipeline stage times.  
Parenthetical times, e.g. `2(3)`, mean 2 cycles to execute but stage is busy for 3 cycles.

### FADD, FSUB

| Opclass | Size | Precision | Operands    | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|-------------|:----------:|:---------:|:-------------:|
| 0       | —    | Any       | Norm,Norm   | 2(3)       | 3         | 2(3)          |
| 0       | —    | Any       | Norm,Zero   | 2(3)       | 3         | 2(3)          |
| 0       | —    | Any       | Zero,Zero   | 4          | 0         | 0             |
| 0       | —    | Any       | —,Inf       | 4          | 0         | 0             |
| 0       | —    | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | S,D  | Any       | Norm,Norm   | 2(3)       | 3         | 2(3)          |
| 2       | S,D  | Any       | Norm,Zero   | 2(3)       | 3         | 2(3)          |
| 2       | S,D  | Any       | Zero,Zero   | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,Inf       | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | X    | Any       | Norm,Norm   | 3(4)       | 3         | 2(3)          |
| 2       | X    | Any       | Norm,Zero   | 3(4)       | 3         | 2(3)          |
| 2       | X    | Any       | Zero,Zero   | 5          | 0         | 0             |
| 2       | X    | Any       | —,Inf       | 5          | 0         | 0             |
| 2       | X    | Any       | —,NAN       | 5          | 0         | 0             |

### FMUL

| Opclass | Size | Precision | Operands    | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|-------------|:----------:|:---------:|:-------------:|
| 0       | —    | Any       | Norm,Norm   | 2(3)       | 5         | 2(3)          |
| 0       | —    | Any       | —,Zero      | 4          | 0         | 0             |
| 0       | —    | Any       | —,Inf       | 4          | 0         | 0             |
| 0       | —    | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | S,D  | Any       | Norm,Norm   | 2(3)       | 5         | 2(3)          |
| 2       | S,D  | Any       | —,Zero      | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,Inf       | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | X    | Any       | Norm,Norm   | 3(4)       | 5         | 2(3)          |
| 2       | X    | Any       | —,Zero      | 5          | 0         | 0             |
| 2       | X    | Any       | —,Inf       | 5          | 0         | 0             |
| 2       | X    | Any       | —,NAN       | 5          | 0         | 0             |

### FDIV

| Opclass | Size | Precision | Operands    | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|-------------|:----------:|:---------:|:-------------:|
| 0       | —    | Any       | Norm,Norm   | 2(3)       | 37.5      | 2(3)          |
| 0       | —    | Any       | —,Zero      | 4          | 0         | 0             |
| 0       | —    | Any       | —,Inf       | 4          | 0         | 0             |
| 0       | —    | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | S,D  | Any       | Norm,Norm   | 2(3)       | 37.5      | 2(3)          |
| 2       | S,D  | Any       | —,Zero      | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,Inf       | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | X    | Any       | Norm,Norm   | 3(4)       | 37.5      | 2(3)          |
| 2       | X    | Any       | —,Zero      | 5          | 0         | 0             |
| 2       | —    | Any       | —,Inf       | 5          | 0         | 0             |
| 2       | X    | Any       | —,NAN       | 5          | 0         | 0             |

### FSQRT

| Opclass | Size | Precision | Operands           | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|--------------------|:----------:|:---------:|:-------------:|
| 0       | —    | Any       | Norm               | 2(3)       | 103       | 2(3)          |
| 0       | —    | Any       | Zero\|Inf\|NAN     | 4          | 0         | 0             |
| 2       | S,D  | Any       | Norm               | 2(3)       | 103       | 2(3)          |
| 2       | S,D  | Any       | Zero\|Inf\|NAN     | 4          | 0         | 0             |
| 2       | X    | Any       | Norm               | 3(4)       | 103       | 2(3)          |
| 2       | X    | Any       | Zero\|Inf\|NAN     | 5          | 0         | 0             |

### FMOVE, FABS, FNEG (FPU internal)

| Opclass | Size | Precision | Operands           | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|--------------------|:----------:|:---------:|:-------------:|
| 0       | —    | X         | Norm\|Zero\|Inf    | 2          | 0         | 0             |
| 0       | —    | X         | NAN                | 3          | 0         | 0             |
| 0       | —    | S,D       | Norm               | 5          | 0         | 0             |
| 0       | —    | S,D       | Zero\|Inf          | 3          | 0         | 0             |
| 0       | —    | S,D       | NAN                | 4          | 0         | 0             |
| 2       | S    | Any       | Norm\|Zero\|Inf    | 3          | 0         | 0             |
| 2       | S    | Any       | NAN                | 4          | 0         | 0             |
| 2       | D    | D,X       | Norm\|Zero\|Inf    | 3          | 0         | 0             |
| 2       | D    | D,X       | NAN                | 4          | 0         | 0             |
| 2       | D    | S         | Norm               | 5          | 0         | 0             |
| 2       | D    | S         | Zero\|Inf          | 4          | 0         | 0             |
| 2       | D    | S         | NAN                | 5          | 0         | 0             |
| 2       | X    | X         | Norm\|Zero\|Inf    | 4          | 0         | 0             |
| 2       | X    | X         | NAN                | 5          | 0         | 0             |
| 2       | X    | S,D       | Norm               | 6          | 0         | 0             |
| 2       | X    | S,D       | Zero\|Inf          | 5          | 0         | 0             |
| 2       | X    | S,D       | NAN                | 6          | 0         | 0             |
| 2       | B,W  | Any       | +Norm\|Zero        | 1.5(11)    | 4.5       | 2             |
| 2       | L    | D,X       | +Norm\|Zero        | 1.5(11)    | 4.5       | 2             |
| 2       | L    | S         | +Norm\|Zero        | 1.5(12.5)  | 4.5       | 2             |
| 2       | B,W  | Any       | −Norm              | 1.5(11.5)  | 5         | 2             |
| 2       | L    | D,X       | −Norm              | 1.5(11.5)  | 5         | 2             |
| 2       | L    | S         | −Norm              | 1.5(13)    | 5         | 2             |

### FMOVE (store FPn to integer/memory, opclass 3)

| Opclass | Size  | Precision | Operands       | Conversion | Execution | Normalization |
|:-------:|:-----:|:---------:|----------------|:----------:|:---------:|:-------------:|
| 3       | S,D   | Any       | Any            | 3          | 0         | 0             |
| 3       | X    | Any       | Any            | 4          | 0         | 0             |
| 3       | B,W,L | Any       | +(Norm\|Zero)  | 3(9)       | 1.5       | 3.5           |
| 3       | B,W,L | Any       | −(Norm\|Zero)  | 3(10)      | 1.5       | 4.5           |

### FMOVEM (FPU internal)

| Opclass | Conversion         | Execution | Normalization |
|:-------:|:------------------:|:---------:|:-------------:|
| 4       | 2 + (2 per reg)    | 0         | 0             |
| 5       | 2 + (2 per reg)    | 0         | 0             |
| 6       | 2 + (3 per reg)    | 0         | 0             |
| 7       | 2 + (3 per reg)    | 0         | 0             |

### FCMP (FPU internal)

| Opclass | Size | Precision | Operands    | Conversion | Execution | Normalization |
|:-------:|:----:|:---------:|-------------|:----------:|:---------:|:-------------:|
| 0       | —    | Any       | Norm,Norm   | 2(3)       | 3         | 1             |
| 0       | —    | Any       | Norm,Zero   | 2(3)       | 3         | 1             |
| 0       | —    | Any       | Zero,Zero   | 4          | 0         | 0             |
| 0       | —    | Any       | —,Inf       | 4          | 0         | 0             |
| 0       | —    | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | S,D  | Any       | Norm,Norm   | 2(3)       | 3         | 1             |
| 2       | S,D  | Any       | Norm,Zero   | 2(3)       | 3         | 1             |
| 2       | S,D  | Any       | Zero,Zero   | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,Inf       | 4          | 0         | 0             |
| 2       | S,D  | Any       | —,NAN       | 4          | 0         | 0             |
| 2       | X    | Any       | Norm,Norm   | 3(4)       | 3         | 1             |
| 2       | X    | Any       | Norm,Zero   | 3(4)       | 3         | 1             |
| 2       | X    | Any       | Zero,Zero   | 5          | 0         | 0             |
| 2       | X    | Any       | —,Inf       | 5          | 0         | 0             |
| 2       | X    | Any       | —,NAN       | 5          | 0         | 0             |

---

*End of MC68040 Instruction Timings (Section 10)*
