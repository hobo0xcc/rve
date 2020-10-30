LD:=gcc
CC:=gcc
LDFLAGS:=-lncurses -lcurses
CCFLAGS:=-std=c11 -g
RM:=rm -rf
MKDIR:=mkdir -p
SRC:=$(wildcard src/*.c)
OBJ:=$(SRC:.c=.o)
BINDIR:=bin
BIN:=rve

# .PHONY: $(BINDIR)/$(BIN)
$(BINDIR)/$(BIN): $(OBJ)
	$(MKDIR) $(BINDIR)
	$(LD) $(LDFLAGS) -o $@ $^

# .PHONY: %.o
$(OBJ): $(SRC)
	$(foreach src, $(SRC), $(eval $(shell $(CC) $(CCFLAGS) -c $(src) -o $(src:.c=.o))))

xv6:
	bin/rve ~/d/oss/riscv-rust/resources/xv6/kernel --disk ~/d/oss/riscv-rust/resources/xv6/fs.img

clean:
	$(RM) $(OBJ) $(BINDIR)
