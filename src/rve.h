#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define XLEN 64

#define DRAM_BASE 0x80000000

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

#define VALEN 39
#define PAGESIZE 4096
#define LEVELS 3
#define PTESIZE 8

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
    InstructionAddressMisaligned = 0,
    InstructionAccessFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAMOAddressMisaligned = 6,
    StoreAMOAccessFault = 7,
    EnvironmentCallFromUMode = 8,
    EnvironmentCallFromSMode = 9,
    EnvironmentCallFromMMode = 11,
    InstructionPageFault = 12,
    LoadPageFault = 13,
    StoreAMOPageFault = 15,
};

enum CSR {
    SSTATUS = 0x100,
    STVEC = 0x105,
    SEPC = 0x141,
    SCAUSE = 0x142,
    STVAL = 0x143,
    SATP = 0x180,

    MSTATUS = 0x300,
    MEDELEG = 0x302,
    MIDELEG = 0x303,
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

enum AccessType {
    AccessInstruction = 0,
    AccessLoad = 1,
    AccessStore = 2,
};

enum RV64Sv {
    Bare = 0,
    Sv39 = 8,
    Sv48 = 9,
    Sv57 = 10,
    Sv64 = 11,
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

uint64_t GetRange(uint64_t v, uint64_t start, uint64_t end);
uint64_t Translate(State *state, uint64_t v_addr, uint8_t access_type);
void Write8(State *state, uint64_t v_addr, uint8_t val);
void Write16(State *state, uint64_t v_addr, uint16_t val);
void Write32(State *state, uint64_t v_addr, uint32_t val);
void Write64(State *state, uint64_t v_addr, uint64_t val);
uint8_t Read8(State *state, uint64_t v_addr);
uint16_t Read16(State *state, uint64_t v_addr);
uint32_t Read32(State *state, uint64_t v_addr);
uint64_t Read64(State *state, uint64_t v_addr);
uint32_t Fetch32(State *state, uint64_t v_addr);

void LoadBinaryIntoMemory(State *state, uint8_t *bin, size_t bin_size,
                          uint64_t load_addr);
void PrintRegisters(State *state, bool is_debug);
uint64_t LoadElf(State *state, size_t size, uint8_t *bin);
void ExecInstruction(State *state, uint32_t instr);
int64_t SetNBits(int32_t n);
int64_t SetOneBit(int i);
void WriteCSR(State *state, uint16_t csr, uint8_t start_bit, uint8_t end_bit, uint64_t val);
uint64_t ReadCSR(State *state, uint16_t csr, uint8_t start_bit, uint8_t end_bit);
void Error(const char *fmt, ...);

void TakeTrap(State *state);
State *NewState(size_t mem_size);
void ResetState(State *state);

void RunTest();
