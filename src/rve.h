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

#define PLIC_BASE 0xc000000
#define PLIC_SIZE 0x4000000
#define PLIC_PRIORITY_BASE 0x04
#define PLIC_PRIORITY_SIZE 0xff8
#define PLIC_ENABLE_BASE 0x2080
#define PLIC_ENABLE_SIZE 0x08
#define PLIC_THRESHOLD_BASE 0x201000
#define PLIC_THRESHOLD_SIZE 0x04
#define PLIC_IRQ_BASE 0x201004
#define PLIC_IRQ_SIZE 0x04

#define CLINT_BASE 0x2000000
#define CLINT_SIZE 0x10000
#define CLINT_MSIP_BASE 0x00
#define CLINT_MSIP_SIZE 0x04
#define CLINT_MTIMECMP_BASE 0x4000
#define CLINT_MTIMECMP_SIZE 0x08
#define CLINT_MTIME_BASE 0xBFF8
#define CLINT_MTIME_SIZE 0x08

#define VIRTIO_BASE 0x10001000
#define VIRTIO_SIZE 0x1000
#define VIRTIO_MAGIC_VALUE_BASE 0x00
#define VIRTIO_DEVICE_VERSION_BASE 0x04
#define VIRTIO_DEVICE_ID_BASE 0x08
#define VIRTIO_VENDOR_ID_BASE 0x0c
#define VIRTIO_HOST_FEATURES_BASE 0x10
#define VIRTIO_GUEST_FEATURES_BASE 0x20
#define VIRTIO_GUEST_PAGE_SIZE_BASE 0x28
#define VIRTIO_QUEUE_NUM_MAX_BASE 0x34
#define VIRTIO_QUEUE_NUM_BASE 0x38
#define VIRTIO_QUEUE_ALIGN_BASE 0x3c
#define VIRTIO_QUEUE_PFN_BASE 0x40
#define VIRTIO_QUEUE_NOTIFY_BASE 0x50
#define VIRTIO_INTERRUPT_STATUS_BASE 0x60
#define VIRTIO_INTERRUPT_ACK_BASE 0x64
#define VIRTIO_STATUS_BASE 0x70

#define VIRTIO_NOTIFY 0x1234

#define DESC_NUM 8
#define VRING_DESC_SIZE 16

#define VIRTIO_IRQ 1
#define UART_IRQ 10

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
    USTATUS = 0x000,
    UIE = 0x004,
    UTVEC = 0x005,
    UEPC = 0x041,
    UCAUSE = 0x042,
    UTVAL = 0x043,
    UIP = 0x044,

    SSTATUS = 0x100,
    SIE = 0x104,
    STVEC = 0x105,
    SEPC = 0x141,
    SCAUSE = 0x142,
    STVAL = 0x143,
    SIP = 0x144,
    SATP = 0x180,

    MSTATUS = 0x300,
    MEDELEG = 0x302,
    MIDELEG = 0x303,
    MIE = 0x304,
    MTVEC = 0x305,
    MSCRATCH = 0x340,
    MEPC = 0x341,
    MCAUSE = 0x342,
    MTVAL = 0x343,
    MIP = 0x344,
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

typedef struct Uart {
    uint8_t uart_mem[UART_SIZE];
} Uart;

typedef struct Clint {
    uint32_t msip;
    uint64_t mtimecmp;
    uint64_t mtime;
} Clint;

typedef struct Plic {
    uint32_t priority[1024];
    uint32_t irq;
    uint64_t enable_bits;
    uint32_t threshold;
    uint32_t clain_complete;
} Plic;

typedef struct Virtio {
    uint32_t host_features;
    uint32_t guest_features;
    uint32_t guest_page_size;
    uint32_t queue_num_max;
    uint32_t queue_num;
    uint32_t queue_align;
    uint32_t queue_pfn;
    uint32_t queue_notify;
    uint32_t interrupt_status;
    uint32_t interrupt_ack;
    uint32_t status;
    uint32_t *config;

    uint32_t id;
    uint8_t *disk;
} Virtio;

typedef struct State {
    uint64_t pc;
    uint64_t csr[4096];
    int64_t x[32];
    uint8_t *mem;
    uint64_t clock;

    Uart *uart;
    Clint *clint;
    Plic *plic;
    Virtio *virtio;

    bool excepted;
    uint64_t exception_code;
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
void SetVec(Vec *v, int idx, void *item);

bool IsVirtioInterrupting(State *state);
uint64_t DescAddr(State *state);
uint8_t ReadDisk(State *state, uint64_t addr);
void WriteDisk(State *state, uint64_t addr, uint8_t value);
void DiskAccess(State *state);

void HandleTrap(State *state, uint64_t instr_addr);
bool HandleInterrupt(State *state, uint64_t instr_addr);

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

void Tick(State *state);

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
