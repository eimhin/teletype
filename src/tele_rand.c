#include "tele_rand.h"

void tele_srand(tele_rand_t *r, s16 seed) {
    r->z = 12345;
    r->w = seed;
    r->seed = seed;
}

/*
 * MWC generator concatenates two 16-bit multiply-
 * with-carry generators, x(n)=36969x(n-1)+carry,
 * y(n)=18000y(n-1)+carry mod 2^16, has period about 2^60
 */
s32 tele_rand(tele_rand_t *r) {
    r->z = 36969 * (r->z & 65535) + (r->z >> 16);
    r->w = 18000 * (r->w & 65535) + (r->w >> 16);
    return ((r->z << 16) + r->w) & 0x7FFFFFFF;
}
