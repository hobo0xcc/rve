#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define XLEN 64

enum OpCode {
    OP_IMM = 0x13,
    OP_LUI = 0x37,
    OP_AUIPC = 0x17,
    OP = 0x33,
    JAL = 0x6f,
    JALR = 0x67,
    LOAD = 0x03,
    STORE = 0x23,
    BRANCH = 0x63,
    OP_IMM_32 = 0x1b,
    OP_32 = 0x3b,
    SYSTEM = 0x73,
};

typedef struct State {
    uint64_t pc;
    int64_t csr[4096];
    int64_t x[32];
    uint8_t *mem;
} State;

typedef struct ExecData {
    unsigned long entry_addr;
    unsigned long virtual_addr;
    size_t text_size;
    uint8_t *text;
} ExecData;

typedef struct Vec {
    int cap;
    int len;
    void **data;
} Vec;

Vec *CreateVec();
void GrowVec(Vec *v, int n);
void PushVec(Vec *v, void *item);
void *GetVec(Vec *v, int idx);

void LoadBinaryIntoMemory(State *state, uint8_t *bin, size_t bin_size, uint64_t load_addr);
uint64_t LoadElf(State *state, size_t size, uint8_t *bin);
void ExecInstruction(State *state, uint32_t instr);
void Error(const char *fmt, ...);
