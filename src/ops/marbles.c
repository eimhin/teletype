#include "ops/marbles.h"

#include "helpers.h"
#include "random.h"
#include "teletype.h"
#include "teletype_io.h"

// Marbles-style "deja vu" random looping, backed by Teletype pattern memory.
//
// Each MRBL op drives one pattern bank as an independent deja-vu loop: at every
// call the bank's playhead advances over its first P.L cells, and with
// probability (100 - dv) the current cell is overwritten with a freshly drawn
// random value. dv 0 = always refresh (pure random); dv 100 = never refresh
// (a locked loop of length P.L). Because the loop lives in the pattern, it is
// edited in TRACKER mode and persisted with the scene for free.
//
// The ops draw from a dedicated rng stream (rand_states.s.marbles) so they
// neither perturb nor are perturbed by RAND / TOSS elsewhere. Seed it with the
// MRBL.SD op (defined alongside the other seed ops in seed.c).

static inline uint32_t mrbl_rand(scene_state_t *ss) {
    return random_next(&ss->rand_states.s.marbles.rand);
}

// limit pn to within 0 and PATTERN_COUNT - 1 inclusive
static int16_t mrbl_normalise_pn(int16_t pn) {
    if (pn < 0)
        return 0;
    else if (pn >= PATTERN_COUNT)
        return PATTERN_COUNT - 1;
    else
        return pn;
}

// Advance bank pn's playhead over its first `len` cells; with probability
// (100 - dv) overwrite the current cell with `fresh`. Return the recalled (or
// freshly written) value.
static int16_t mrbl_core(scene_state_t *ss, int16_t p, int16_t dv,
                         int16_t fresh) {
    int16_t pn = mrbl_normalise_pn(p);

    if (dv < 0)
        dv = 0;
    else if (dv > 100)
        dv = 100;

    int16_t len = ss_get_pattern_len(ss, pn);
    if (len <= 0) return 0;

    int16_t i = ss_get_pattern_idx(ss, pn);
    if (i < 0 || i >= len) i = 0;

    if ((int32_t)(mrbl_rand(ss) % 100) >= dv)  // prob (100 - dv): refresh
        ss_set_pattern_val(ss, pn, i, fresh);

    int16_t out = ss_get_pattern_val(ss, pn, i);
    ss_set_pattern_idx(ss, pn, (i + 1) % len);
    tele_pattern_updated();
    return out;
}

// MRBL.V p dv spr -> deja-vu raw value in 0..spr
static void op_MRBL_V_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t p = cs_pop(cs);
    int16_t dv = cs_pop(cs);
    int16_t spr = cs_pop(cs);
    if (spr < 0) spr = 0;
    int16_t fresh = (int16_t)(mrbl_rand(ss) % ((int32_t)spr + 1));
    cs_push(cs, mrbl_core(ss, p, dv, fresh));
}

// MRBL.D p dv deg -> deja-vu scale degree 1..deg (feed straight into N.B)
static void op_MRBL_D_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t p = cs_pop(cs);
    int16_t dv = cs_pop(cs);
    int16_t deg = cs_pop(cs);
    if (deg < 1) deg = 1;
    int16_t fresh = (int16_t)(1 + mrbl_rand(ss) % deg);
    cs_push(cs, mrbl_core(ss, p, dv, fresh));
}

// MRBL.T p dv bias -> deja-vu gate 0/1, bias = % chance a fresh step is ON
static void op_MRBL_T_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t p = cs_pop(cs);
    int16_t dv = cs_pop(cs);
    int16_t bias = cs_pop(cs);
    // bias is only ever compared, never used as a divisor, so it needs no
    // clamp: it saturates naturally (bias <= 0 -> always off, >= 100 -> always
    // on) over the 0..99 draw.
    int16_t fresh = ((int32_t)(mrbl_rand(ss) % 100) < bias) ? 1 : 0;
    cs_push(cs, mrbl_core(ss, p, dv, fresh));
}

// clang-format off
const tele_op_t op_MRBL_V = MAKE_GET_OP(MRBL.V, op_MRBL_V_get, 3, true);
const tele_op_t op_MRBL_D = MAKE_GET_OP(MRBL.D, op_MRBL_D_get, 3, true);
const tele_op_t op_MRBL_T = MAKE_GET_OP(MRBL.T, op_MRBL_T_get, 3, true);
// clang-format on
