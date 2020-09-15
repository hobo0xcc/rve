#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rve.h"

void Error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

State *NewState(size_t mem_size) {
    State *state = calloc(1, sizeof(State));
    state->mem = calloc(1, mem_size);
    return state;
}

void LoadBinaryIntoMemory(State *state, uint8_t *bin, size_t bin_size, uint64_t load_addr) {
    memcpy(state->mem + load_addr, bin, bin_size);
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

void CPUMain(State *state, uint64_t start_addr, size_t code_size) {
    for (state->pc = start_addr; state->pc < start_addr + code_size; ) {
        ExecInstruction(state, *(uint32_t *)(state->mem + state->pc));
        state->x[0] = 0;
    }
}

void PrintRegisters(State *state) {
    for (int i = 0; i < 32; i++) {
        printf("x%d:\t %lld\n", i, state->x[i]);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        Error("Missing argument, at least 1 argument required.");
    }

    uint8_t *bin;
    size_t size = ReadBinaryFile(argv[1], &bin);
    State *state = NewState(0x7A12000);

    uint64_t addr = LoadElf(state, size, bin);

    state->x[1] = INT64_MAX;

    CPUMain(state, addr, size);

    PrintRegisters(state);
    int32_t result = state->x[10]; 

    free(bin);
    free(state->mem);
    free(state);
    return result;
}
