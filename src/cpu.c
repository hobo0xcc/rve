#include "rve.h"
#include <stdint.h>
#include <stdio.h>
#include <curses.h>

// Set N-bit(s).
int64_t SetNBits(int32_t n) {
    if (n == 64)
        return ~((uint64_t)0);
    return ((uint64_t)1 << n) - 1;
}

// Set i-th bit.
int64_t SetOneBit(int i) { return ((uint64_t)1 << i); }

// v[start:end].
uint64_t GetRange(uint64_t v, uint64_t start, uint64_t end) {
    if (start < 0 || end >= 64) {
        Error("Out of range: GetRange");
    }
    
    uint64_t mask = SetNBits(end - start + 1);
    return (v & (mask << start)) >> start;
}

void HandleTrap(State *state, uint64_t instr_addr) {
    uint8_t prev_mode = state->mode;
    uint64_t cause = state->exception_code;
    // printf("Trap: addr: 0x%llx, cause: 0x%llx\n", instr_addr, cause);
    uint64_t mdeleg = MEDELEG;
    if (cause & SetOneBit(63)) {
        mdeleg = MIDELEG;
    }
    if (state->mode <= SUPERVISOR && state->csr[mdeleg] >> (cause & 0xffff) & 1) {
        state->mode = SUPERVISOR;
        state->csr[SEPC] = instr_addr;
        uint64_t vector = 0;
        if (ReadCSR(state, STVEC, 0, 0) == 1) {
            vector = 4 * (cause & SetNBits(63));
        }
        state->pc = (state->csr[STVEC] & ~1) + vector;
        state->csr[SCAUSE] = cause;
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
        state->csr[MEPC] = instr_addr;
        uint64_t vector = 0;
        if (ReadCSR(state, MTVEC, 0, 0) == 1) {
            vector = 4 * (cause & SetNBits(63));
        }
        state->pc = (state->csr[MTVEC] & ~1) + vector;
        state->csr[MCAUSE] = cause;
        state->csr[MTVAL] = 0;
        // Save current Mode into CSRs[mstatus].MPP.
        WriteCSR(state, MSTATUS, 11, 12, prev_mode);
    }
}

bool HandleInterrupt(State *state, uint64_t instr_addr) {
    uint16_t mint = 0;
    if (state->mode == MACHINE) {
        if (ReadCSR(state, MSTATUS, 3, 3) == 0) {
            return false;
        }

        mint = ReadCSR(state, MIP, 0, 15) & ReadCSR(state, MIE, 0, 15);
    } else if (state->mode == SUPERVISOR) {
        if (ReadCSR(state, SSTATUS, 1, 1) == 0) {
            return false;
        }

        mint = ReadCSR(state, SIP, 0, 15) & ReadCSR(state, SIE, 0, 15);
    } else if (state->mode == USER) {
        mint = ReadCSR(state, SIP, 0, 15) & ReadCSR(state, SIE, 0, 15);
    }
    if (mint == 0) {
        return false;
    }

    // uint16_t mint = ReadCSR(state, MIP, 0, 15) & ReadCSR(state, MIE, 0, 15);
    // printf("MIP: 0x%llx, MIE: 0x%llx\n", state->csr[MIP], state->csr[MIE]);
    uint64_t exception_code = 0;
    bool interrupted = true;
    if (mint & SetOneBit(11)) {
        exception_code = SetOneBit(63) | 11;
    } else if (mint & SetOneBit(9)) {
        exception_code = SetOneBit(63) | 9;
        // printf("Machine External Interrupt\n");
    } else if (mint & SetOneBit(7)) {
        exception_code = SetOneBit(63) | 7;
        // printf("mtimecmp: %llx, mtime: %llx\n", state->clint->mtimecmp, state->clint->mtime);
    } else if (mint & SetOneBit(5)) {
        exception_code = SetOneBit(63) | 5;
    } else if (mint & SetOneBit(3)) {
        exception_code = SetOneBit(63) | 3;
    } else if (mint & SetOneBit(1)) {
        exception_code = SetOneBit(63) | 1;
    } else {
        // There's no pending interrupt or the interrupt isn't implemented yet.
        interrupted = false;
    }

    if (interrupted) {
        state->excepted = true;
        state->exception_code = exception_code;
        HandleTrap(state, instr_addr);
        if (state->mode == MACHINE) {
            WriteCSR(state, MIP, exception_code & 0xffff, exception_code & 0xffff, 0);
        } else if (state->mode == SUPERVISOR) {
            WriteCSR(state, SIP, exception_code & 0xffff, exception_code & 0xffff, 0);
        } else if (state->mode == USER) {
            WriteCSR(state, SIP, exception_code & 0xffff, exception_code & 0xffff, 0);
        }
        return true;
    }

    return false;
}

void UartTick(State *state) {
    if (state->uart->rbr == 0) {
        int8_t val = getch();
        if (val != ERR && val != 0) {
            state->uart->rbr = val;
            state->uart->lsr |= 0x01;
        }
    }
    if (state->uart->thr != 0) {
        addch(state->uart->thr);
        state->uart->thr = 0;
        state->uart->lsr |= 0x20;
    }
}

void ClintTick(State *state) {
    state->clint->mtime++;

    WriteCSR(state, MIP, 3, 3, 0);
    if ((state->clint->msip & 1) != 0) {
        WriteCSR(state, MIP, 3, 3, 1);
    }

    WriteCSR(state, MIP, 7, 7, 0);
    if (state->clint->mtimecmp > 0 && state->clint->mtime >= state->clint->mtimecmp) {
        // state->clint->mtime = 0;
        // WriteCSR(state, MIP, 7, 7, 1);
    }
}

void PlicTick(State *state, bool virtio_int, bool uart_int) {
    uint32_t virtio_irq = VIRTIO_IRQ;
    uint32_t uart_irq = UART_IRQ;
    uint32_t virtio_priority = state->plic->priority[virtio_irq];
    uint32_t uart_priority = state->plic->priority[uart_irq];
    bool virtio_enable = state->plic->enable_bits >> virtio_irq & 1;
    bool uart_enable = state->plic->enable_bits >> uart_irq & 1;

    bool ints[2] = {virtio_int, uart_int};
    uint32_t irqs[2] = {virtio_irq, uart_irq};
    uint32_t priorities[2] = {virtio_priority, uart_priority};
    bool enables[2] = {virtio_enable, uart_enable};

    int priority = 0;
    int irq = 0;
    for (int i = 0; i < 2; i++) {
        // if (ints[i])
        //     printf("irqs[i]: %x, ints[i]: %d, enables[i]: %d, priorities[i]: %d, priority: %d, threshold: %d\n", irqs[i], ints[i], enables[i], priorities[i], priority, state->plic->threshold);
        if (ints[i] && enables[i] && priorities[i] > priority && priorities[i] > state->plic->threshold) {
            irq = irqs[i];
            priority = priorities[i];
        }
    }

    if (irq != 0) {
        // printf("Interrupted: irq: %d, virtio_int: %d\n", irq, virtio_int);
        state->plic->irq = irq;
        if (state->mode == MACHINE) {
            WriteCSR(state, MIP, 9, 9, 1);
        } else if (state->mode == SUPERVISOR) {
            WriteCSR(state, SIP, 9, 9, 1);
        }
    }
}

void VirtioTick(State *state) {
    if (state->virtio->queue_notify != 0x1234) {
        // printf("virtio enabled: pc: %llx\n", state->pc);
        state->virtio->interrupt_status |= 1;
        DiskAccess(state);
        state->virtio->queue_notify = 0x1234;
    }
}

void Tick(State *state) {
    state->clock++;
    uint8_t b_mode = state->mode;
    uint64_t b_sepc = state->csr[SEPC];
    uint64_t pc = state->pc;
    uint32_t instr = Fetch32(state, state->pc);
    
    ExecInstruction(state, instr);
    state->x[0] = 0;

    if (state->excepted) {
        HandleTrap(state, pc);
        state->excepted = false;
        state->exception_code = 0;
    }

    UartTick(state);
    VirtioTick(state);
    ClintTick(state);
    PlicTick(state, IsVirtioInterrupting(state), IsUartInterrupting(state));
    bool interrupted = HandleInterrupt(state, state->pc);
    if (interrupted && state->excepted) {
        state->excepted = false;
        state->exception_code = 0;
    }

    if (state->csr[SEPC] != b_sepc) {
        // printf("sepc changed: pc: 0x%llx, 0x%llx to 0x%llx\n", Translate(state, state->pc, AccessInstruction), b_sepc, state->csr[SEPC]);
    }
    if (state->mode != b_mode) {
        // printf("mode changed: pc: 0x%llx, %x to %x\n", Translate(state, state->pc, AccessInstruction), b_mode, state->mode);
    }
}

uint64_t WriteRange8(uint64_t dest, uint8_t val, uint64_t start) {
    uint8_t mask = dest & (SetNBits(8) << (start * 8));
    dest ^= mask;
    dest |= val << (start * 8);
    return dest;
}

uint8_t ReadRange8(uint64_t src, uint64_t start) {
    return (src & (SetNBits(8) << (start * 8))) >> (start * 8);
}

void UartWrite(State *state, uint64_t offset, uint8_t val) {
    // if (offset == 0) {
    //     state->uart->uart_mem[offset] = ch;
    //     if (state->uart->uart_mem[3] & SetOneBit(7)) {
    //         return;
    //     }
    //     putc(ch, stdout);
    //     fflush(stdout);
    // } else {
    //     state->uart->uart_mem[offset] = ch;
    // }

    uint8_t dlab = state->uart->lcr >> 7 & 1;
    if (offset == UART_THR && dlab == 0) {
        state->uart->thr = val;
        // Set THRE to 0.
        // THRE indicate that whether the thr register is zero.
        state->uart->lsr &= ~0x20;
    } else if (offset == UART_IER && dlab == 0) {
        state->uart->ier = val;
    } else if (offset == UART_LCR) {
        state->uart->lcr = val;
    } else if (offset == UART_MCR) {
        state->uart->mcr = val;
    } else if (offset == UART_SCR) {
        state->uart->scr = val;
    }
}

uint8_t UartRead(State *state, uint64_t offset) {
    // if (offset == 0) {
    //     if (state->uart->uart_mem[3] & SetOneBit(7)) {
    //         return state->uart->uart_mem[offset];
    //     }
    //     uint8_t ch = getc(stdin);
    //     state->uart->uart_mem[offset] = ch;
    //     return ch;
    // } else {
    //     return state->uart->uart_mem[offset];
    // }

    uint8_t dlab = state->uart->lcr >> 7 & 1;
    if (offset == UART_RBR && dlab == 0) {
        uint8_t rbr = state->uart->rbr;
        state->uart->rbr = 0;
        state->uart->lsr &= ~0x01;
        return rbr;
    } else if (offset == UART_IER && dlab == 0) {
        return state->uart->ier;
    } else if (offset == UART_IIR) {
        return state->uart->iir;
    } else if (offset == UART_LCR) {
        return state->uart->lcr;
    } else if (offset == UART_MCR) {
        return state->uart->mcr;
    } else if (offset == UART_LSR) {
        return state->uart->lsr;
    } else if (offset == UART_MSR) {
        return state->uart->msr;
    } else if (offset == UART_SCR) {
        return state->uart->scr;
    } else {
        return 0;
    }
}

void ClintWrite(State *state, uint64_t offset, uint8_t val) {
    if (offset >= CLINT_MSIP_BASE && offset < CLINT_MSIP_BASE + CLINT_MSIP_SIZE) {
        state->clint->msip = WriteRange8(state->clint->msip, val, offset - CLINT_MSIP_BASE);
    } else if (offset >= CLINT_MTIMECMP_BASE && offset < CLINT_MTIMECMP_BASE + CLINT_MTIMECMP_SIZE) {
        state->clint->mtimecmp = WriteRange8(state->clint->mtimecmp, val, offset - CLINT_MTIMECMP_BASE);
    } else if (offset >= CLINT_MTIME_BASE && offset < CLINT_MTIME_BASE + CLINT_MTIME_SIZE) {
        state->clint->mtime = WriteRange8(state->clint->mtime, val, offset - CLINT_MTIME_BASE);
    } else {
        // Do nothing.
    }
}

uint8_t ClintRead(State *state, uint64_t offset) {
    if (offset >= CLINT_MSIP_BASE && offset < CLINT_MSIP_BASE + CLINT_MSIP_SIZE) {
        return ReadRange8(state->clint->msip, offset - CLINT_MSIP_BASE);
    } else if (offset >= CLINT_MTIMECMP_BASE && offset < CLINT_MTIMECMP_BASE + CLINT_MTIMECMP_SIZE) {
        return ReadRange8(state->clint->mtimecmp, offset - CLINT_MTIMECMP_BASE);
    } else if (offset >= CLINT_MTIME_BASE && offset < CLINT_MTIME_BASE + CLINT_MTIME_SIZE) {
        return ReadRange8(state->clint->mtime, offset - CLINT_MTIME_BASE);
    } else {
        return 0;
    }
}

void PlicWrite(State *state, uint64_t offset, uint8_t val) {
    if (offset >= PLIC_PRIORITY_BASE && offset < PLIC_PRIORITY_BASE + PLIC_PRIORITY_SIZE) {
        state->plic->priority[(offset - PLIC_PRIORITY_BASE + 4) / 4] = WriteRange8(state->plic->priority[(offset - PLIC_PRIORITY_BASE + 4) / 4], val, (offset - PLIC_PRIORITY_BASE + 4) % 4);
    } else if (offset >= PLIC_ENABLE_BASE && offset < PLIC_ENABLE_BASE + PLIC_ENABLE_SIZE) {
        state->plic->enable_bits = WriteRange8(state->plic->enable_bits, val, offset - PLIC_ENABLE_BASE);
    } else if (offset >= PLIC_THRESHOLD_BASE && offset < PLIC_THRESHOLD_BASE + PLIC_THRESHOLD_SIZE) {
        state->plic->threshold = WriteRange8(state->plic->threshold, val, offset - PLIC_THRESHOLD_BASE);
    } else if (offset >= PLIC_IRQ_BASE && offset < PLIC_IRQ_BASE + PLIC_IRQ_SIZE) {
        state->plic->irq = WriteRange8(state->plic->irq, val, offset - PLIC_IRQ_BASE);
    }
}

uint8_t PlicRead(State *state, uint64_t offset) {
    if (offset >= PLIC_PRIORITY_BASE && offset < PLIC_PRIORITY_BASE + PLIC_PRIORITY_SIZE) {
        return ReadRange8(state->plic->priority[(offset - PLIC_PRIORITY_BASE) / 4], (offset - PLIC_PRIORITY_BASE) % 4);
    } else if (offset >= PLIC_ENABLE_BASE && offset < PLIC_ENABLE_BASE + PLIC_ENABLE_SIZE) {
        return ReadRange8(state->plic->enable_bits, offset - PLIC_ENABLE_BASE);
    } else if (offset >= PLIC_THRESHOLD_BASE && offset < PLIC_THRESHOLD_BASE + PLIC_THRESHOLD_SIZE) {
        return ReadRange8(state->plic->threshold, offset - PLIC_THRESHOLD_BASE);
    } else if (offset >= PLIC_IRQ_BASE && offset < PLIC_IRQ_BASE + PLIC_IRQ_SIZE) {
        return ReadRange8(state->plic->irq, offset - PLIC_IRQ_BASE);
    } else {
        return 0;
    }
}

bool IsUartInterrupting(State *state) {
    if ((state->uart->ier & 0x1) != 0) {
        if (state->uart->rbr != 0) {
            state->uart->iir = 0x04;
            return true;
        }
    }

    if ((state->uart->ier & 0x2) != 0) {
        if (state->uart->thr == 0) {
            state->uart->iir = 0x02;
            return true;
        }
    }

    state->uart->iir = 0xf;
    return false;
}

bool IsVirtioInterrupting(State *state) {
    if ((state->virtio->interrupt_status & 1) == 1) {
        return true;
    }
    return false;
}

uint64_t GetNewId(State *state) {
    state->virtio->id += 1;
    return state->virtio->id;
}

uint64_t DescAddr(State *state) {
    return (uint64_t)state->virtio->queue_pfn * (uint64_t)state->virtio->guest_page_size;
}

uint8_t ReadDisk(State *state, uint64_t addr) {
    return state->virtio->disk[addr];
}

void WriteDisk(State *state, uint64_t addr, uint8_t value) {
    state->virtio->disk[addr] = value;
}

void DiskAccess(State *state) {
    // printf("Disk Access\n");
    uint64_t desc_addr = DescAddr(state);
    // printf("rve: desc: %lld\n", (int64_t)desc_addr);
    uint64_t avail_addr = DescAddr(state) + 8 * 16;
    // printf("rve: avail: %lld\n", (int64_t)avail_addr);
    uint64_t used_addr = DescAddr(state) + 4096;
    // printf("rve: used: %lld\n", (int64_t)used_addr);

    uint32_t queue_size = state->virtio->queue_num;
    uint16_t offset = Read16(state, avail_addr);
    uint16_t index = Read16(state, avail_addr + 2);
    uint64_t index_addr = avail_addr + 4 + (uint64_t)state->virtio->id * 2 /*desc_addr + VRING_DESC_SIZE * index*/;
    uint64_t head_index = Read16(state, index_addr) % queue_size;
    uint64_t desc_addr0 = desc_addr + VRING_DESC_SIZE * head_index;
    uint64_t addr0 = Read64(state, desc_addr0);
    uint16_t next0 = Read16(state, desc_addr0 + 14);
    uint64_t desc_addr1 = desc_addr + VRING_DESC_SIZE * next0;
    uint64_t addr1 = Read64(state, desc_addr1);
    uint32_t len1 = Read32(state, desc_addr1 + 8);
    uint16_t flags1 = Read16(state, desc_addr1 + 12);
    uint16_t next1 = Read64(state, desc_addr1 + 14);
    uint64_t desc_addr2 = desc_addr + VRING_DESC_SIZE * next1;
    uint64_t addr2 = Read64(state, desc_addr2);
    uint32_t len2 = Read32(state, desc_addr2 + 8);
    uint16_t flags2 = Read16(state, desc_addr2 + 12);

    uint64_t blk_sector = Read64(state, addr0 + 8);

    if ((flags1 & 2) == 0) {
        for (uint64_t i = 0; i < len1; i++) {
            uint8_t data = Read8(state, addr1 + i);
            WriteDisk(state, blk_sector * 512 + i, data);
        }
    } else {
        for (uint64_t i = 0; i < len1; i++) {
            uint8_t data = ReadDisk(state, blk_sector * 512 + i);
            Write8(state, addr1 + i, data);
            // printf("DiskRead disk_addr: %llx, addr: %llx, %c\n", blk_sector * 512 + i, addr1 + i, data);
        }
    }

    if (flags2 & 2) {
        Write8(state, addr2, 0);
    } else {
        Error("Third descriptor must be write mode");
    }

    uint64_t new_id = GetNewId(state) % queue_size;
    Write16(state, used_addr + 2, new_id % 8);
}

void VirtioWrite(State *state, uint64_t offset, uint8_t val) {
    if (offset >= VIRTIO_GUEST_FEATURES_BASE && offset < VIRTIO_GUEST_FEATURES_BASE + 4) {
        state->virtio->guest_features = WriteRange8(state->virtio->guest_features, val, offset - VIRTIO_GUEST_FEATURES_BASE);
        return;
    } else if (offset >= VIRTIO_GUEST_PAGE_SIZE_BASE && offset < VIRTIO_GUEST_PAGE_SIZE_BASE + 4) {
        state->virtio->guest_page_size = WriteRange8(state->virtio->guest_page_size, val, offset - VIRTIO_GUEST_PAGE_SIZE_BASE);
        return;
    } else if (offset >= VIRTIO_QUEUE_NUM_BASE && offset < VIRTIO_QUEUE_NUM_BASE + 4) {
        state->virtio->queue_num = WriteRange8(state->virtio->queue_num, val, offset - VIRTIO_QUEUE_NUM_BASE);
        return;
    } else if (offset >= VIRTIO_QUEUE_ALIGN_BASE && offset < VIRTIO_QUEUE_ALIGN_BASE + 4) {
        state->virtio->queue_align = WriteRange8(state->virtio->queue_align, val, offset - VIRTIO_QUEUE_ALIGN_BASE);
        return;
    } else if (offset >= VIRTIO_QUEUE_PFN_BASE && offset < VIRTIO_QUEUE_PFN_BASE + 4) {
        state->virtio->queue_pfn = WriteRange8(state->virtio->queue_pfn, val, offset - VIRTIO_QUEUE_PFN_BASE);
        return;
    } else if (offset >= VIRTIO_QUEUE_NOTIFY_BASE && offset < VIRTIO_QUEUE_NOTIFY_BASE + 4) {
        state->virtio->queue_notify = WriteRange8(state->virtio->queue_notify, val, offset - VIRTIO_QUEUE_NOTIFY_BASE);
        return;
    } else if (offset >= VIRTIO_INTERRUPT_ACK_BASE && offset < VIRTIO_INTERRUPT_ACK_BASE + 4) {
        if ((val & 1) == 1) {
            state->virtio->interrupt_status &= ~1;
            // printf("virtio disabled: pc: %llx, interrupt_status: %d\n", state->pc, state->virtio->interrupt_status);
        }
        state->virtio->interrupt_ack = WriteRange8(state->virtio->interrupt_ack, val, offset - VIRTIO_INTERRUPT_ACK_BASE);
        return;
    } else if (offset >= VIRTIO_STATUS_BASE && offset < VIRTIO_STATUS_BASE + 4) {
        state->virtio->status = WriteRange8(state->virtio->status, val, offset - VIRTIO_STATUS_BASE);
        return;
    }
}

uint8_t VirtioRead(State *state, uint64_t offset) {
    if (offset >= VIRTIO_MAGIC_VALUE_BASE && offset < VIRTIO_MAGIC_VALUE_BASE + 4) {
        return ReadRange8(0x74726976, offset - VIRTIO_MAGIC_VALUE_BASE);
    } else if (offset >= VIRTIO_DEVICE_VERSION_BASE && offset < VIRTIO_DEVICE_VERSION_BASE + 4) {
        return ReadRange8(0x01, offset - VIRTIO_DEVICE_VERSION_BASE);
    } else if (offset >= VIRTIO_DEVICE_ID_BASE && offset < VIRTIO_DEVICE_ID_BASE + 4) {
        return ReadRange8(0x02, offset - VIRTIO_DEVICE_ID_BASE);
    } else if (offset >= VIRTIO_VENDOR_ID_BASE && offset < VIRTIO_VENDOR_ID_BASE + 4) {
        return ReadRange8(0x554d4551, offset - VIRTIO_VENDOR_ID_BASE);
    } else if (offset >= VIRTIO_HOST_FEATURES_BASE && offset < VIRTIO_HOST_FEATURES_BASE + 4) {
        return ReadRange8(state->virtio->host_features, offset - VIRTIO_HOST_FEATURES_BASE);
    } else if (offset >= VIRTIO_QUEUE_NUM_MAX_BASE && offset < VIRTIO_QUEUE_NUM_MAX_BASE + 4) {
        return ReadRange8(0x2000, offset - VIRTIO_QUEUE_NUM_MAX_BASE);
    } else if (offset >= VIRTIO_QUEUE_PFN_BASE && offset < VIRTIO_QUEUE_PFN_BASE + 4) {
        return ReadRange8(state->virtio->queue_pfn, offset - VIRTIO_QUEUE_PFN_BASE);
    } else if (offset >= VIRTIO_INTERRUPT_STATUS_BASE && offset < VIRTIO_INTERRUPT_STATUS_BASE + 4) {
        return ReadRange8(state->virtio->interrupt_status, offset - VIRTIO_INTERRUPT_STATUS_BASE);
    } else if (offset >= VIRTIO_STATUS_BASE && offset < VIRTIO_STATUS_BASE + 4) {
        return ReadRange8(state->virtio->status, offset - VIRTIO_STATUS_BASE);
    } else {
        return 0;
    }
}

void MemWrite8(State *state, uint64_t addr, uint8_t val) {
    if (addr >= UART_BASE && addr < (UART_BASE + UART_SIZE)) {
        UartWrite(state, addr - UART_BASE, val);
        return;
    } else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
        ClintWrite(state, addr - CLINT_BASE, val);
        return;
    } else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
        PlicWrite(state, addr - PLIC_BASE, val);
        return;
    } else if (addr >= VIRTIO_BASE && addr < (VIRTIO_BASE + VIRTIO_SIZE)) {
        VirtioWrite(state, addr - VIRTIO_BASE, val);
        return;
    } else if (addr >= DRAM_BASE) {
        *(uint8_t *)(state->mem + (addr - DRAM_BASE)) = val;
        return;
    }
    state->excepted = true;
    state->exception_code = StoreAMOPageFault;
    return;
}

void MemWrite16(State *state, uint64_t addr, uint16_t val) {
    // if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return;
    // } else if (addr >= DRAM_BASE) {
    //     *(uint16_t *)(state->mem + (addr - DRAM_BASE)) = val;
    //     return;
    // }
    for (int i = 0; i < 2; i++) {
        MemWrite8(state, addr + i, (uint8_t)(val >> (i * 8) & SetNBits(8)));
    }
}

void MemWrite32(State *state, uint64_t addr, uint32_t val) {
    // if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return;
    // } else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
    //     return;
    // } else if (addr >= DRAM_BASE) {
    //     *(uint32_t *)(state->mem + (addr - DRAM_BASE)) = val;
    //     return;
    // }
    for (int i = 0; i < 4; i++) {
        MemWrite8(state, addr + i, (uint8_t)(val >> (i * 8) & SetNBits(8)));
    }
}

void MemWrite64(State *state, uint64_t addr, uint64_t val) {
    // if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return;
    // } else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
    //     return;
    // } else if (addr >= DRAM_BASE) {
    //     *(uint64_t *)(state->mem + (addr - DRAM_BASE)) = val;
    //     return;
    // }
    for (int i = 0; i < 8; i++) {
        MemWrite8(state, addr + i, (uint8_t)(val >> (i * 8) & SetNBits(8)));
    }
}

uint8_t MemRead8(State *state, uint64_t addr) {
    if (addr >= UART_BASE && addr < (UART_BASE + UART_SIZE)) {
        return UartRead(state, addr - UART_BASE);
    } else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
        return ClintRead(state, addr - CLINT_BASE);
    } else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
        return PlicRead(state, addr - PLIC_BASE);
    } else if (addr >= VIRTIO_BASE && addr < (VIRTIO_BASE + VIRTIO_SIZE)) {
        return VirtioRead(state, addr - VIRTIO_BASE);
    } else if (addr >= DRAM_BASE) {
        return *(uint8_t *)(state->mem + (addr - DRAM_BASE));
    }
    state->excepted = true;
    state->exception_code = LoadPageFault;
    return 0;
}

uint16_t MemRead16(State *state, uint64_t addr) {
    // if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return 0;
    // } else if (addr >= DRAM_BASE) {
    //     return *(uint16_t *)(state->mem + (addr - DRAM_BASE));
    // }
    uint16_t val = 0;
    for (int i = 0; i < 2; i++) {
        val |= (uint16_t)(MemRead8(state, addr + i)) << (i * 8);
    }
    return val;
}

uint32_t MemRead32(State *state, uint64_t addr) {
    // if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
    //     return 0;
    // } else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return 0;
    // } else if (addr >= DRAM_BASE) {
    //     return *(uint32_t *)(state->mem + (addr - DRAM_BASE));
    // }
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= (uint32_t)(MemRead8(state, addr + i)) << (i * 8);
    }
    return val;
}

uint64_t MemRead64(State *state, uint64_t addr) {
    // if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
    //     return 0;
    // } else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
    //     return 0;
    // } else if (addr >= DRAM_BASE) {
    //     return *(uint64_t *)(state->mem + (addr - DRAM_BASE));
    // }
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (uint64_t)(MemRead8(state, addr + i)) << (i * 8);
    }
    return val;
}

void AccessFault(State *state, uint8_t access_type) {
    state->excepted = true;
    switch (access_type) {
    case AccessInstruction:
        state->exception_code = InstructionAccessFault;
        break;
    case AccessLoad:
        state->exception_code = LoadAccessFault;
        break;
    case AccessStore:
        state->exception_code = StoreAMOAccessFault;
        break;
    default:
        Error("Unknown access type: %d", access_type);
        break;
    }
}

void PageFault(State *state, uint8_t access_type) {
    state->excepted = true;
    switch (access_type) {
    case AccessInstruction:
        state->exception_code = InstructionPageFault;
        break;
    case AccessLoad:
        state->exception_code = LoadPageFault;
        break;
    case AccessStore:
        state->exception_code = StoreAMOPageFault;
        break;
    default:
        Error("Unknown access type: %d", access_type);
        break;
    }
}

// Translate a virtual address to a physical address.
uint64_t Translate(State *state, uint64_t v_addr, uint8_t access_type) {
    uint8_t MODE = ReadCSR(state, SATP, 60, 63);

    if (MODE == Bare || state->mode == MACHINE) {
        return v_addr;
    }
    if (MODE != Sv39) {
        Error("Unimplemented Translation Mode: %d", MODE);
    }

    uint64_t a;
    int64_t i;
    bool access_fault;
    bool page_fault;
    uint64_t PPN;
    uint64_t va_vpn_i;
    uint64_t pte_addr;
    uint64_t pte;
    uint8_t pte_v;
    uint8_t pte_r;
    uint8_t pte_w;
    uint8_t pte_x;
    uint8_t pte_u;
    uint8_t pte_g;
    uint8_t pte_a;
    uint8_t pte_d;
    uint8_t pte_rsw;
    uint64_t pte_ppn;
    uint64_t pa;
step1:
    // printf("step1\n");
    page_fault = false;
    uint64_t va = GetRange(v_addr, VALEN, XLEN - 1);
    if (GetRange(v_addr, VALEN - 1, VALEN - 1) == 1 && va != SetNBits(XLEN - VALEN)) {
        page_fault = true;
    }
    if (GetRange(v_addr, VALEN - 1, VALEN - 1) == 0 && va != 0) {
        page_fault = true;
    }
    if (page_fault) {
        // printf("step1 page fault\n");
        PageFault(state, access_type);
        return v_addr;
    }

step2:
    // printf("step2\n");
    PPN = ReadCSR(state, SATP, 0, 43);
    a = PPN * PAGESIZE;
    i = LEVELS - 1;

step3:
    // printf("step3\n");
    va_vpn_i = ((v_addr >> 12) >> (i * 9)) & SetNBits(9);
    pte_addr = a + va_vpn_i * PTESIZE;
    pte = MemRead64(state, pte_addr);

step4:
    // printf("step4\n");
    pte_v = pte & SetNBits(1);
    pte_r = pte >> 1 & SetNBits(1);
    pte_w = pte >> 2 & SetNBits(1);
    pte_x = pte >> 3 & SetNBits(1);
    pte_u = pte >> 4 & SetNBits(1);
    pte_g = pte >> 5 & SetNBits(1);
    pte_a = pte >> 6 & SetNBits(1);
    pte_d = pte >> 7 & SetNBits(1);
    pte_ppn = pte >> 10;
    pte_rsw = pte >> 8 & SetNBits(2);
    if (pte_v == 0 || (pte_r == 0 && pte_w == 1)) {
        // printf("step4 page fault\n");
        PageFault(state, access_type);
        return v_addr;
    }

step5:
    // printf("step5\n");
    // If pte.r=1 or pte.x=1, go to step 6. 
    if (pte_r == 1 || pte_x == 1) {
        goto step6;
    } else {
        i--;
        if (i < 0) {
            // printf("step5 page fault\n");
            PageFault(state, access_type);
            return v_addr;
        } else {
            a =  pte_ppn * PAGESIZE;
            goto step3;
        }
    }

step6:
    // printf("step6\n");
    page_fault = false;
    // When SUM=0, S-mode memory accesses to pages that areaccessible by U-mode (U=1 in Figure 4.17) will fault. 
    // if (ReadCSR(state, MSTATUS, 18, 18) == 0 && pte_u == 1) {
    //     page_fault = true;
    // }
    if (access_type == AccessInstruction) {
        if (pte_x == 0) {
            printf("AccessInstruction Page Fault\n");
            page_fault = true;
        }
    }
    if (access_type == AccessLoad) {
        // if (ReadCSR(state, MSTATUS, 19, 19) == 0) {
        if (pte_r == 0) {
            printf("AccessLoad Page Fault\n");
            page_fault = true;
        }
        // } else if (ReadCSR(state, MSTATUS, 19, 19) == 1) {
        //     if (pte_r == 0 || pte_x == 0) {
        //         page_fault = true;
        //     }
        // }
    }
    if (access_type == AccessStore) {
        if (pte_w == 0) {
            printf("AccessStore Page Fault\n");
            page_fault = true;
        }
    }

    if (page_fault) {
        printf("step6 page fault\n");
        PageFault(state, access_type);
        return v_addr;
    }

step7:
    // printf("step7\n");
    if (i > 0) {
        if ((pte_ppn & SetNBits(9 * i)) != 0) {
            page_fault = true;
        }
    }
    if (page_fault) {
        printf("step7 page fault\n");
        PageFault(state, access_type);
        return v_addr;
    }

step8:
    // printf("step8\n");
    if (pte_a == 0 || (access_type == AccessStore && pte_d == 0)) {
        uint64_t new_pte = pte | SetOneBit(6) | (access_type == AccessStore ? SetOneBit(7) : 0);
        MemWrite64(state, pte_addr, new_pte);
    }

step9:
    // printf("step9\n");
    pa = 0;
    pa |= (v_addr & SetNBits(12));
    if (i > 0) {
        pa |= (v_addr >> 12 & SetNBits(9 * i)) << 12;
    }
    pa |= (pte_ppn & (SetNBits(44) ^ SetNBits(9 * i))) << 12;
    // printf("va: 0x%llx -> pa: 0x%llx\n", v_addr, pa);
    return pa;
}

void Write8(State *state, uint64_t v_addr, uint8_t val) {
    uint64_t p_addr = Translate(state, v_addr, AccessStore);
    if (state->excepted) return;
    MemWrite8(state, p_addr, val);
}

void Write16(State *state, uint64_t v_addr, uint16_t val)  {
    uint64_t p_addr = Translate(state, v_addr, AccessStore);
    if (state->excepted) return;
    MemWrite16(state, p_addr, val);
}

void Write32(State *state, uint64_t v_addr, uint32_t val)  {
    uint64_t p_addr = Translate(state, v_addr, AccessStore);
    if (state->excepted) return;
    MemWrite32(state, p_addr, val);
}

void Write64(State *state, uint64_t v_addr, uint64_t val)  {
    uint64_t p_addr = Translate(state, v_addr, AccessStore);
    if (state->excepted) return;
    MemWrite64(state, p_addr, val);
}

uint8_t Read8(State *state, uint64_t v_addr) {
    uint64_t p_addr = Translate(state, v_addr, AccessLoad);
    if (state->excepted) return 0;
    return MemRead8(state, p_addr);
}

uint16_t Read16(State *state, uint64_t v_addr) {
    uint64_t p_addr = Translate(state, v_addr, AccessLoad);
    if (state->excepted) return 0;
    return MemRead16(state, p_addr);
}

uint32_t Read32(State *state, uint64_t v_addr) {
    uint64_t p_addr = Translate(state, v_addr, AccessLoad);
    if (state->excepted) return 0;
    return MemRead32(state, p_addr);
}

uint64_t Read64(State *state, uint64_t v_addr) {
    uint64_t p_addr = Translate(state, v_addr, AccessLoad);
    if (state->excepted) return 0;
    return MemRead64(state, p_addr);
}

uint32_t Fetch32(State *state, uint64_t v_addr) {
    uint64_t p_addr = Translate(state, v_addr, AccessInstruction);
    if (state->excepted) return 0;
    // printf("v_addr: %llx -> p_addr: %llx\n", v_addr, p_addr);
    return MemRead32(state, p_addr);
}

// CSRs[csr][start_bit:end_bit] = val
void WriteCSR(State *state, uint16_t csr, uint8_t start_bit, uint8_t end_bit,
              uint64_t val) {
    assert(start_bit <= end_bit);

    uint64_t mask = SetNBits(end_bit - start_bit + 1);
    val &= mask;
    state->csr[csr] &= (~(mask << start_bit));
    state->csr[csr] |= (val << start_bit);
}

// CSRs[csr][start_bit:end_bit]
uint64_t ReadCSR(State *state, uint16_t csr, uint8_t start_bit,
                 uint8_t end_bit) {
    assert(start_bit <= end_bit);

    uint64_t mask = SetNBits(end_bit - start_bit + 1);
    return (state->csr[csr] & (mask << start_bit)) >> start_bit;
}

// Check whether current privilege mode is `mode`.
// If the privilege mode isn't `mode`, raise `IllegalInstruction` exception.
bool Require(State *state, uint8_t mode) {
    if (state->mode == MACHINE) {
        if (mode == MACHINE || mode == SUPERVISOR || mode == USER) {
            return true;
        }
    } else if (state->mode == SUPERVISOR) {
        if (mode == SUPERVISOR || mode == USER) {
            return true;
        }
    } else if (state->mode == USER) {
        if (mode == USER) {
            return true;
        }
    }

    state->excepted = true;
    // printf("Require Illegal Instruction: pc: 0x%llx, state->mode: %d, mode: %d\n", state->pc, state->mode, mode);
    state->exception_code = IllegalInstruction;
    return false;
}

// Sign-extend a value of `i` by supposing that its top bit is `top_bit`.
int64_t Sext(int64_t i, int top_bit) {
    if (i & SetOneBit(top_bit)) {
        i |= SetNBits(64 - top_bit - 1) << (top_bit + 1);
    }

    return i;
}

void ExecAddi(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    int32_t immediate = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] + immediate;
}

void ExecSlli(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t immediate = instr >> 20 & SetNBits(6);

    state->x[rd] = (uint64_t)state->x[rs1] << (uint64_t)immediate;
}

void ExecSlti(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    int32_t immediate = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] < immediate;
}

void ExecSltiu(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    int32_t immediate = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = ((uint64_t)state->x[rs1]) < ((uint64_t)immediate);
}

void ExecXori(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    int32_t immediate = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] ^ immediate;
}

void ExecSrli(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(6);

    state->x[rd] = (int64_t)((uint64_t)state->x[rs1] >> (uint32_t)immediate);
}

void ExecSrai(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(6);

    state->x[rd] = state->x[rs1] >> immediate;
}

void ExecOri(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint64_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] | immediate;
}

void ExecAndi(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] & immediate;
}

void ExecOpImmInstr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);
    uint8_t funct7 = (instr >> 25) & SetNBits(7);
    uint8_t funct6 = (instr >> 26) & SetNBits(6);

    switch (funct3) {
    case 0x0:
        ExecAddi(state, instr);
        break;
    case 0x1:
        ExecSlli(state, instr);
        break;
    case 0x2:
        ExecSlti(state, instr);
        break;
    case 0x3:
        ExecSltiu(state, instr);
        break;
    case 0x4:
        ExecXori(state, instr);
        break;
    case 0x5:
        if (funct6 == 0x00) {
            ExecSrli(state, instr);
        } else if (funct6 == 0x10) {
            ExecSrai(state, instr);
        }
        break;
    case 0x6:
        ExecOri(state, instr);
        break;
    case 0x7:
        ExecAndi(state, instr);
        break;
    default:
        Error("Unknown OP_IMM instruction: %2x", funct3);
    }
}

void ExecAuipc(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    int64_t immediate = Sext((instr >> 12) & SetNBits(20), 19);

    state->x[rd] = state->pc + (immediate << 12);
}

void ExecLui(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint64_t immediate = (instr >> 12) & SetNBits(20);

    state->x[rd] = Sext(immediate << 12, 31);
}

void ExecAdd(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] + state->x[rs2];
}

void ExecMul(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] * state->x[rs2];
}

void ExecSub(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] - state->x[rs2];
}

void ExecSll(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] << state->x[rs2];
}

void ExecMulh(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        ((__int128_t)state->x[rs1] * (__int128_t)state->x[rs2]) >> XLEN;
}

void ExecSlt(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] < state->x[rs2];
}

void ExecMulhsu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        ((__int128_t)state->x[rs1] * (__uint128_t)(uint64_t)state->x[rs2]) >> XLEN;
}

void ExecSltu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = ((uint64_t)state->x[rs1]) < ((uint64_t)state->x[rs2]);
}

void ExecMulhu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        ((__uint128_t)(uint64_t)state->x[rs1] * (__uint128_t)(uint64_t)state->x[rs2]) >> XLEN;
}

void ExecXor(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] ^ state->x[rs2];
}

void ExecDiv(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = -1;
    } else if (state->x[rs1] == -((int64_t)1 << 63) && state->x[rs2] == -1) {
        state->x[rd] = -((int64_t)1 << 63);
    } else {
        state->x[rd] = state->x[rs1] / state->x[rs2];
    }
}

void ExecSrl(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    // In RV64I, Use SetNBits(6).
    state->x[rd] =
        ((uint64_t)state->x[rs1]) >> ((uint64_t)(state->x[rs2] & SetNBits(5)));
}

void ExecDivu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = ~((uint64_t)0);
    } else {
        state->x[rd] = (uint64_t)state->x[rs1] / (uint64_t)state->x[rs2];
    }
}

void ExecSra(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    // In RV64I, Use SetNBits(6).
    state->x[rd] = state->x[rs1] >> (state->x[rs2] & SetNBits(5));
}

void ExecOr(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] | state->x[rs2];
}

void ExecRem(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = state->x[rs1];
    } else if (state->x[rs1] == -((int64_t)1 << 63) && state->x[rs2] == -1) {
        state->x[rd] = 0;
    } else {
        state->x[rd] = state->x[rs1] % state->x[rs2];
    }
}

void ExecAnd(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] & state->x[rs2];
}

void ExecRemu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = state->x[rs1];
    } else {
        state->x[rd] = (uint64_t)state->x[rs1] % (uint64_t)state->x[rs2];
    }
}

void ExecOpInstr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);
    uint8_t funct7 = (instr >> 25) & SetNBits(7);

    switch (funct3) {
    case 0x0:
        if (funct7 == 0x00) {
            ExecAdd(state, instr);
        } else if (funct7 == 0x01) {
            ExecMul(state, instr);
        } else if (funct7 == 0x20) {
            ExecSub(state, instr);
        }
        break;
    case 0x1:
        if (funct7 == 0x00) {
            ExecSll(state, instr);
        } else if (funct7 == 0x01) {
            ExecMulh(state, instr);
        }
        break;
    case 0x2:
        if (funct7 == 0x00) {
            ExecSlt(state, instr);
        } else if (funct7 == 0x01) {
            ExecMulhsu(state, instr);
        }
        break;
    case 0x3:
        if (funct7 == 0x00) {
            ExecSltu(state, instr);
        } else if (funct7 == 0x01) {
            ExecMulhu(state, instr);
        }
        break;
    case 0x4:
        if (funct7 == 0x00) {
            ExecXor(state, instr);
        } else if (funct7 == 0x01) {
            ExecDiv(state, instr);
        }
        break;
    case 0x5:
        if (funct7 == 0x00) {
            ExecSrl(state, instr);
        } else if (funct7 == 0x01) {
            ExecDivu(state, instr);
        } else if (funct7 == 0x20) {
            ExecSra(state, instr);
        }
        break;
    case 0x6:
        if (funct7 == 0x00) {
            ExecOr(state, instr);
        } else if (funct7 == 0x01) {
            ExecRem(state, instr);
        }
        break;
    case 0x7:
        if (funct7 == 0x00) {
            ExecAnd(state, instr);
        } else if (funct7 == 0x01) {
            ExecRemu(state, instr);
        }
        break;
    }
}

void ExecJal(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    int32_t offset = Sext((instr >> 31 & SetNBits(1)) << 20 |
                              (instr >> 12 & SetNBits(8)) << 12 |
                              (instr >> 20 & SetNBits(1)) << 11 |
                              (instr >> 21 & SetNBits(10)) << 1,
                          20);

    state->x[rd] = state->pc + sizeof(instr);
    state->pc += offset;
}

void ExecJalr(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    uint64_t t = state->pc + sizeof(instr);
    state->pc = (state->x[rs1] + offset) & ~1;
    state->x[rd] = t;
}

void ExecLb(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(Read8(state, state->x[rs1] + offset), 7);
}

void ExecLh(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(Read16(state, state->x[rs1] + offset), 15);
}

void ExecLw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(Read32(state, state->x[rs1] + offset), 31);
}

void ExecLd(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Read64(state, state->x[rs1] + offset);
}

void ExecLbu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Read8(state, state->x[rs1] + offset);
}

void ExecLhu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Read16(state, state->x[rs1] + offset);
}

void ExecLwu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Read32(state, state->x[rs1] + offset);
}

void ExecLoadInstr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);

    switch (funct3) {
    case 0x0:
        ExecLb(state, instr);
        break;
    case 0x1:
        ExecLh(state, instr);
        break;
    case 0x2:
        ExecLw(state, instr);
        break;
    case 0x3:
        ExecLd(state, instr);
        break;
    case 0x4:
        ExecLbu(state, instr);
        break;
    case 0x5:
        ExecLhu(state, instr);
        break;
    case 0x6:
        ExecLwu(state, instr);
        break;
    default:
        Error("Unimplemented Load Instruction: funct3=0x%x", funct3);
    }
}

void ExecSb(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    // printf("sb: pc: %llx, addr: %llx, val: %d\n", state->pc, state->x[rs1] + offset, (uint8_t)state->x[rs2]);
    Write8(state, state->x[rs1] + offset,
           (uint8_t)state->x[rs2]);
}

void ExecSh(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    Write16(state, state->x[rs1] + offset,
            (uint16_t)state->x[rs2]);
}

void ExecSw(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    Write32(state, state->x[rs1] + offset, (uint32_t)state->x[rs2]);
}

void ExecSd(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    Write64(state, state->x[rs1] + offset, (uint64_t)state->x[rs2]);
}

void ExecStoreInstr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);

    switch (funct3) {
    case 0x0:
        ExecSb(state, instr);
        break;
    case 0x1:
        ExecSh(state, instr);
        break;
    case 0x2:
        ExecSw(state, instr);
        break;
    case 0x3:
        ExecSd(state, instr);
        break;
    default:
        Error("Unimplemented Store Instruction: funct3=0x%x", funct3);
    }
}

void ExecBeq(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if (state->x[rs1] == state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBne(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if (state->x[rs1] != state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBlt(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if (state->x[rs1] < state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBge(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if (state->x[rs1] >= state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBltu(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if ((uint64_t)state->x[rs1] < (uint64_t)state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBgeu(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(((instr >> 31 & SetNBits(1)) << 12) |
                              ((instr >> 7 & SetNBits(1)) << 11) |
                              ((instr >> 25 & SetNBits(6)) << 5) |
                              ((instr >> 8 & SetNBits(4)) << 1),
                          12);

    if ((uint64_t)state->x[rs1] >= (uint64_t)state->x[rs2])
        state->pc += offset;
    else
        state->pc += sizeof(instr);
}

void ExecBranchInstr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);

    switch (funct3) {
    case 0x0:
        ExecBeq(state, instr);
        break;
    case 0x1:
        ExecBne(state, instr);
        break;
    case 0x4:
        ExecBlt(state, instr);
        break;
    case 0x5:
        ExecBge(state, instr);
        break;
    case 0x6:
        ExecBltu(state, instr);
        break;
    case 0x7:
        ExecBgeu(state, instr);
        break;
    default:
        Error("Unimplemented Branch Instruction: funct3=0x%x", funct3);
    }
}

void ExecAddiw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = Sext((state->x[rs1] + immediate) & SetNBits(32), 31);
}

void ExecSlliw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    // state->x[rd] = Sext((state->x[rs1] << (uint32_t)immediate) & SetNBits(32), 31);
    state->x[rd] = (int64_t)(int32_t)(state->x[rs1] << (uint32_t)immediate);
}

void ExecSrliw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        Sext((uint64_t)(state->x[rs1] & SetNBits(32)) >> immediate, 31);
}

void ExecSraiw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((int32_t)(state->x[rs1] & SetNBits(32)) >> (uint32_t)immediate, 31);
}

void ExecOpImm32Instr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);
    uint8_t funct7 = (instr >> 25) & SetNBits(7);

    switch (funct3) {
    case 0x0:
        ExecAddiw(state, instr);
        break;
    case 0x1:
        ExecSlliw(state, instr);
        break;
    case 0x5:
        if (funct7 == 0x00) {
            ExecSrliw(state, instr);
        } else if (funct7 == 0x20) {
            ExecSraiw(state, instr);
        }
        break;
    }
}

void ExecAddw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((state->x[rs1] + state->x[rs2]) & SetNBits(32), 31);
}

void ExecMulw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((state->x[rs1] * state->x[rs2]) & SetNBits(32), 31);
}

void ExecSubw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((state->x[rs1] - state->x[rs2]) & SetNBits(32), 31);
}

void ExecSllw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        Sext((state->x[rs1] << (state->x[rs2] & SetNBits(5))) & SetNBits(32), 31);
}

void ExecDivw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = -1;
    } else if ((int32_t)(state->x[rs1] & SetNBits(32)) == -((int32_t)1 << 31) && (int32_t)(state->x[rs2] & SetNBits(32)) == -1) {
        state->x[rd] = -((int32_t)1 << 31);
    } else {
        state->x[rd] = Sext(((int32_t)(state->x[rs1] & SetNBits(32)) / (int32_t)(state->x[rs2] & SetNBits(32))), 31);
    }
}

void ExecSrlw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((uint32_t)state->x[rs1] >> state->x[rs2], 31);
}

void ExecDivuw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = ~((uint64_t)0);
    } else {
        state->x[rd] = Sext((state->x[rs1] & SetNBits(32)) /
                                (state->x[rs2] & SetNBits(32)),
                            31);
    }
}

void ExecSraw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        Sext((int32_t)(state->x[rs1] & SetNBits(32)) >> (uint32_t)(state->x[rs2] & SetNBits(5)), 31);
}

void ExecRemw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = state->x[rs1];
    } else {
        state->x[rd] = Sext((state->x[rs1] % state->x[rs2]) & SetNBits(32), 31);
    }
}

void ExecRemuw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    if (state->x[rs2] == 0) {
        state->x[rd] = state->x[rs1];
    } else {
        state->x[rd] = Sext((state->x[rs1] & SetNBits(32)) %
                                (state->x[rs2] & SetNBits(32)),
                            31);
    }
}

void ExecOp32Instr(State *state, uint32_t instr) {
    uint8_t funct3 = (instr >> 12) & SetNBits(3);
    uint8_t funct7 = (instr >> 25) & SetNBits(7);

    switch (funct3) {
    case 0x0:
        if (funct7 == 0x00) {
            ExecAddw(state, instr);
        } else if (funct7 == 0x01) {
            ExecMulw(state, instr);
        } else {
            ExecSubw(state, instr);
        }
        break;
    case 0x1:
        ExecSllw(state, instr);
        break;
    case 0x4:
        if (funct7 == 0x01) {
            ExecDivw(state, instr);
        }
        break;
    case 0x5:
        if (funct7 == 0x00) {
            ExecSrlw(state, instr);
        } else if (funct7 == 0x01) {
            ExecDivuw(state, instr);
        } else if (funct7 == 0x20) {
            ExecSraw(state, instr);
        }
        break;
    case 0x6:
        if (funct7 == 0x01) {
            ExecRemw(state, instr);
        }
        break;
    case 0x7:
        if (funct7 == 0x01) {
            ExecRemuw(state, instr);
        }
    }
}

void ExecCAddi4spn(State *state, uint16_t instr) {
    uint32_t uimm =
        ((instr >> 7 & SetNBits(4)) << 6) | ((instr >> 11 & SetNBits(2)) << 4) |
        ((instr >> 5 & SetNBits(1)) << 3) | ((instr >> 6 & SetNBits(1)) << 2);
    uint8_t rd = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[2] + uimm;
}

void ExecCLw(State *state, uint16_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(1)) << 6) |
                   ((instr >> 6 & SetNBits(1)) << 2);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rd = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = Sext(Read32(state, state->x[8 + rs1] + uimm) & SetNBits(32), 31);
}

void ExecCLd(State *state, uint32_t instr) {
    uint8_t uimm =
        ((instr >> 5 & SetNBits(2)) << 6) | ((instr >> 10 & SetNBits(3)) << 3);
    uint8_t rs1 = instr >> 7 & SetNBits(3);
    uint8_t rd = instr >> 2 & SetNBits(3);

    state->x[8 + rd] = Read64(state, state->x[8 + rs1] + uimm);
}

void ExecCSw(State *state, uint16_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(1)) << 6) |
                   ((instr >> 6 & SetNBits(1)) << 2);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    Write32(state, state->x[8 + rs1] + uimm, state->x[8 + rs2]);
}

void ExecCSd(State *state, uint32_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(2)) << 6);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    Write64(state, state->x[8 + rs1] + uimm, state->x[8 + rs2]);
}

void ExecCAddi(State *state, uint16_t instr) {
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 5) | (instr >> 2 & SetNBits(5)), 5);

    if (imm == 0) {
        Error("Invalid c.addi immediate");
    }
    uint8_t rd = instr >> 7 & SetNBits(5);

    state->x[rd] = state->x[rd] + imm;
}

void ExecCAddiw(State *state, uint16_t instr) {
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 5) | ((instr >> 2) & SetNBits(5)), 5);
    uint8_t rd = (instr >> 7) & SetNBits(5);

    state->x[rd] = Sext((state->x[rd] + imm) & SetNBits(32), 31);
}

void ExecCLi(State *state, uint16_t instr) {
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 5) | (instr >> 2 & SetNBits(5)), 5);
    uint8_t rd = (instr >> 7) & SetNBits(5);

    state->x[rd] = imm;
}

void ExecCAddi16sp(State *state, uint16_t instr) {
    int32_t imm_t = (instr >> 2) & SetNBits(5);
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 9) | ((imm_t >> 1 & SetNBits(2)) << 7) |
            ((imm_t >> 3 & SetNBits(1)) << 6) | ((imm_t & SetNBits(1)) << 5) |
            ((imm_t >> 4 & SetNBits(1)) << 4),
        9);

    state->x[2] = state->x[2] + imm;
}

void ExecCLui(State *state, uint16_t instr) {
    int32_t imm = Sext(((instr >> 12 & SetNBits(1)) << 17) |
                           ((instr >> 2 & SetNBits(5)) << 12),
                       17);
    uint8_t rd = (instr >> 7) & SetNBits(5);

    if (imm == 0)
        Error("Invalid c.lui operand");

    state->x[rd] = imm;
}

void ExecCSrli(State *state, uint16_t instr) {
    uint32_t uimm =
        ((instr >> 12 & SetNBits(1)) << 5) | (instr >> 2 & SetNBits(5));
    uint8_t rd = (instr >> 7) & SetNBits(3);

    state->x[8 + rd] = (uint64_t)state->x[8 + rd] >> uimm;
}

void ExecCSrai(State *state, uint16_t instr) {
    uint32_t uimm =
        ((instr >> 12 & SetNBits(1)) << 5) | ((instr >> 2) & SetNBits(5));
    uint8_t rd = (instr >> 7) & SetNBits(3);

    state->x[8 + rd] = (int32_t)state->x[8 + rd] >> uimm;
}

void ExecCAndi(State *state, uint16_t instr) {
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 5) | ((instr >> 2) & SetNBits(5)), 5);
    uint8_t rd = (instr >> 7) & SetNBits(3);

    state->x[8 + rd] = state->x[8 + rd] & imm;
}

void ExecCJ(State *state, uint16_t instr) {
    uint32_t imm_t = (instr >> 2);
    // [11|4|9:8|10|6|7|3:1|5] = [10|9|8:7|6|5|4|3:1|0]
    int32_t imm = Sext(((imm_t >> 10 & SetNBits(1)) << 11) |
                           ((imm_t >> 6 & SetNBits(1)) << 10) |
                           ((imm_t >> 7 & SetNBits(2)) << 8) |
                           ((imm_t >> 4 & SetNBits(1)) << 7) |
                           ((imm_t >> 5 & SetNBits(1)) << 6) |
                           ((imm_t >> 0 & SetNBits(1)) << 5) |
                           ((imm_t >> 9 & SetNBits(1)) << 4) |
                           ((imm_t >> 1 & SetNBits(3)) << 1),
                       11);

    state->pc += imm;
}

void ExecCBeqz(State *state, uint16_t instr) {
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    int32_t imm = Sext(((instr >> 12 & SetNBits(1)) << 8) |
                           ((instr >> 5 & SetNBits(2)) << 6) |
                           ((instr >> 2 & SetNBits(1)) << 5) |
                           ((instr >> 10 & SetNBits(2)) << 3) |
                           ((instr >> 3 & SetNBits(2)) << 1),
                       8);

    if (state->x[8 + rs1] == 0)
        state->pc += imm;
    else
        state->pc += sizeof(instr);
}

void ExecCBnez(State *state, uint16_t instr) {
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    int32_t imm = Sext(((instr >> 12 & SetNBits(1)) << 8) |
                           ((instr >> 5 & SetNBits(2)) << 6) |
                           ((instr >> 2 & SetNBits(1)) << 5) |
                           ((instr >> 10 & SetNBits(2)) << 3) |
                           ((instr >> 3 & SetNBits(2)) << 1),
                       8);

    if (state->x[8 + rs1] != 0)
        state->pc += imm;
    else
        state->pc += sizeof(instr);
}

void ExecCSub(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[8 + rd] - state->x[8 + rs2];
}

void ExecCXor(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[8 + rd] ^ state->x[8 + rs2];
}

void ExecCOr(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[8 + rd] | state->x[8 + rs2];
}

void ExecCAnd(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[8 + rd] & state->x[8 + rs2];
}

void ExecCSubw(State *state, uint16_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(3);
    uint8_t rs2 = instr >> 2 & SetNBits(3);

    state->x[8 + rd] =
        Sext((state->x[8 + rd] - state->x[8 + rs2]) & SetNBits(32), 31);
}

void ExecCAddw(State *state, uint16_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(3);
    uint8_t rs2 = instr >> 2 & SetNBits(3);

    state->x[8 + rd] =
        Sext((state->x[8 + rd] + state->x[8 + rs2]) & SetNBits(32), 31);
}

void ExecCSlli(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint32_t uimm =
        ((instr >> 12 & SetNBits(1)) << 5) | ((instr >> 2 & SetNBits(5)));

    state->x[rd] = (uint64_t)state->x[rd] << uimm;
}

void ExecCLwsp(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint32_t uimm = ((instr >> 2 & SetNBits(2)) << 6) |
                    ((instr >> 12 & SetNBits(1)) << 5) |
                    ((instr >> 4 & SetNBits(3)) << 2);

    state->x[rd] = Sext(Read32(state, state->x[2] + uimm) & SetNBits(32), 31);
}

void ExecCLdsp(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint32_t uimm = (instr >> 2 & SetNBits(3)) << 6 |
                    (instr >> 12 & SetNBits(1)) << 5 |
                    (instr >> 5 & SetNBits(2)) << 3;

    state->x[rd] = Read64(state, state->x[2] + uimm);
}

void ExecCJr(State *state, uint16_t instr) {
    uint8_t rs1 = (instr >> 7) & SetNBits(5);

    state->pc = state->x[rs1];
}

void ExecCMv(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs2 = (instr >> 2) & SetNBits(5);

    state->x[rd] = state->x[rs2];
}

void ExecCJalr(State *state, uint16_t instr) {
    uint8_t rs1 = (instr >> 7) & SetNBits(5);

    int64_t t = state->pc + 2;
    state->pc = state->x[rs1];
    state->x[1] = t;
}

void ExecCAdd(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs2 = (instr >> 2) & SetNBits(5);

    state->x[rd] = state->x[rd] + state->x[rs2];
}

void ExecCSwsp(State *state, uint16_t instr) {
    uint8_t rs2 = (instr >> 2) & SetNBits(5);
    uint32_t uimm =
        ((instr >> 9 & SetNBits(4)) << 2) | ((instr >> 7 & SetNBits(2)) << 6);

    Write32(state, state->x[2] + uimm, state->x[rs2]);
}

void ExecCSdsp(State *state, uint32_t instr) {
    uint8_t rs2 = instr >> 2 & SetNBits(5);
    uint32_t uimm =
        (instr >> 7 & SetNBits(3)) << 6 | (instr >> 10 & SetNBits(3)) << 3;

    Write64(state, state->x[2] + uimm, state->x[rs2]);
}

void ExecCompressedInstr(State *state, uint16_t instr) {
    if (instr == 0) {
        state->excepted = true;
        printf("Compressed Instr Illegal\n");
        state->exception_code = IllegalInstruction;
        return;
    }
    uint8_t opcode = instr & SetNBits(2);

    switch (opcode) {
    case 0x0: {
        uint8_t rd = (instr >> 2) & SetNBits(2);
        int8_t funct3 = (instr >> 13) & SetNBits(3);
        uint8_t f_5_12 = (instr >> 5) & SetNBits(8);

        if (funct3 == 0x0 && f_5_12 != 0) {
            ExecCAddi4spn(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x1) {
            // TODO: Implement c.fld.
            state->pc += 2;
        } else if (funct3 == 0x2) {
            ExecCLw(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLd(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x5) {
            // TODO: Implement c.fsd.
            state->pc += 2;
        } else if (funct3 == 0x6) {
            ExecCSw(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x7) {
            ExecCSd(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else {
            Error("Invalid instruction: funct3=%d", funct3);
        }
    } break;
    case 0x1: {
        uint8_t r1 = (instr >> 7) & SetNBits(3);
        int32_t immediate = Sext(((instr >> 2) & SetNBits(5)) |
                                     ((instr >> 12 & SetNBits(1)) << 5),
                                 5);
        uint8_t funct3 = (instr >> 13) & SetNBits(3);
        uint8_t funct2_1 = (instr >> 10) & SetNBits(2);
        uint8_t funct2_2 = (instr >> 5) & SetNBits(2);
        uint8_t funct6 = (instr >> 10) & SetNBits(6);
        uint8_t rd = instr >> 7 & SetNBits(5);

        if (funct6 == 0x23 && funct2_2 == 0x0) {
            ExecCSub(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x1) {
            ExecCXor(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x2) {
            ExecCOr(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x3) {
            ExecCAnd(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct6 == 0x27 && funct2_2 == 0x0) {
            ExecCSubw(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct6 == 0x27 && funct2_2 == 0x1) {
            ExecCAddw(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x0 && rd == 0) {
            // C.NOP
            state->pc += 2;
        } else if (funct3 == 0x0 && rd != 0x0) {
            ExecCAddi(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x1 && rd != 0x0) {
            ExecCAddiw(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x2) {
            ExecCLi(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x3 && rd == 2) {
            ExecCAddi16sp(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLui(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x0) {
            ExecCSrli(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x1) {
            ExecCSrai(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x2) {
            ExecCAndi(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x5) {
            ExecCJ(state, instr);
            if (state->excepted) return;
        } else if (funct3 == 0x6) {
            ExecCBeqz(state, instr);
            if (state->excepted) return;
        } else if (funct3 == 0x7) {
            ExecCBnez(state, instr);
            if (state->excepted) return;
        } else {
            Error("Invalid instruction");
        }
    } break;
    case 0x2: {
        uint8_t funct3 = (instr >> 13) & SetNBits(3);
        uint8_t f_7_11 = (instr >> 7) & SetNBits(5);
        uint8_t f_2_6 = (instr >> 2) & SetNBits(5);
        uint8_t f_12 = (instr >> 12) & SetNBits(1);

        if (funct3 == 0x0 && f_12 == 0 && f_2_6 == 0 && f_7_11 != 0) {
            printf("c.slli64\n");
            state->pc += 2;
            // TODO: Understand c.slli64.
        } else if (funct3 == 0x0 && f_7_11 != 0) {
            ExecCSlli(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x1) {
            // TODO: Implement c.fldsp.
        } else if (funct3 == 0x2 && f_7_11 != 0) {
            ExecCLwsp(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLdsp(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 0 && f_7_11 != 0 && f_2_6 == 0) {
            ExecCJr(state, instr);
            if (state->excepted) return;
        } else if (funct3 == 0x4 && f_12 == 0 && f_7_11 != 0 && f_2_6 != 0) {
            ExecCMv(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 == 0 && f_2_6 == 0) {
            // ExecCEbreak(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 != 0 && f_2_6 == 0) {
            ExecCJalr(state, instr);
            if (state->excepted) return;
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 != 0 && f_2_6 != 0) {
            ExecCAdd(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x5) {
            // TODO: Implement c.fsdsp.
            state->pc += 2;
        } else if (funct3 == 0x6) {
            ExecCSwsp(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else if (funct3 == 0x7) {
            ExecCSdsp(state, instr);
            if (state->excepted) return;
            state->pc += 2;
        } else {
            Error("Invalid instruction");
        }
    } break;
    }
}

void ExecEcall(State *state, uint32_t instr) {
    // printf("ecall: Exception: pc: 0x%llx\n", state->pc);
    // printf("ecall: a0: %llx, a1: %llx, a7: %llx\n", state->x[10], state->x[11], state->x[17]);
    state->excepted = true;
    int exception_code;
    if (state->mode == 0x3) {
        exception_code = EnvironmentCallFromMMode;
    } else if (state->mode == 0x1) {
        exception_code = EnvironmentCallFromSMode;
    } else if (state->mode == 0x0) {
        exception_code = EnvironmentCallFromUMode;
    } else {
        state->excepted = false;
        return;
    }

    state->exception_code = exception_code;
}

void ExecEbreak(State *state, uint32_t instr) {
    // TODO: Implement break point exception.
    printf("Break\n");
}

void ExecSfencevma(State *state, uint32_t instr) {
    // Do nothing currently.
}

void ExecWfi(State *state, uint32_t instr) {
    printf("wfi\n");
    PrintRegisters(state, false);
    exit(state->x[10]);
    while (true);
}

void ExecMret(State *state, uint32_t instr) {
    Require(state, MACHINE);
    state->pc = state->csr[MEPC];
    state->mode = ReadCSR(state, MSTATUS, 11, 12);
    uint64_t mpie = ReadCSR(state, MSTATUS, 7, 7);
    WriteCSR(state, MSTATUS, 3, 3, mpie);
    WriteCSR(state, MSTATUS, 7, 7, 1);
    WriteCSR(state, MSTATUS, 11, 12, 0);
}

void ExecSret(State *state, uint32_t instr) {
    // printf("sret: mode: %d, pc: 0x%llx, sepc: 0x%llx, sstatus: %llx\n", state->mode, state->pc, state->csr[SEPC], state->csr[SSTATUS]);
    if (!Require(state, SUPERVISOR)) {
        return;
    }
    state->pc = state->csr[SEPC];
    state->mode = ReadCSR(state, SSTATUS, 8, 8);
    uint64_t spie = ReadCSR(state, SSTATUS, 5, 5);
    WriteCSR(state, SSTATUS, 1, 1, spie);
    WriteCSR(state, SSTATUS, 5, 5, 1);
    WriteCSR(state, SSTATUS, 8, 8, 0);
}

void ExecUret(State *state, uint32_t instr) { }

void ExecCsrrw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    int64_t t = state->csr[csr];
    state->csr[csr] = state->x[rs1];
    state->x[rd] = t;
}

void ExecCsrrs(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    int64_t t = state->csr[csr];
    state->csr[csr] = t | state->x[rs1];
    state->x[rd] = t;
}

void ExecCsrrc(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    int64_t t = state->csr[csr];
    state->csr[csr] = t & ~state->x[rs1];
    state->x[rd] = t;
}

void ExecCsrrwi(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t zimm = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    state->x[rd] = state->csr[csr];
    state->csr[csr] = zimm;
}

void ExecCsrrsi(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t zimm = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    int64_t t = state->csr[csr];
    state->csr[csr] = t | zimm;
    state->x[rd] = t;
}

void ExecCsrrci(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t zimm = instr >> 15 & SetNBits(5);
    uint32_t csr = instr >> 20 & SetNBits(12);

    int64_t t = state->csr[csr];
    state->csr[csr] = t & ~zimm;
    state->x[rd] = t;
}

void ExecSystemInstr(State *state, uint32_t instr) {
    uint8_t funct3 = instr >> 12 & SetNBits(3);
    bool is_pc_written = false;

    switch (funct3) {
    case 0x0: {
        uint32_t funct12 = instr >> 20 & SetNBits(12);
        if (funct12 == 0x00) {
            ExecEcall(state, instr);
        } else if (funct12 == 0x01) {
            ExecEbreak(state, instr);
        } else {
            uint8_t funct5 = instr >> 20 & SetNBits(5);
            uint8_t funct7 = instr >> 25 & SetNBits(7);
            if (funct7 == 0x09) {
                ExecSfencevma(state, instr);
            } else if (funct7 == 0x08 && funct5 == 0x5) {
                ExecWfi(state, instr);
            } else if (funct7 == 0x18 && funct5 == 0x2) {
                ExecMret(state, instr);
                is_pc_written = true;
            } else if (funct7 == 0x08) {
                ExecSret(state, instr);
                is_pc_written = true;
            } else if (funct7 == 0x00 && funct5 == 0x2) {
                ExecUret(state, instr);
                is_pc_written = true;
            } else {
                Error("Unknown instruction: System");
            }
        }
    } break;
    case 0x1:
        ExecCsrrw(state, instr);
        break;
    case 0x2:
        ExecCsrrs(state, instr);
        break;
    case 0x3:
        ExecCsrrc(state, instr);
        break;
    case 0x5:
        ExecCsrrwi(state, instr);
        break;
    case 0x6:
        ExecCsrrsi(state, instr);
        break;
    case 0x7:
        ExecCsrrci(state, instr);
        break;
    default:
        Error("Invalid instruction");
    }

    if (!is_pc_written)
        state->pc += sizeof(instr);
}

void ExecFence(State *state, uint32_t instr) {}

void ExecFencei(State *state, uint32_t instr) {}

void ExecMiscMem(State *state, uint32_t instr) {
    uint8_t funct3 = instr >> 12 & SetNBits(3);
    if (funct3 == 0x0) {
        ExecFence(state, instr);
    } else if (funct3 == 0x1) {
        ExecFencei(state, instr);
    } else {
        Error("Invalid instruction: MISC-MEM.");
    }
}

void ExecAmoaddw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    Write32(state, state->x[rs1], (uint32_t)(t + state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoswapw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    state->x[rd] = t;
}

void ExecLrw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    state->x[rd] = Sext(Read32(state, state->x[rs1]), 31);
}

void ExecScw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    state->x[rd] = 0;
}

void ExecAmoxorw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    Write32(state, state->x[rs1], (uint32_t)(t ^ state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoorw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    Write32(state, state->x[rs1], (uint32_t)(t | state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoandw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    Write32(state, state->x[rs1], (uint32_t)(t & state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmominw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    if (t < state->x[rs2]) {
        Write32(state, state->x[rs1], (uint32_t)t);
    } else {
        Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmomaxw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    if (t > state->x[rs2]) {
        Write32(state, state->x[rs1], (uint32_t)t);
    } else {
        Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmominuw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    if ((uint64_t)t < (uint64_t)(state->x[rs2])) {
        Write32(state, state->x[rs1], (uint32_t)t);
    } else {
        Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmomaxuw(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Sext(Read32(state, state->x[rs1]), 31);
    if ((uint64_t)t > (uint64_t)(state->x[rs2])) {
        Write32(state, state->x[rs1], (uint32_t)t);
    } else {
        Write32(state, state->x[rs1], (uint32_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmoaddd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(t + state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoswapd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    state->x[rd] = t;
}

void ExecLrd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    state->x[rd] = t;
}

void ExecScd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    state->x[rd] = 0;
}

void ExecAmoxord(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(t ^ state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoord(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(t | state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmoandd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    Write64(state, state->x[rs1], (uint64_t)(t & state->x[rs2]));
    state->x[rd] = t;
}

void ExecAmomind(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    if (t < state->x[rs2]) {
        Write64(state, state->x[rs1], (uint64_t)t);
    } else {
        Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmomaxd(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    if (t > state->x[rs2]) {
        Write64(state, state->x[rs1], (uint64_t)t);
    } else {
        Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmominud(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    if ((uint64_t)t < (uint64_t)(state->x[rs2])) {
        Write64(state, state->x[rs1], (uint64_t)t);
    } else {
        Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmomaxud(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint8_t rs1 = instr >> 15 & SetNBits(5);
    uint8_t rs2 = instr >> 20 & SetNBits(5);
    uint8_t rl = instr >> 25 & SetNBits(1);
    uint8_t aq = instr >> 26 & SetNBits(1);

    int64_t t = Read64(state, state->x[rs1]);
    if ((uint64_t)t > (uint64_t)(state->x[rs2])) {
        Write64(state, state->x[rs1], (uint64_t)t);
    } else {
        Write64(state, state->x[rs1], (uint64_t)(state->x[rs2]));
    }
    state->x[rd] = t;
}

void ExecAmo(State *state, uint32_t instr) {
    uint8_t funct5 = instr >> 27 & SetNBits(5);
    uint8_t funct3 = instr >> 12 & SetNBits(3);

    if (funct3 == 0x2) {
        switch (funct5) {
        case 0x00:
            ExecAmoaddw(state, instr);
            break;
        case 0x01:
            ExecAmoswapw(state, instr);
            break;
        case 0x02:
            ExecLrw(state, instr);
            break;
        case 0x03:
            ExecScw(state, instr);
            break;
        case 0x04:
            ExecAmoxorw(state, instr);
            break;
        case 0x08:
            ExecAmoorw(state, instr);
            break;
        case 0x0c:
            ExecAmoandw(state, instr);
            break;
        case 0x10:
            ExecAmominw(state, instr);
            break;
        case 0x14:
            ExecAmomaxw(state, instr);
            break;
        case 0x18:
            ExecAmominuw(state, instr);
            break;
        case 0x1c:
            ExecAmomaxuw(state, instr);
            break;
        default:
            Error("Unknown amo instruction: %.2x", funct5);
            break;
        }
    } else if (funct3 == 0x3) {
        switch (funct5) {
        case 0x00:
            ExecAmoaddd(state, instr);
            break;
        case 0x01:
            ExecAmoswapd(state, instr);
            break;
        case 0x02:
            ExecLrd(state, instr);
            break;
        case 0x03:
            ExecScd(state, instr);
            break;
        case 0x04:
            ExecAmoxord(state, instr);
            break;
        case 0x08:
            ExecAmoord(state, instr);
            break;
        case 0x0c:
            ExecAmoandd(state, instr);
            break;
        case 0x10:
            ExecAmomind(state, instr);
            break;
        case 0x14:
            ExecAmomaxd(state, instr);
            break;
        case 0x18:
            ExecAmominud(state, instr);
            break;
        case 0x1c:
            ExecAmomaxud(state, instr);
            break;
        default:
            Error("Unknown amo instruction: %.2x", funct5);
            break;
        }
    }
}

void ExecInstruction(State *state, uint32_t instr) {
    uint8_t opcode = instr & SetNBits(7);
    // If an instruction is compressed.
    if ((opcode & SetNBits(2)) ^ SetNBits(2)) {
        ExecCompressedInstr(state, (uint16_t)(instr & SetNBits(16)));
        return;
    }

    // 32-bit Instruction.
    switch (opcode) {
    case OP_IMM:
        ExecOpImmInstr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case OP_AUIPC:
        ExecAuipc(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case OP_LUI:
        ExecLui(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case OP:
        ExecOpInstr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case JAL:
        ExecJal(state, instr);
        if (state->excepted) return;
        break;
    case JALR:
        ExecJalr(state, instr);
        if (state->excepted) return;
        break;
    case LOAD:
        ExecLoadInstr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case STORE:
        ExecStoreInstr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case BRANCH:
        ExecBranchInstr(state, instr);
        if (state->excepted) return;
        break;
    case OP_IMM_32:
        ExecOpImm32Instr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case OP_32:
        ExecOp32Instr(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case SYSTEM:
        ExecSystemInstr(state, instr);
        if (state->excepted) return;
        break;
    case MISC_MEM:
        ExecMiscMem(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    case AMO:
        ExecAmo(state, instr);
        if (state->excepted) return;
        state->pc += sizeof(instr);
        break;
    default:
        Error("Unknown opcode: 0x%.2x.", opcode);
        break;
    }
}
