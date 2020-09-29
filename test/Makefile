RISCVARCH:=-march=rv64g

RISCVAS:=riscv64-unknown-elf-as
RISCVASFLAGS:=$(RISCVARCH)
RISCVOBJCOPY:=riscv64-unknown-elf-objcopy
RISCVCC:=riscv64-unknown-elf-gcc
RISCVCCFLAGS:=-g -ffreestanding -O0 $(RISCVARCH) -Wl,--gc-sections -nostartfiles -nostdlib -nodefaultlibs
RISCVLD:=riscv64-unknown-elf-ld
RISCVLDFLAGS:=-e _start
RM:=rm -rf

SRC:=$(wildcard *.s)
SRC_C:=$(wildcard *.c)
OBJ:=$(SRC:.s=.o) $(SRC_C:.c=.o)
BIN:=$(SRC:.s=.bin) $(SRC_C:.c=.bin)
RUNTIME:=runtime/crt0.s runtime/stack.c

$(BIN): $(OBJ)
	$(foreach obj, $(OBJ), $(eval $(shell $(RISCVLD) $(RICSVLDFLAGS) -o $(obj:.o=.bin) $(obj))))

$(OBJ): $(SRC) $(SRC_C)
	$(foreach src, $(SRC), $(eval $(shell $(RISCVAS) $(RISCVASFLAGS) -o $(src:.s=.o) $(src))))
	$(foreach src, $(SRC_C), $(eval $(shell $(RISCVCC) $(RISCVCCFLAGS) -o $(src:.c=.o) $(src) $(RUNTIME)))) 

clean:
	$(RM) *.o *.bin
