LD:=gcc
CC:=gcc
CCFLAGS:=-std=c11
RM:=rm -rf
MKDIR:=mkdir -p
SRC:=$(wildcard src/*.c)
OBJ:=$(SRC:.c=.o)
BINDIR:=bin
BIN:=rve

# .PHONY: $(BINDIR)/$(BIN)
$(BINDIR)/$(BIN): $(OBJ)
	$(MKDIR) $(BINDIR)
	$(LD) -o $@ $^

# .PHONY: %.o
$(OBJ): $(SRC)
	$(foreach src, $(SRC), $(eval $(shell $(CC) $(CCFLAGS) -c $(src) -o $(src:.c=.o))))

clean:
	$(RM) $(OBJ) $(BINDIR)
