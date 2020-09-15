#include "rve.h"

Vec *CreateVec() {
    Vec *v = malloc(sizeof(Vec));
    v->cap = sizeof(void *);
    v->len = 0;
    v->data = malloc(sizeof(void *));
    return v;
}

void GrowVec(Vec *v, int n) {
    int size = (v->len + n) * sizeof(void *);
    if (v->cap >= size)
        return;

    while (v->cap < size)
        v->cap *= 2;
    v->data = realloc(v->data, v->cap);
    if (v->data == NULL)
        Error("Failed to allocate memory");
}

void PushVec(Vec *v, void *item) {
    GrowVec(v, 1);
    v->data[v->len++] = item;
}

void *GetVec(Vec *v, int idx) {
    if (v->len <= idx || idx < 0) {
        Error("Index out of range in Vec: %d", idx);
    }

    return v->data[idx];
}
