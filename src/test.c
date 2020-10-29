#include "rve.h"

void ExecMulhsu(State *state, uint32_t instr);

uint64_t mulhu(uint64_t a, uint64_t b)
{
  uint64_t t;
  uint32_t y1, y2, y3;
  uint64_t a0 = (uint32_t)a, a1 = a >> 32;
  uint64_t b0 = (uint32_t)b, b1 = b >> 32;

  t = a1*b0 + ((a0*b0) >> 32);
  y1 = t;
  y2 = t >> 32;

  t = a0*b1 + y1;
  y1 = t;

  t = a1*b1 + y2 + (t >> 32);
  y2 = t;
  y3 = t >> 32;

  return ((uint64_t)y3 << 32) | y2;
}

int64_t mulhsu(int64_t a, uint64_t b)
{
  int negate = a < 0;
  uint64_t res = mulhu(a < 0 ? -a : a, b);
  return negate ? ~res + (a * b == 0) : res;
}

void RunTest() {
    State *state = NewState(1000);
    ResetState(state);

    state->csr[MSTATUS] = 0;
    WriteCSR(state, MSTATUS, 0, 1, 3);
    assert(state->csr[MSTATUS] == 3);

    state->csr[MSTATUS] = 0;
    WriteCSR(state, MSTATUS, 2, 2, 1);
    assert(state->csr[MSTATUS] == 4);

    state->csr[MSTATUS] = 0;
    WriteCSR(state, MSTATUS, 0, 63, 20);
    assert(ReadCSR(state, MSTATUS, 0, 4) == 20);

    Write8(state, CLINT_BASE, 0x0e);
    assert(Read8(state, CLINT_BASE) == 0x0e);

    assert(GetRange(10, 2, 3) == 2);

    state->x[1] = 0xffffffff80000000;
    state->x[2] = 0xffffffffffff8000;
    printf("a: %lld\n", mulhsu(state->x[1], state->x[2]));
    uint32_t instr = 0x0220a733;
    ExecMulhsu(state, instr);
    printf("%lld, %lld, %lld\n", state->x[1], state->x[2], state->x[14]);
}
