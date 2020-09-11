RISCVAS:=riscv64-unknown-elf-as
RISCVOBJCOPY:=riscv64-unknown-elf-objcopy
RM:=rm -rf

SRC:=$(wildcard *.s)
OBJ:=$(SRC:.s=.o)
BIN:=$(SRC:.s=.bin)

$(BIN): $(OBJ)
	$(foreach obj, $(OBJ), $(eval $(shell $(RISCVOBJCOPY) -O binary $(obj) $(obj:.o=.bin))))

$(OBJ): $(SRC)
	$(foreach src, $(SRC), $(eval $(shell $(RISCVAS) -march=rv64g -o $(src:.s=.o) $(src))))

clean:
	$(RM) *.o *.bin
