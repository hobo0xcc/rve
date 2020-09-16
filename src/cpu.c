#include "rve.h"

int64_t SetNBits(int32_t n) { return ((uint64_t)1 << n) - 1; }

int64_t SetOneBit(int i) { return (1 << i); }

int64_t Sext(int64_t i, int top_bit) {
    if (i & SetOneBit(top_bit)) {
        i |= SetNBits(64 - top_bit - 1) << (top_bit + 1);
    }

    return i;
}

// TODO: Pass argument of instruction's fields instead of passing `uint32_t
// instr`.

void ExecAddi(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] + immediate;
}

void ExecSlli(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    state->x[rd] = (uint64_t)state->x[rs1] << (uint64_t)immediate;
}

void ExecSlti(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] < immediate;
}

void ExecSltiu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = ((uint64_t)state->x[rs1]) < ((uint64_t)immediate);
}

void ExecXori(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

    state->x[rd] = state->x[rs1] ^ immediate;
}

void ExecSrli(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    state->x[rd] = (uint64_t)state->x[rs1] >> (uint64_t)immediate;
}

void ExecSrai(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t immediate = (instr >> 20) & SetNBits(5);

    state->x[rd] = state->x[rs1] >> immediate;
}

void ExecOri(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t immediate = Sext((instr >> 20) & SetNBits(12), 11);

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
        if (funct7 == 0x00) {
            ExecSrli(state, instr);
        } else if (funct7 == 0x20) {
            ExecSrai(state, instr);
        }
        break;
    case 0x6:
        ExecOri(state, instr);
    case 0x7:
        ExecAndi(state, instr);
        break;
    default:
        Error("Unknown OP_IMM instruction: %2x", funct3);
    }
}

void ExecAuipc(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    int32_t immediate = Sext((instr >> 12) & SetNBits(20), 19);

    state->x[rd] = state->pc + (immediate << 12);
}

void ExecLui(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    int32_t immediate = Sext((instr >> 12) & SetNBits(20), 19);

    state->x[rd] = immediate << 12;
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
        ((__int128_t)state->x[rs1] * (__uint128_t)state->x[rs2]) >> XLEN;
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
        ((__uint128_t)state->x[rs1] * (__uint128_t)state->x[rs2]) >> XLEN;
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

    state->x[rd] = state->x[rs1] / state->x[rs2];
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

    state->x[rd] = (uint64_t)state->x[rs1] / (uint64_t)state->x[rs2];
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

    state->x[rd] = state->x[rs1] % state->x[rs2];
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

    state->x[rd] = (uint64_t)state->x[rs1] % (uint64_t)state->x[rs2];
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
        } else if (funct7 == 0x02) {
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

    int32_t t = state->pc + sizeof(instr);
    state->pc = (state->x[rs1] + offset) & ~1;
    state->x[rd] = t;
}

void ExecLb(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(*(int8_t *)(state->mem + state->x[rs1] + offset), 7);
}

void ExecLh(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(*(int16_t *)(state->mem + state->x[rs1] + offset), 15);
}

void ExecLw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = Sext(*(int32_t *)(state->mem + state->x[rs1] + offset), 31);
}

void ExecLd(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = *(int64_t *)(state->mem + state->x[rs1] + offset);
}

void ExecLbu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = *(uint8_t *)(state->mem + state->x[rs1] + offset);
}

void ExecLhu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = *(uint16_t *)(state->mem + state->x[rs1] + offset);
}

void ExecLwu(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    int32_t offset = Sext(instr >> 20 & SetNBits(12), 11);

    state->x[rd] = *(uint32_t *)(state->mem + state->x[rs1] + offset);
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

    *(uint8_t *)(state->mem + state->x[rs1] + offset) =
        (uint8_t)(state->x[rs2] & SetNBits(8));
}

void ExecSh(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    *(uint16_t *)(state->mem + state->x[rs1] + offset) =
        (uint16_t)(state->x[rs2] & SetNBits(16));
}

void ExecSw(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    *(uint32_t *)(state->mem + state->x[rs1] + offset) =
        (uint32_t)state->x[rs2];
}

void ExecSd(State *state, uint32_t instr) {
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);
    int32_t offset = Sext(
        ((instr >> 25 & SetNBits(7)) << 5) | ((instr >> 7 & SetNBits(5))), 11);

    *(uint64_t *)(state->mem + state->x[rs1] + offset) =
        (uint64_t)state->x[rs2];
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

    state->x[rd] = Sext((uint64_t)state->x[rs1] << immediate, 31);
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

    state->x[rd] = Sext((state->x[rs1] & SetNBits(32)) >> immediate, 31);
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
        Sext((uint32_t)(state->x[rs1] << state->x[rs2]) & SetNBits(32), 31);
}

void ExecDivw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((state->x[rs1] / state->x[rs2]) & SetNBits(32), 31);
}

void ExecSrlw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((uint32_t)state->x[rs1] >> state->x[rs2], 32);
}

void ExecSraw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] =
        Sext((state->x[rs1] & SetNBits(32)) >> (rs2 & SetNBits(5)), 31);
}

void ExecRemw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext((state->x[rs1] % state->x[rs2]) & SetNBits(32), 31);
}

void ExecRemuw(State *state, uint32_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint8_t rs1 = (instr >> 15) & SetNBits(5);
    uint8_t rs2 = (instr >> 20) & SetNBits(5);

    state->x[rd] = Sext(
        (state->x[rs1] & SetNBits(32)) % (state->x[rs2] & SetNBits(32)), 31);
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
    int8_t rd = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] = state->x[2] + uimm;
}

void ExecCLw(State *state, uint16_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(1)) << 6) |
                   ((instr >> 6 & SetNBits(1)) << 1);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rd = (instr >> 2) & SetNBits(3);

    state->x[8 + rd] =
        Sext(*(int32_t *)(state->mem + state->x[8 + rs1] + uimm), 31);
}

void ExecCLd(State *state, uint32_t instr) {
    uint8_t uimm =
        ((instr >> 5 & SetNBits(2)) << 6) | ((instr >> 10 & SetNBits(3)) << 3);
    uint8_t rs1 = instr >> 7 & SetNBits(3);
    uint8_t rd = instr >> 2 & SetNBits(3);

    state->x[8 + rd] =
        Sext(*(int64_t *)(state->mem + state->x[8 + rs1] + uimm), 31);
}

void ExecCSw(State *state, uint16_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(1)) << 6) |
                   ((instr >> 6 & SetNBits(1)) << 1);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    *(uint32_t *)(state->mem + state->x[8 + rs1] + uimm) = state->x[8 + rs2];
}

void ExecCSd(State *state, uint32_t instr) {
    uint8_t uimm = ((instr >> 10 & SetNBits(3)) << 3) |
                   ((instr >> 5 & SetNBits(1)) << 6) |
                   ((instr >> 6 & SetNBits(1)) << 1);
    uint8_t rs1 = (instr >> 7) & SetNBits(3);
    uint8_t rs2 = (instr >> 2) & SetNBits(3);

    *(uint64_t *)(state->mem + state->x[8 + rs1] + uimm) = state->x[8 + rs2];
}

void ExecCAddi(State *state, uint16_t instr) {
    int32_t imm = Sext(
        ((instr >> 12 & SetNBits(1)) << 5) | (instr >> 2 & SetNBits(5)), 5);

    if (imm == 0) {
        Error("Invalid c.addi immediate");
    }
    uint8_t rd = (instr >> 7) & SetNBits(5);

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
                           ((instr >> 4 & SetNBits(2)) << 6) |
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
                           ((instr >> 4 & SetNBits(2)) << 6) |
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

    state->x[rd] = state->x[rd] << uimm;
}

void ExecCLwsp(State *state, uint16_t instr) {
    uint8_t rd = (instr >> 7) & SetNBits(5);
    uint32_t uimm = ((instr >> 2 & SetNBits(2)) << 6) |
                    ((instr >> 12 & SetNBits(1)) << 5) |
                    ((instr >> 4 & SetNBits(3)) << 2);

    state->x[rd] = *(int32_t *)(state->mem + state->x[2] + uimm);
}

void ExecCLdsp(State *state, uint32_t instr) {
    uint8_t rd = instr >> 7 & SetNBits(5);
    uint32_t uimm = (instr >> 2 & SetNBits(3)) << 6 |
                    (instr >> 12 & SetNBits(1)) << 5 |
                    (instr >> 5 & SetNBits(2)) << 3;

    state->x[rd] = *(uint64_t *)(state->mem + state->x[2] + uimm);
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

    *(int32_t *)(state->mem + state->x[2] + uimm) = state->x[rs2];
}

void ExecCSdsp(State *state, uint32_t instr) {
    uint8_t rs2 = instr >> 2 & SetNBits(5);
    uint32_t uimm =
        (instr >> 7 & SetNBits(3)) << 6 | (instr >> 10 & SetNBits(3)) << 3;

    *(int64_t *)(state->mem + state->x[2] + uimm) = state->x[rs2];
}

void ExecCompressedInstr(State *state, uint16_t instr) {
    if (instr == 0) {
        Error("Invalid instruction: zero");
    }
    uint8_t opcode = instr & SetNBits(2);

    switch (opcode) {
    case 0x0: {
        uint8_t rd = (instr >> 2) & SetNBits(2);
        int8_t funct3 = (instr >> 13) & SetNBits(3);
        uint8_t f_5_12 = (instr >> 5) & SetNBits(8);

        if (funct3 == 0x0 && f_5_12 != 0) {
            ExecCAddi4spn(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x1) {
            // TODO: Implement c.fld.
            state->pc += 2;
        } else if (funct3 == 0x2) {
            ExecCLw(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLd(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x5) {
            // TODO: Implement c.fsd.
            state->pc += 2;
        } else if (funct3 == 0x6) {
            ExecCSw(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x7) {
            ExecCSd(state, instr);
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

        if (funct6 == 0x23 && funct2_2 == 0x0) {
            ExecCSub(state, instr);
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x1) {
            ExecCXor(state, instr);
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x2) {
            ExecCOr(state, instr);
            state->pc += 2;
        } else if (funct6 == 0x23 && funct2_2 == 0x3) {
            ExecCAnd(state, instr);
            state->pc += 2;
        } else if (funct6 == 0x27 && funct2_2 == 0x0) {
            ExecCSubw(state, instr);
            state->pc += 2;
        } else if (funct6 == 0x27 && funct2_2 == 0x1) {
            ExecCAddw(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x0) {
            ExecCAddi(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x1) {
            ExecCAddiw(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x2) {
            ExecCLi(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x3 && immediate != 0 && r1 == 2) {
            ExecCAddi16sp(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLui(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x0) {
            ExecCSrli(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x1) {
            ExecCSrai(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && funct2_1 == 0x2) {
            ExecCAndi(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x5) {
            ExecCJ(state, instr);
        } else if (funct3 == 0x6) {
            ExecCBeqz(state, instr);
        } else if (funct3 == 0x7) {
            ExecCBnez(state, instr);
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
            // TODO: Understand c.slli64.
        } else if (funct3 == 0x0) {
            ExecCSlli(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x1) {
            // TODO: Implement c.fldsp.
        } else if (funct3 == 0x2 && f_7_11 != 0) {
            ExecCLwsp(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x3) {
            ExecCLdsp(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 0 && f_7_11 != 0 && f_2_6 == 0) {
            ExecCJr(state, instr);
        } else if (funct3 == 0x4 && f_12 == 0 && f_7_11 != 0 && f_2_6 != 0) {
            ExecCMv(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 == 0 && f_2_6 == 0) {
            // ExecCEbreak(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 != 0 && f_2_6 == 0) {
            ExecCJalr(state, instr);
        } else if (funct3 == 0x4 && f_12 == 1 && f_7_11 != 0 && f_2_6 != 0) {
            ExecCAdd(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x5) {
            // TODO: Implement c.fsdsp.
            state->pc += 2;
        } else if (funct3 == 0x6) {
            ExecCSwsp(state, instr);
            state->pc += 2;
        } else if (funct3 == 0x7) {
            ExecCSdsp(state, instr);
            state->pc += 2;
        } else {
            Error("Invalid instruction");
        }
    } break;
    }
}

void ExecEcall(State *state, uint32_t instr) {
    // TODO: Implement exception.
    printf("Exception\n");
}

void ExecEbreak(State *state, uint32_t instr) {
    // TODO: Implement break point exception.
    printf("Break\n");
}

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

    switch (funct3) {
    case 0x0: {
        uint32_t funct7 = instr >> 20 & SetNBits(12);
        if (funct7 == 0x00) {
            ExecEcall(state, instr);
        } else if (funct7 == 0x01) {
            ExecEbreak(state, instr);
        }
    }

    break;
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
        state->pc += sizeof(instr);
        break;
    case OP_AUIPC:
        ExecAuipc(state, instr);
        state->pc += sizeof(instr);
        break;
    case OP_LUI:
        ExecLui(state, instr);
        state->pc += sizeof(instr);
        break;
    case OP:
        ExecOpInstr(state, instr);
        state->pc += sizeof(instr);
        break;
    case JAL:
        ExecJal(state, instr);
        break;
    case JALR:
        ExecJalr(state, instr);
        break;
    case LOAD:
        ExecLoadInstr(state, instr);
        state->pc += sizeof(instr);
        break;
    case STORE:
        ExecStoreInstr(state, instr);
        state->pc += sizeof(instr);
        break;
    case BRANCH:
        ExecBranchInstr(state, instr);
        break;
    case OP_IMM_32:
        ExecOpImm32Instr(state, instr);
        state->pc += sizeof(instr);
        break;
    case OP_32:
        ExecOp32Instr(state, instr);
        state->pc += sizeof(instr);
        break;
    case SYSTEM:
        ExecSystemInstr(state, instr);
        state->pc += sizeof(instr);
        break;
    default:
        Error("Unknown opcode: 0x%.2x.", opcode);
        break;
    }
}
