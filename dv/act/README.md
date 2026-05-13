# Minion ACT4 Integration

This directory integrates Minion with the RISC-V Architectural Certification
Tests (ACTs) using ACT4 release `4.0.0` and its required Sail RISC-V `0.10`.

The PR-gated ACT set is:

```text
I,M,Zca,Zicsr,Zifencei
```

Those are the scalar ACTs that match the current Minion Verilator integration
with `VpuEn=0`. Floating-point and privileged/timer/interrupt tests are left out
until those platform contracts are exposed and passing through the same runner.

Run locally:

```bash
make act-test
```

Useful overrides:

```bash
ACT_ROOT=/path/to/riscv-arch-test make -C dv/act test
ACT_EXTENSIONS=I make -C dv/act test
ACT_MAX_CYCLES=5000000 make -C dv/act run
```

`make -C dv/act test` fetches ACT4 and Sail locally under `build/act/tools` when
they are not already available. If `riscv64-unknown-elf-gcc` is not on `PATH`, it
installs the xPack GCC 15.2 RISC-V embedded toolchain under the same build tree
and creates `riscv64-unknown-elf-*` shims for ACT4.

The ACT simulator uses the ECP5 `prim_mul_div` by default because it is fully
flip-flop based and stable under full-core Verilator timing. The rest of the
primitive set remains the normal `TECH=generic` simulation selection unless
overridden.
