#include "rve.h"

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
}
