#include "ops/custom_pattern.h"

#include "helpers.h"
#include "random.h"
#include "teletype.h"
#include "teletype_io.h"

////////////////////////////////////////////////////////////////////////////////
// Helpers /////////////////////////////////////////////////////////////////////

// limit col to within 0 and CUSTOM_PATTERN_WIDTH - 1 inclusive
static int16_t normalise_col(const int16_t col) {
    if (col < 0)
        return 0;
    else if (col >= CUSTOM_PATTERN_WIDTH)
        return CUSTOM_PATTERN_WIDTH - 1;
    else
        return col;
}

// ensure the index is within bounds for the given column, adjusting for
// negative indices (they index from the back, relative to the column's len)
static int16_t normalise_cp_idx(scene_state_t *ss, const int16_t col,
                                int16_t idx) {
    const int16_t len = ss_get_cp_len(ss, col);
    if (idx < 0) {
        if (idx == len)
            idx = 0;
        else if (idx < -len)
            idx = 0;
        else
            idx = len + idx;
    }

    if (idx >= CUSTOM_PATTERN_LENGTH) idx = CUSTOM_PATTERN_LENGTH - 1;
    if (idx < 0) idx = 0;

    return idx;
}

// Increment a column's idx obeying its START, END, WRAP and L. Mirrors
// p_next_inc_i in patterns.c.
static void xp_next_inc_i(scene_state_t *ss, int16_t col) {
    col = normalise_col(col);

    const int16_t len = ss_get_cp_len(ss, col);
    const int16_t start = ss_get_cp_start(ss, col);
    const int16_t end = ss_get_cp_end(ss, col);
    const uint16_t wrap = ss_get_cp_wrap(ss, col);

    int16_t idx = ss_get_cp_idx(ss, col);

    if ((idx == (len - 1)) || (idx == end)) {
        if (wrap) idx = start;
    }
    else
        idx++;

    if (idx > len || idx < 0 || idx >= CUSTOM_PATTERN_LENGTH) idx = 0;

    ss_set_cp_idx(ss, col, idx);
}

////////////////////////////////////////////////////////////////////////////////
// XP get / set (value at column, index) /////////////////////////////////////////

static void op_XP_get(const void *NOTUSED(data), scene_state_t *ss,
                      exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t idx = normalise_cp_idx(ss, col, cs_pop(cs));
    cs_push(cs, ss_get_cp_val(ss, col, idx));
}

static void op_XP_set(const void *NOTUSED(data), scene_state_t *ss,
                      exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t idx = normalise_cp_idx(ss, col, cs_pop(cs));
    int16_t val = cs_pop(cs);
    ss_set_cp_val(ss, col, idx, val);
    tele_pattern_updated();
}

const tele_op_t op_XP = MAKE_GET_SET_OP(XP, op_XP_get, op_XP_set, 2, true);

////////////////////////////////////////////////////////////////////////////////
// XP.HERE (value at the column's current index) /////////////////////////////////

static void op_XP_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_val(ss, col, ss_get_cp_idx(ss, col)));
}

static void op_XP_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t val = cs_pop(cs);
    ss_set_cp_val(ss, col, ss_get_cp_idx(ss, col), val);
    tele_pattern_updated();
}

const tele_op_t op_XP_HERE =
    MAKE_GET_SET_OP(XP.HERE, op_XP_HERE_get, op_XP_HERE_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.I (column playhead index) //////////////////////////////////////////////////

static void op_XP_I_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_idx(ss, col));
}

static void op_XP_I_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t i = normalise_cp_idx(ss, col, cs_pop(cs));
    int16_t len = ss_get_cp_len(ss, col);
    if (i < 0 || len == 0)
        ss_set_cp_idx(ss, col, 0);
    else if (i >= len)
        ss_set_cp_idx(ss, col, len - 1);
    else
        ss_set_cp_idx(ss, col, i);
    // Reset the dwell counter so the next XP.STEP enters the new stage fresh
    // (parity with P.I / p_step).
    ss->cp_dwell[col] = 0;
    tele_pattern_updated();
}

const tele_op_t op_XP_I = MAKE_GET_SET_OP(XP.I, op_XP_I_get, op_XP_I_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.L (column length) //////////////////////////////////////////////////////////

static void op_XP_L_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_len(ss, col));
}

static void op_XP_L_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t l = cs_pop(cs);
    if (l < 0)
        ss_set_cp_len(ss, col, 0);
    else if (l > CUSTOM_PATTERN_LENGTH)
        ss_set_cp_len(ss, col, CUSTOM_PATTERN_LENGTH);
    else
        ss_set_cp_len(ss, col, l);
    tele_pattern_updated();
}

const tele_op_t op_XP_L = MAKE_GET_SET_OP(XP.L, op_XP_L_get, op_XP_L_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.WRAP (column wrap flag) /////////////////////////////////////////////////////

static void op_XP_WRAP_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_wrap(ss, col));
}

static void op_XP_WRAP_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t a = cs_pop(cs);
    ss_set_cp_wrap(ss, col, a >= 1);
}

const tele_op_t op_XP_WRAP =
    MAKE_GET_SET_OP(XP.WRAP, op_XP_WRAP_get, op_XP_WRAP_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.START (column playback start) ///////////////////////////////////////////////

static void op_XP_START_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_start(ss, col));
}

static void op_XP_START_set(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t a = normalise_cp_idx(ss, col, cs_pop(cs));
    ss_set_cp_start(ss, col, a);
    tele_pattern_updated();
}

const tele_op_t op_XP_START =
    MAKE_GET_SET_OP(XP.START, op_XP_START_get, op_XP_START_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.END (column playback end) ///////////////////////////////////////////////////

static void op_XP_END_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_end(ss, col));
}

static void op_XP_END_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t a = normalise_cp_idx(ss, col, cs_pop(cs));
    ss_set_cp_end(ss, col, a);
    tele_pattern_updated();
}

const tele_op_t op_XP_END =
    MAKE_GET_SET_OP(XP.END, op_XP_END_get, op_XP_END_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.NEXT (advance one column) ///////////////////////////////////////////////////

static void op_XP_NEXT_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t col = normalise_col(cs_pop(cs));
    xp_next_inc_i(ss, col);
    cs_push(cs, ss_get_cp_val(ss, col, ss_get_cp_idx(ss, col)));
    tele_pattern_updated();
}

static void op_XP_NEXT_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t col = normalise_col(cs_pop(cs));
    int16_t a = cs_pop(cs);
    xp_next_inc_i(ss, col);
    ss_set_cp_val(ss, col, ss_get_cp_idx(ss, col), a);
    tele_pattern_updated();
}

const tele_op_t op_XP_NEXT =
    MAKE_GET_SET_OP(XP.NEXT, op_XP_NEXT_get, op_XP_NEXT_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.NEXT.ALL (advance every column once, each by its own range) //////////////////

static void op_XP_NEXT_ALL_get(const void *NOTUSED(data), scene_state_t *ss,
                               exec_state_t *NOTUSED(es),
                               command_state_t *NOTUSED(cs)) {
    for (int16_t col = 0; col < CUSTOM_PATTERN_WIDTH; col++)
        xp_next_inc_i(ss, col);
    tele_pattern_updated();
}

const tele_op_t op_XP_NEXT_ALL =
    MAKE_GET_OP(XP.NEXT.ALL, op_XP_NEXT_ALL_get, 0, false);

////////////////////////////////////////////////////////////////////////////////
// XP.D, XP.D.HERE — per-cell dwell duration (ticks) used by XP.STEP
// /////////////// Mirrors the val[] accessor family (XP, XP.HERE) but on the
// dur[] grid. ss_set_cp_dur clamps dur < 1 to 1.

static int16_t cp_d_get(scene_state_t *ss, int16_t col, int16_t idx) {
    col = normalise_col(col);
    idx = normalise_cp_idx(ss, col, idx);
    return ss_get_cp_dur(ss, col, idx);
}

static void cp_d_set(scene_state_t *ss, int16_t col, int16_t idx, int16_t dur) {
    col = normalise_col(col);
    idx = normalise_cp_idx(ss, col, idx);
    ss_set_cp_dur(ss, col, idx, dur);
    tele_pattern_updated();
}

static void op_XP_D_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    cs_push(cs, cp_d_get(ss, col, idx));
}

static void op_XP_D_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t dur = cs_pop(cs);
    cp_d_set(ss, col, idx, dur);
}

const tele_op_t op_XP_D =
    MAKE_GET_SET_OP(XP.D, op_XP_D_get, op_XP_D_set, 2, true);

static void op_XP_D_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss_get_cp_dur(ss, col, ss_get_cp_idx(ss, col)));
}

static void op_XP_D_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    int16_t dur = cs_pop(cs);
    ss_set_cp_dur(ss, col, ss_get_cp_idx(ss, col), dur);
    tele_pattern_updated();
}

const tele_op_t op_XP_D_HERE =
    MAKE_GET_SET_OP(XP.D.HERE, op_XP_D_HERE_get, op_XP_D_HERE_set, 1, true);

////////////////////////////////////////////////////////////////////////////////
// XP.D.RND, XP.D.RND.M, XP.D.RND.N — duration randomisers (mirror P.D.RND*)
// ///////

// Randomise dur[] for a column's cells 0..len-1 to a value in lo..hi inclusive.
// lo/hi clamped >= 1 and swapped if lo > hi (a freeze-the-sequencer 0 is
// impossible). Cells beyond the length are left alone since they never play.
static void cp_d_rnd(scene_state_t *ss, int16_t col, int16_t lo, int16_t hi) {
    col = normalise_col(col);
    int16_t len = ss_get_cp_len(ss, col);
    if (len < 1) return;

    if (lo < 1) lo = 1;
    if (hi < 1) hi = 1;
    if (lo > hi) {
        int16_t t = lo;
        lo = hi;
        hi = t;
    }

    random_state_t *r = &ss->rand_states.s.pattern.rand;
    int16_t range = hi - lo + 1;
    for (int16_t i = 0; i < len; i++) {
        int16_t v = (int16_t)(random_next(r) % range) + lo;
        ss_set_cp_dur(ss, col, i, v);
    }
    tele_pattern_updated();
}

// Randomise dur[] for a column's cells 0..len-1 so they sum to exactly
// `target`. Every cell gets at least 1; the surplus is scattered one tick at a
// time. No-op if `target` can't give every cell >= 1. `col` must be normalised.
static void cp_d_rnd_sum(scene_state_t *ss, int16_t col, int32_t target) {
    int16_t len = ss_get_cp_len(ss, col);
    if (len < 1 || target < len) return;

    random_state_t *r = &ss->rand_states.s.pattern.rand;
    for (int16_t i = 0; i < len; i++) ss_set_cp_dur(ss, col, i, 1);
    for (int32_t surplus = target - len; surplus > 0; surplus--) {
        int16_t i = (int16_t)(random_next(r) % len);
        ss_set_cp_dur(ss, col, i, ss_get_cp_dur(ss, col, i) + 1);
    }
    tele_pattern_updated();
}

// Randomise durations so the column cycle sums to the smallest multiple of n
// that is >= len (so each cell can be >= 1) -- equals n when len <= n.
static void cp_d_rnd_m(scene_state_t *ss, int16_t col, int16_t n) {
    col = normalise_col(col);
    int16_t len = ss_get_cp_len(ss, col);
    if (len < 1) return;
    if (n < 1) n = 1;
    int32_t target = (((int32_t)len + n - 1) / n) * n;
    cp_d_rnd_sum(ss, col, target);
}

// Randomise durations so the column cycle sums to exactly n. No-op when n <
// len.
static void cp_d_rnd_n(scene_state_t *ss, int16_t col, int16_t n) {
    col = normalise_col(col);
    if (n < 1) n = 1;
    cp_d_rnd_sum(ss, col, n);
}

static void op_XP_D_RND_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    int16_t lo = cs_pop(cs);
    int16_t hi = cs_pop(cs);
    cp_d_rnd(ss, col, lo, hi);
}

const tele_op_t op_XP_D_RND = MAKE_GET_OP(XP.D.RND, op_XP_D_RND_get, 3, false);

static void op_XP_D_RND_M_get(const void *NOTUSED(data), scene_state_t *ss,
                              exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cp_d_rnd_m(ss, col, n);
}

const tele_op_t op_XP_D_RND_M =
    MAKE_GET_OP(XP.D.RND.M, op_XP_D_RND_M_get, 2, false);

static void op_XP_D_RND_N_get(const void *NOTUSED(data), scene_state_t *ss,
                              exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cp_d_rnd_n(ss, col, n);
}

const tele_op_t op_XP_D_RND_N =
    MAKE_GET_OP(XP.D.RND.N, op_XP_D_RND_N_get, 2, false);

////////////////////////////////////////////////////////////////////////////////
// XP.STEP, XP.STEP.NEW, XP.STEP?, XP.STEP.ALL
// ///////////////////////////////////// Duration-aware playhead advance,
// mirroring the P.STEP family. cp_step bumps a per-column dwell counter and
// only advances idx (via xp_next_inc_i, which honours START/END/WRAP/L) once
// dwell reaches the current cell's dur[]. cp_just_advanced is 1 on the tick a
// stage just began (incl. the first step after init or an XP.I reset).

static int16_t cp_step(scene_state_t *ss, int16_t col) {
    col = normalise_col(col);
    const int16_t dur = ss_get_cp_dur(ss, col, ss_get_cp_idx(ss, col));

    if (ss->cp_dwell[col] == 0) {
        // entering a stage fresh (after init or an XP.I write).
        ss->cp_dwell[col] = 1;
        ss->cp_just_advanced[col] = 1;
    }
    else if (ss->cp_dwell[col] + 1 > dur) {
        // current stage is over — advance to the next cell.
        xp_next_inc_i(ss, col);
        ss->cp_dwell[col] = 1;
        ss->cp_just_advanced[col] = 1;
    }
    else {
        ss->cp_dwell[col] = ss->cp_dwell[col] + 1;
        ss->cp_just_advanced[col] = 0;
    }
    return ss_get_cp_val(ss, col, ss_get_cp_idx(ss, col));
}

static void op_XP_STEP_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = cs_pop(cs);
    cs_push(cs, cp_step(ss, col));
    tele_pattern_updated();
}

const tele_op_t op_XP_STEP = MAKE_GET_OP(XP.STEP, op_XP_STEP_get, 1, true);

static void op_XP_STEP_NEW_get(const void *NOTUSED(data), scene_state_t *ss,
                               exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cs_push(cs, ss->cp_just_advanced[col] ? 1 : 0);
}

const tele_op_t op_XP_STEP_NEW =
    MAKE_GET_OP(XP.STEP.NEW, op_XP_STEP_NEW_get, 1, true);

// XP.STEP? — advance like XP.STEP but push the just_advanced flag instead of
// val[], so a script can gate in one op: `IF XP.STEP? 0: SCRIPT 2`.
static void op_XP_STEPQ_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t col = normalise_col(cs_pop(cs));
    cp_step(ss, col);
    cs_push(cs, ss->cp_just_advanced[col] ? 1 : 0);
    tele_pattern_updated();
}

const tele_op_t op_XP_STEPQ = MAKE_GET_OP(XP.STEP?, op_XP_STEPQ_get, 1, true);

// XP.STEP.ALL — dwell-step every column once, each by its own duration/range.
// Duration-aware counterpart to XP.NEXT.ALL. No return.
static void op_XP_STEP_ALL_get(const void *NOTUSED(data), scene_state_t *ss,
                               exec_state_t *NOTUSED(es),
                               command_state_t *NOTUSED(cs)) {
    for (int16_t col = 0; col < CUSTOM_PATTERN_WIDTH; col++) cp_step(ss, col);
    tele_pattern_updated();
}

const tele_op_t op_XP_STEP_ALL =
    MAKE_GET_OP(XP.STEP.ALL, op_XP_STEP_ALL_get, 0, false);
