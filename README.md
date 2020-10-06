# rve: RISC-V Emulator written in C

rve is a RISC-V Emulator written in C which implements RV64IMAC currently and created from scratch.
`rve` is an abbreviation of "Risc-V Emulator".

# Build

```bash
$ make
```

# Usage

```
rve [--debug] file
```

rve run the ELF-Executable `file` and then prints registers and program-counter.
The `--debug` option is used to run the tests of riscv-tests.
If the `--debug` option is given, rve will return the value of gp(x3) register.

# Test

Run the tests of riscv-tests.
Currently it runs rv64ui-p-* only.

```bash
$ ./test.sh
```
