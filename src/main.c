#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

typedef struct State {
    int32_t pc;
    int32_t x[32];
} State;

void Error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

State *NewState() {
    State *state = calloc(1, sizeof(State));
    return state;
}

int32_t SetNBits(int32_t n) {
    int32_t res = 0;
    for (int i = 0; i <= n; i++) {
        res |= 1 << i; 
    }

    return res;
}

void ExecAddi(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(4);
    uint8_t rs1 = (instr >> 15) & SetNBits(4);
    uint16_t immediate = (instr >> 20) & SetNBits(11);

    state->x[rd] = state->x[rs1] + immediate;
}

void ExecInstruction(State *state, uint32_t instr) {
    uint8_t opcode = instr & SetNBits(6);
    switch (opcode) {
    case 0x13:
        ExecAddi(state, instr);
        break;
    default:
        Error("Unknown opcode: %2x.", opcode);
    }
}

size_t ReadBinaryFile(const char *name, uint8_t **buf) {
    FILE *fp = fopen(name, "rb");
    if (fp == NULL) {
        Error("Can't open the file: %s.", name);
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *result = malloc(size);
    fread(result, size, 1, fp);
    *buf = result;
    fclose(fp);
    return size;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        Error("Missing argument, at least 1 argument required.");
    }

    uint8_t *bin;
    size_t size = ReadBinaryFile(argv[1], &bin);
    State *state = NewState();
    int instr_size = 4;
    for (state->pc = 0; state->pc < size; state->pc += instr_size) {
        ExecInstruction(state, *(uint32_t *)(bin + state->pc));
    }

    int32_t result = state->x[1];
    
    free(bin);
    free(state);
    return result;
}
