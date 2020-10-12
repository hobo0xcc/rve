#include "rve.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void ResetState(State *state) {
    memset(state->x, 0, sizeof(state->x));
    state->pc = 0;
    state->excepted = false;
    state->exception_code = 0;
    memset(state->csr, 0, sizeof(state->csr));
    state->uart_mem[5] |= 0x20;
    state->mode = MACHINE;
}

void LoadBinaryIntoMemory(State *state, uint8_t *bin, size_t bin_size,
                          uint64_t load_addr) {
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

void TakeTrap(State *state) {
    uint8_t prev_mode = state->mode;
    uint8_t cause = state->exception_code;
    printf("Trap\n");
    if (state->mode <= SUPERVISOR && state->csr[MEDELEG] >> cause & 1) {
        state->mode = SUPERVISOR;
        state->csr[SEPC] = state->pc;
        state->pc = state->csr[STVEC] & ~1;
        state->csr[MCAUSE] = state->exception_code;
        state->csr[STVAL] = 0;
        WriteCSR(state, SSTATUS, 5, 5, ReadCSR(state, SSTATUS, 1, 1));
        WriteCSR(state, SSTATUS, 1, 1, 0);
        if (prev_mode == USER) {
            WriteCSR(state, SSTATUS, 8, 8, 0);
        } else {
            WriteCSR(state, SSTATUS, 8, 8, 1);
        }
    } else {
        state->mode = MACHINE;
        state->csr[MEPC] = state->pc;
        state->pc = state->csr[MTVEC] & ~1;
        state->csr[MCAUSE] = state->exception_code;
        state->csr[MTVAL] = 0;
        // Save current Mode into CSRs[mstatus].MPP.
        WriteCSR(state, MSTATUS, 11, 12, prev_mode);
    }
}

void CPUMain(State *state, uint64_t start_addr, size_t code_size,
             bool is_debug) {
    uint64_t count = 0;
    for (state->pc = start_addr; ;) {
        count++;
        if (is_debug && count >= 10000)
            return;
        if (state->pc == 0) {
            break;
        }
        uint32_t instr = Fetch32(state, state->pc);
        if (!state->excepted) {
            printf("pc: %llx\n", state->pc);
            ExecInstruction(state, instr);
        }
        state->x[0] = 0;
        if (state->excepted) {
            TakeTrap(state);
            state->excepted = false;
            state->exception_code = 0;
        }
    }
}

void PrintRegisters(State *state, bool is_debug) {
    for (int i = 0; i < 32; i++) {
        printf("x%d:\t %lld\n", i, state->x[i]);
    }
    printf("pc: %llx\n", state->pc);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        Error("Missing argument, at least 1 argument required.");
    }

    int prog_name_idx = 1;
    bool is_debug = false;
    if (!strcmp(argv[1], "--debug")) {
        RunTest();
        is_debug = true;
        prog_name_idx++;
    }

    uint8_t *bin;
    size_t size = ReadBinaryFile(argv[prog_name_idx], &bin);
    State *state = NewState(0x8010000/*0x7A12000*/);
    ResetState(state);

    uint64_t addr = LoadElf(state, size, bin);

    state->x[1] = (uint64_t)(-2);

    CPUMain(state, addr, size, is_debug);

    PrintRegisters(state, is_debug);
    int32_t result;
    result = state->x[10];

    free(bin);
    free(state->mem);
    free(state);
    return result;
}
