#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define XLEN 64
#define UART_BASE 0x10000000
#define UART_SIZE 0x100

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
    MISC_MEM = 0x0f,
    AMO = 0x2f,
};

enum Exception {
    IllegalInstruction = 2,
    EnvironmentCallFromUMode = 8,
    EnvironmentCallFromSMode = 9,
    EnvironmentCallFromMMode = 11,
};

enum CSR {
    SSTATUS = 0x100,
    SEPC = 0x141,
    SCAUSE = 0x142,
    STVAL = 0x143,

    MSTATUS = 0x300,
    MTVEC = 0x305,
    MEPC = 0x341,
    MCAUSE = 0x342,
    MTVAL = 0x343,
};

enum PrivilegeLevel {
    USER = 0x0,
    SUPERVISOR = 0x1,
    MACHINE = 0x3,
};

typedef struct State {
    uint64_t pc;
    uint64_t csr[4096];
    int64_t x[32];
    uint8_t *mem;
    uint8_t uart_mem[0x100];
    bool excepted;
    int exception_code;
    uint8_t mode;
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

void LoadBinaryIntoMemory(State *state, uint8_t *bin, size_t bin_size,
                          uint64_t load_addr);
uint64_t LoadElf(State *state, size_t size, uint8_t *bin);
void ExecInstruction(State *state, uint32_t instr);
int64_t SetNBits(int32_t n);
int64_t SetOneBit(int i);
void WriteCSR(State *state, uint16_t csr, uint8_t start_bit, uint8_t end_bit, uint64_t val);
uint64_t ReadCSR(State *state, uint16_t csr, uint8_t start_bit, uint8_t end_bit);
void Error(const char *fmt, ...);

State *NewState(size_t mem_size);
void ResetState(State *state);

void RunTest();
