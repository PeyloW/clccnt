# MC68000 Instruction Timings — Section 8

Source: Motorola MC68000 8-/16-/32-Bit Microprocessors User's Manual, Section 8 (16-Bit Instruction Execution Times).
Applies to: MC68000, MC68HC000, MC68HC001, MC68EC000 (16-bit mode).

---

## Timing Notation

```
n(r/w)
```

- **n** = Total number of external clock (CLK) periods, including instruction fetch and all applicable operand fetches and stores
- **r** = Number of bus read cycles
- **w** = Number of bus write cycles

Each memory read or write cycle occupies **4 clock periods**. Wait states add directly to these totals.

> **Example:** `18(3/1)` — 18 total clocks: 12 for three reads, 4 for one write, 2 idle (internal).

---

## Timing Assumptions

1. All memory read and write cycles consist of **4 clock periods**. Longer cycles generate wait states which must be added to the totals.
2. `+` in a table cell means **add the effective address calculation time** (Table 8-1) to that value.
3. `—` means the addressing mode or size is not valid for that instruction/variant.

---

## §1 Effective Address Calculation Times

Times include fetching extension words, computing the address, and fetching the memory operand. Write cycles are zero for all EA calculations.

| Addressing Mode | Description | Byte, Word | Long |
|---|---|:---:|:---:|
| Dn | Data Register Direct | 0(0/0) | 0(0/0) |
| An | Address Register Direct | 0(0/0) | 0(0/0) |
| (An) | Address Register Indirect | 4(1/0) | 8(2/0) |
| (An)+ | Address Register Indirect with Postincrement | 4(1/0) | 8(2/0) |
| –(An) | Address Register Indirect with Predecrement | 6(1/0) | 10(2/0) |
| (d16,An) | Address Register Indirect with Displacement | 8(2/0) | 12(3/0) |
| (d8,An,Xn)† | Address Register Indirect with Index | 10(2/0) | 14(3/0) |
| (xxx).W | Absolute Short | 8(2/0) | 12(3/0) |
| (xxx).L | Absolute Long | 12(3/0) | 16(4/0) |
| (d16,PC) | PC Indirect with Displacement | 8(2/0) | 12(3/0) |
| (d8,PC,Xn)† | PC Indirect with Index | 10(2/0) | 14(3/0) |
| #\<data\> | Immediate | 4(1/0) | 8(2/0) |

> **Note †:** The size of the index register (Xn) does not affect execution time.

---

## §2 MOVE Instruction Execution Times

Totals include instruction fetch, operand reads, and operand writes.

> **Note †:** The size of the index register (Xn) does not affect execution time.

### Table 8-2. Move Byte and Word Instruction Execution Times

| Source \ Dest | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xn)† | (xxx).W | (xxx).L |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Dn | 4(1/0) | 4(1/0) | 8(1/1) | 8(1/1) | 8(1/1) | 12(2/1) | 14(2/1) | 12(2/1) | 16(3/1) |
| An | 4(1/0) | 4(1/0) | 8(1/1) | 8(1/1) | 8(1/1) | 12(2/1) | 14(2/1) | 12(2/1) | 16(3/1) |
| (An) | 8(2/0) | 8(2/0) | 12(2/1) | 12(2/1) | 12(2/1) | 16(3/1) | 18(3/1) | 16(3/1) | 20(4/1) |
| (An)+ | 8(2/0) | 8(2/0) | 12(2/1) | 12(2/1) | 12(2/1) | 16(3/1) | 18(3/1) | 16(3/1) | 20(4/1) |
| –(An) | 10(2/0) | 10(2/0) | 14(2/1) | 14(2/1) | 14(2/1) | 18(3/1) | 20(3/1) | 18(3/1) | 22(4/1) |
| (d16,An) | 12(3/0) | 12(3/0) | 16(3/1) | 16(3/1) | 16(3/1) | 20(4/1) | 22(4/1) | 20(4/1) | 24(5/1) |
| (d8,An,Xn)† | 14(3/0) | 14(3/0) | 18(3/1) | 18(3/1) | 18(3/1) | 22(4/1) | 24(4/1) | 22(4/1) | 26(5/1) |
| (xxx).W | 12(3/0) | 12(3/0) | 16(3/1) | 16(3/1) | 16(3/1) | 20(4/1) | 22(4/1) | 20(4/1) | 24(5/1) |
| (xxx).L | 16(4/0) | 16(4/0) | 20(4/1) | 20(4/1) | 20(4/1) | 24(5/1) | 26(5/1) | 24(5/1) | 28(6/1) |
| (d16,PC) | 12(3/0) | 12(3/0) | 16(3/1) | 16(3/1) | 16(3/1) | 20(4/1) | 22(4/1) | 20(4/1) | 24(5/1) |
| (d8,PC,Xn)† | 14(3/0) | 14(3/0) | 18(3/1) | 18(3/1) | 18(3/1) | 22(4/1) | 24(4/1) | 22(4/1) | 26(5/1) |
| #\<data\> | 8(2/0) | 8(2/0) | 12(2/1) | 12(2/1) | 12(2/1) | 16(3/1) | 18(3/1) | 16(3/1) | 20(4/1) |

### Table 8-3. Move Long Instruction Execution Times

| Source \ Dest | Dn | An | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xn)† | (xxx).W | (xxx).L |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Dn | 4(1/0) | 4(1/0) | 12(1/2) | 12(1/2) | 12(1/2) | 16(2/2) | 18(2/2) | 16(2/2) | 20(3/2) |
| An | 4(1/0) | 4(1/0) | 12(1/2) | 12(1/2) | 12(1/2) | 16(2/2) | 18(2/2) | 16(2/2) | 20(3/2) |
| (An) | 12(3/0) | 12(3/0) | 20(3/2) | 20(3/2) | 20(3/2) | 24(4/2) | 26(4/2) | 24(4/2) | 28(5/2) |
| (An)+ | 12(3/0) | 12(3/0) | 20(3/2) | 20(3/2) | 20(3/2) | 24(4/2) | 26(4/2) | 24(4/2) | 28(5/2) |
| –(An) | 14(3/0) | 14(3/0) | 22(3/2) | 22(3/2) | 22(3/2) | 26(4/2) | 28(4/2) | 26(4/2) | 30(5/2) |
| (d16,An) | 16(4/0) | 16(4/0) | 24(4/2) | 24(4/2) | 24(4/2) | 28(5/2) | 30(5/2) | 28(5/2) | 32(6/2) |
| (d8,An,Xn)† | 18(4/0) | 18(4/0) | 26(4/2) | 26(4/2) | 26(4/2) | 30(5/2) | 32(5/2) | 30(5/2) | 34(6/2) |
| (xxx).W | 16(4/0) | 16(4/0) | 24(4/2) | 24(4/2) | 24(4/2) | 28(5/2) | 30(5/2) | 28(5/2) | 32(6/2) |
| (xxx).L | 20(5/0) | 20(5/0) | 28(5/2) | 28(5/2) | 28(5/2) | 32(6/2) | 34(6/2) | 32(6/2) | 36(7/2) |
| (d16,PC) | 16(4/0) | 16(4/0) | 24(4/2) | 24(4/2) | 24(4/2) | 28(5/2) | 30(5/2) | 28(5/2) | 32(5/2) |
| (d8,PC,Xn)† | 18(4/0) | 18(4/0) | 26(4/2) | 26(4/2) | 26(4/2) | 30(5/2) | 32(5/2) | 30(5/2) | 34(6/2) |
| #\<data\> | 12(3/0) | 12(3/0) | 20(3/2) | 20(3/2) | 20(3/2) | 24(4/2) | 26(4/2) | 24(4/2) | 28(5/2) |

---

## §3 Standard Instruction Execution Times

Times include performing the operation, storing the result, and reading the next instruction. Add EA calculation time where `+` is shown.

Notation: **An** = address register operand; **Dn** = data register operand; **ea** = effective address operand; **M** = memory effective address operand.

| Instruction | Size | op \<ea\>, An‡ | op \<ea\>, Dn | op Dn, \<M\> |
|---|---|:---:|:---:|:---:|
| ADD / ADDA | Byte, Word | 8(1/0)+ | 4(1/0)+ | 8(1/1)+ |
| ADD / ADDA | Long | 6(1/0)+† | 6(1/0)+† | 12(1/2)+ |
| AND | Byte, Word | — | 4(1/0)+ | 8(1/1)+ |
| AND | Long | — | 6(1/0)+† | 12(1/2)+ |
| CMP / CMPA | Byte, Word | 6(1/0)+ | 4(1/0)+ | — |
| CMP / CMPA | Long | 6(1/0)+ | 6(1/0)+ | — |
| DIVS | — | — | 158(1/0)+* | — |
| DIVU | — | — | 140(1/0)+* | — |
| EOR | Byte, Word | — | 4(1/0)** | 8(1/1)+ |
| EOR | Long | — | 8(1/0)** | 12(1/2)+ |
| MULS | — | — | 70(1/0)+* | — |
| MULU | — | — | 70(1/0)+* | — |
| OR | Byte, Word | — | 4(1/0)+ | 8(1/1)+ |
| OR | Long | — | 6(1/0)+† | 12(1/2)+ |
| SUB | Byte, Word | 8(1/0)+ | 4(1/0)+ | 8(1/1)+ |
| SUB | Long | 6(1/0)+† | 6(1/0)+† | 12(1/2)+ |

> **Note +:** Add effective address calculation time.
> **Note ‡:** Word or long only.
> **Note *:** Indicates maximum basic value; add word effective address calculation time.
> **Note †:** The base time of 6 clock periods is increased to 8 if the effective address mode is register direct or immediate (also add effective address time in that case).
> **Note \*\*:** Only valid effective address mode is data register direct.

**DIVS / DIVU:** The divide algorithm provides less than 10% difference between best- and worst-case timings.

**MULS / MULU:** The multiply algorithm requires `38 + 2n` clocks where:
- MULU: n = number of ones in the \<ea\>
- MULS: n = number of 10 or 01 bit-pair transitions in the 17-bit value formed by concatenating \<ea\> with a zero LSB (worst case: source = $5555)

---

## §4 Immediate Instruction Execution Times

Times include fetching the immediate operand, performing the operation, storing the result, and reading the next instruction. Add EA calculation time where `+` is shown.

Notation: **#** = immediate operand; **Dn** = data register; **An** = address register; **M** = memory operand.

| Instruction | Size | op #, Dn | op #, An | op #, M |
|---|---|:---:|:---:|:---:|
| ADDI | Byte, Word | 8(2/0) | — | 12(2/1)+ |
| ADDI | Long | 16(3/0) | — | 20(3/2)+ |
| ADDQ | Byte, Word | 4(1/0) | 4(1/0)* | 8(1/1)+ |
| ADDQ | Long | 8(1/0) | 8(1/0) | 12(1/2)+ |
| ANDI | Byte, Word | 8(2/0) | — | 12(2/1)+ |
| ANDI | Long | 14(3/0) | — | 20(3/2)+ |
| CMPI | Byte, Word | 8(2/0) | — | 8(2/0)+ |
| CMPI | Long | 14(3/0) | — | 12(3/0)+ |
| EORI | Byte, Word | 8(2/0) | — | 12(2/1)+ |
| EORI | Long | 16(3/0) | — | 20(3/2)+ |
| MOVEQ | Long | 4(1/0) | — | — |
| ORI | Byte, Word | 8(2/0) | — | 12(2/1)+ |
| ORI | Long | 16(3/0) | — | 20(3/2)+ |
| SUBI | Byte, Word | 8(2/0) | — | 12(2/1)+ |
| SUBI | Long | 16(3/0) | — | 20(3/2)+ |
| SUBQ | Byte, Word | 4(1/0) | 8(1/0)* | 8(1/1)+ |
| SUBQ | Long | 8(1/0) | 8(1/0) | 12(1/2)+ |

> **Note +:** Add effective address calculation time.
> **Note *:** ADDQ/SUBQ to An: 4(1/0) for byte/word, 8(1/0) for long (both sizes).

---

## §5 Single Operand Instruction Execution Times

Add EA calculation time where `+` is shown.

| Instruction | Size | Register | Memory |
|---|---|:---:|:---:|
| CLR | Byte, Word | 4(1/0) | 8(1/1)+ |
| CLR | Long | 6(1/0) | 12(1/2)+ |
| NBCD | Byte | 6(1/0) | 8(1/1)+ |
| NEG | Byte, Word | 4(1/0) | 8(1/1)+ |
| NEG | Long | 6(1/0) | 12(1/2)+ |
| NEGX | Byte, Word | 4(1/0) | 8(1/1)+ |
| NEGX | Long | 6(1/0) | 12(1/2)+ |
| NOT | Byte, Word | 4(1/0) | 8(1/1)+ |
| NOT | Long | 6(1/0) | 12(1/2)+ |
| Scc | Byte, False | 4(1/0) | 8(1/1)+ |
| Scc | Byte, True | 6(1/0) | 8(1/1)+ |
| TAS | Byte | 4(1/0) | 14(2/1)+ |
| TST | Byte, Word | 4(1/0) | 4(1/0)+ |
| TST | Long | 4(1/0) | 4(1/0)+ |

> **Note +:** Add effective address calculation time.

---

## §6 Shift and Rotate Instruction Execution Times

`n` is the shift count. Add EA calculation time where `+` is shown.

| Instruction | Size | Register | Memory |
|---|---|:---:|:---:|
| ASR, ASL | Byte, Word | 6+2n(1/0) | 8(1/1)+ |
| ASR, ASL | Long | 8+2n(1/0) | — |
| LSR, LSL | Byte, Word | 6+2n(1/0) | 8(1/1)+ |
| LSR, LSL | Long | 8+2n(1/0) | — |
| ROR, ROL | Byte, Word | 6+2n(1/0) | 8(1/1)+ |
| ROR, ROL | Long | 8+2n(1/0) | — |
| ROXR, ROXL | Byte, Word | 6+2n(1/0) | 8(1/1)+ |
| ROXR, ROXL | Long | 8+2n(1/0) | — |

> **Note +:** Add effective address calculation time (word operands only for memory form).
> **Note:** Long memory shifts are not supported.

---

## §7 Bit Manipulation Instruction Execution Times

Add EA calculation time where `+` is shown.

| Instruction | Size | Dynamic Register | Dynamic Memory | Static Register | Static Memory |
|---|---|:---:|:---:|:---:|:---:|
| BCHG | Byte | — | 8(1/1)+ | — | 12(2/1)+ |
| BCHG | Long | 8(1/0)* | — | 12(2/0)* | — |
| BCLR | Byte | — | 8(1/1)+ | — | 12(2/1)+ |
| BCLR | Long | 10(1/0)* | — | 14(2/0)* | — |
| BSET | Byte | — | 8(1/1)+ | — | 12(2/1)+ |
| BSET | Long | 8(1/0)* | — | 12(2/0)* | — |
| BTST | Byte | — | 4(1/0)+ | — | 8(2/0)+ |
| BTST | Long | 6(1/0) | — | 10(2/0) | — |

> **Note +:** Add effective address calculation time.
> **Note *:** Indicates maximum value; data addressing mode only.

---

## §8 Conditional Instruction Execution Times

| Instruction | Condition / Displacement | Branch Taken | Branch Not Taken |
|---|---|:---:|:---:|
| Bcc | Byte displacement | 10(2/0) | 8(1/0) |
| Bcc | Word displacement | 10(2/0) | 12(2/0) |
| BRA | Byte displacement | 10(2/0) | — |
| BRA | Word displacement | 10(2/0) | — |
| BSR | Byte displacement | 18(2/2) | — |
| BSR | Word displacement | 18(2/2) | — |
| DBcc | cc true | — | 12(2/0) |
| DBcc | cc false, count not expired | 10(2/0) | — |
| DBcc | cc false, counter expired | — | 14(3/0) |

---

## §9 JMP, JSR, LEA, PEA, and MOVEM Instruction Execution Times

`n` is the number of registers to transfer. The size of the index register (Xn) does not affect execution time.

| Instruction | Size | (An) | (An)+ | –(An) | (d16,An) | (d8,An,Xn) | (xxx).W | (xxx).L | (d16,PC) | (d8,PC,Xn) |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| JMP | — | 8(2/0) | — | — | 10(2/0) | 14(3/0) | 10(2/0) | 12(3/0) | 10(2/0) | 14(3/0) |
| JSR | — | 16(2/2) | — | — | 18(2/2) | 22(2/2) | 18(2/2) | 20(3/2) | 18(2/2) | 22(2/2) |
| LEA | — | 4(1/0) | — | — | 8(2/0) | 12(2/0) | 8(2/0) | 12(3/0) | 8(2/0) | 12(2/0) |
| PEA | — | 12(1/2) | — | — | 16(2/2) | 20(2/2) | 16(2/2) | 20(3/2) | 16(2/2) | 20(2/2) |
| MOVEM M→R | Word | 12+4n(3+n/0) | 12+4n(3+n/0) | — | 16+4n(4+n/0) | 18+4n(4+n/0) | 16+4n(4+n/0) | 20+4n(5+n/0) | 16+4n(4+n/0) | 18+4n(4+n/0) |
| MOVEM M→R | Long | 12+8n(3+2n/0) | 12+8n(3+2n/0) | — | 16+8n(4+2n/0) | 18+8n(4+2n/0) | 16+8n(4+2n/0) | 20+8n(5+2n/0) | 16+8n(4+2n/0) | 18+8n(4+2n/0) |
| MOVEM R→M | Word | 8+4n(2/n) | — | 8+4n(2/n) | 12+4n(3/n) | 14+4n(3/n) | 12+4n(3/n) | 16+4n(4/n) | — | — |
| MOVEM R→M | Long | 8+8n(2/2n) | — | 8+8n(2/2n) | 12+8n(3/2n) | 14+8n(3/2n) | 12+8n(3/2n) | 16+8n(4/2n) | — | — |

---

## §10 Multiprecision Instruction Execution Times

Times include fetching both operands, performing the operation, storing the result, and reading the next instruction.

Notation: **Dn** = data register operand; **M** = memory operand (using `–(An)` addressing).

| Instruction | Size | op Dn, Dn | op M, M |
|---|---|:---:|:---:|
| ADDX | Byte, Word | 4(1/0) | 18(3/1) |
| ADDX | Long | 8(1/0) | 30(5/2) |
| CMPM | Byte, Word | — | 12(3/0) |
| CMPM | Long | — | 20(5/0) |
| SUBX | Byte, Word | 4(1/0) | 18(3/1) |
| SUBX | Long | 8(1/0) | 30(5/2) |
| ABCD | Byte | 6(1/0) | 18(3/1) |
| SBCD | Byte | 6(1/0) | 18(3/1) |

---

## §11 Miscellaneous Instruction Execution Times

Add EA calculation time where `+` is shown.

| Instruction | Size | Register | Memory |
|---|---|:---:|:---:|
| ANDI to CCR | Byte | 20(3/0) | — |
| ANDI to SR | Word | 20(3/0) | — |
| CHK (no trap) | — | 10(1/0)+ | — |
| EORI to CCR | Byte | 20(3/0) | — |
| EORI to SR | Word | 20(3/0) | — |
| ORI to CCR | Byte | 20(3/0) | — |
| ORI to SR | Word | 20(3/0) | — |
| MOVE from SR | — | 6(1/0) | 8(1/1)+ |
| MOVE to CCR | — | 12(1/0) | 12(1/0)+ |
| MOVE to SR | — | 12(2/0) | 12(2/0)+ |
| EXG | — | 6(1/0) | — |
| EXT | Word | 4(1/0) | — |
| EXT | Long | 4(1/0) | — |
| LINK | — | 16(2/2) | — |
| MOVE from USP | — | 4(1/0) | — |
| MOVE to USP | — | 4(1/0) | — |
| NOP | — | 4(1/0) | — |
| RESET | — | 132(1/0) | — |
| RTE | — | 20(5/0) | — |
| RTR | — | 20(2/0) | — |
| RTS | — | 16(4/0) | — |
| STOP | — | 4(0/0) | — |
| SWAP | — | 4(1/0) | — |
| TRAPV | — | 4(1/0) | — |
| UNLK | — | 12(3/0) | — |

> **Note +:** Add effective address calculation time.

### MOVEP

| Instruction | Size | Register → Memory | Memory → Register |
|---|---|:---:|:---:|
| MOVEP | Word | 16(2/2) | 16(4/0) |
| MOVEP | Long | 24(2/4) | 24(6/0) |

---

## §12 Exception Processing Execution Times

Times include all stacking, the vector fetch, and the fetch of the first instruction of the handler routine. Add EA calculation time where `+` is shown.

| Exception | Clock Periods |
|---|:---:|
| Address Error | 50(4/7) |
| Bus Error | 50(4/7) |
| CHK Instruction | 40(4/3)+ |
| Divide by Zero | 38(4/3)+ |
| Illegal Instruction | 34(4/3) |
| Interrupt | 44(5/3)* |
| Privilege Violation | 34(4/3) |
| RESET | 40(6/0)** |
| Trace | 34(4/3) |
| TRAP Instruction | 34(4/3) |
| TRAPV Instruction | 34(5/3) |

> **Note +:** Add effective address calculation time.
> **Note *:** Interrupt acknowledge cycle assumed to take 4 clock periods.
> **Note \*\*:** Time from when RESET and HALT are first sampled negated to when instruction execution starts.
