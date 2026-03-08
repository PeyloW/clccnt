# clccnt — MC68k Clock Cycle Counter

A command-line tool that counts clock cycles for MC68k assembly instructions.
Useful for optimizing time-critical routines on the Atari ST and other 68k-based systems.

## Build & install

Requires a C++23 compiler (clang++ or g++).

```
make
make install                    # installs to /usr/local/bin
make PREFIX=~/.local install    # or a custom prefix
```

## Usage

### Single instruction

```
$ clccnt -i "move.l d0,(a0)+"
  move.l d0,(a0)+                          12 cycles
```

Variable-timing instructions (MUL, DIV, shifts, Scc) are estimated at a configurable
point between best and worst case. Use `-e` to adjust (0.0 = best, 1.0 = worst, default 0.5).
When a constant operand is given, the exact cycle count is used:

```
$ clccnt -i "mulu #160,d0"
  mulu #160,d0                             46 cycles

$ clccnt -e 0.0 -i "mulu d0,d1"
  mulu d0,d1                               40 cycles

$ clccnt -e 1.0 -i "mulu d0,d1"
  mulu d0,d1                               72 cycles
```

Conditional branches show taken and not-taken costs regardless of `-e`:

```
$ clccnt -i "bne foo"
  bne foo                                  10-12 cycles
```

### Source file

```
$ clccnt file.s
$ cat file.s | clccnt
```

Default output shows the cycle range per function. Use `-v` for per-instruction
detail with block structure and path enumeration:

```
$ clccnt -v file.s
function my_func:
  block 0: my_func
     2:   tst.w d0                             4
     3:   beq .skip                            8    12
  block 1:
     4:   add.w d0,d1                          4
  block 2: .skip
     6:   rts                                 16
                                     path 0>2:    32
                                   path 0>1>2:    32
```

Loops are indented, with iteration counts in the path notation. Nested loops
nest further:

```
$ clccnt -v fill_screen.s
function fill_screen:
    block 0: fill_screen
     4:   moveq #79,d1                          4
      block 1: .xloop
     6:     move.b d0,(a0)+                     8
     7:     dbf d1,.xloop                      16    12
    block 2:
     8:   dbf d2,.yloop                        16    12
  block 3:
     9: rts                                    16
                      path (0>(1)*4>2)*4>3:   240
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-i INST` | Count cycles for a single instruction | — |
| `-c CPU` | CPU model: 000, 010, 020, 030, 040, 060 | 000 |
| `-v` | Verbose: show blocks and per-instruction detail | off |
| `-e F` | Estimate factor 0.0–1.0 for variable-timing instructions | 0.5 |
| `-b N` | Bus cycles for rounding | from CPU |
| `-w N` | Bus width in bytes | from CPU |
| `-n N` | Max loop iterations for path analysis | 4 |
| `-h` | Show help | — |

## CPU models

| CPU | Bus | Width | Cache | Pipeline |
|-----|-----|-------|-------|----------|
| 68000 | 4 cycles | 16-bit | — | — |
| 68010 | 4 cycles | 16-bit | — | — |
| 68020 | 2 cycles | 32-bit | 256B I-cache | — |
| 68030 | 2 cycles | 32-bit | 256B I+D cache | — |
| 68040 | 2 cycles | 32-bit | 4KB I+D cache | Stall detection |
| 68060 | 1 cycle | 32-bit | 8KB I+D cache | Dual-issue pairing |

## Syntax support

All three common m68k assembly dialects are accepted:

| Form | Motorola | GCC/GAS | MIT |
|------|----------|---------|-----|
| Register | `d0`, `a0` | `%d0`, `%a0` | `%d0`, `%a0` |
| Indirect | `(a0)` | `(%a0)` | `%a0@` |
| Post-inc | `(a0)+` | `(%a0)+` | `%a0@+` |
| Pre-dec | `-(a0)` | `-(%a0)` | `%a0@-` |
| Displacement | `8(a0)` | `8(%a0)` or `(8,%a0)` | `%a0@(8)` |
| Indexed | `8(a0,d0.w)` | `8(%a0,%d0.w)` or `(8,%a0,%d0.w)` | `%a0@(8,%d0:w)` |

MIT mnemonic suffixes (`movl` -> `move.l`), aliases (`jbsr` -> `bsr`), and `fp` -> `a6` are also handled.
