# rve: RISC-V Emulator written in C

rve is a RISC-V Emulator written in C which implements RV64IMAC currently and created from scratch.
`rve` is an abbreviation of "Risc-V Emulator".

![Demo](./demo.gif)

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

# Test

Run the tests of riscv-tests.
Currently it runs rv64ui-p-* only.

```bash
$ ./test.sh
```
