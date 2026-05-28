#include "ops/patterns.h"

#include <string.h>

#include "helpers.h"
#include "random.h"
#include "teletype.h"
#include "teletype_io.h"

////////////////////////////////////////////////////////////////////////////////
// Helpers /////////////////////////////////////////////////////////////////////

// limit pn to within 0 and PATTERN_COUNT - 1 inclusive
static int16_t normalise_pn(const int16_t pn) {
    if (pn < 0)
        return 0;
    else if (pn >= PATTERN_COUNT)
        return PATTERN_COUNT - 1;
    else
        return pn;
}

// ensure that the pattern index is within bounds
// also adjust for negative indices (they index from the back)
static int16_t normalise_idx(scene_state_t *ss, const int16_t pn, int16_t idx) {
    const int16_t len = ss_get_pattern_len(ss, pn);
    if (idx < 0) {
        if (idx == len)
            idx = 0;
        else if (idx < -len)
            idx = 0;
        else
            idx = len + idx;
    }

    if (idx >= PATTERN_LENGTH) idx = PATTERN_LENGTH - 1;

    return idx;
}

static int16_t wrap(int16_t value, int16_t a, int16_t b) {
    int16_t c, i = value;
    if (a < b) {
        c = b - a + 1;
        while (i >= b) i -= c;
        while (i < a) i += c;
    }
    else {
        c = a - b + 1;
        while (i >= a) i -= c;
        while (i < b) i += c;
    }
    return i;
}

////////////////////////////////////////////////////////////////////////////////
// P.N /////////////////////////////////////////////////////////////////////////

static void op_P_N_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, ss->variables.p_n);
}

static void op_P_N_set(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t a = cs_pop(cs);
    ss->variables.p_n = normalise_pn(a);
}

const tele_op_t op_P_N = MAKE_GET_SET_OP(P.N, op_P_N_get, op_P_N_set, 0, true);


////////////////////////////////////////////////////////////////////////////////
// P and PN ////////////////////////////////////////////////////////////////////

// Get
static int16_t p_get(scene_state_t *ss, int16_t pn, int16_t idx) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    return ss_get_pattern_val(ss, pn, idx);
}

static void op_P_get(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    cs_push(cs, p_get(ss, pn, a));
}

static void op_PN_get(const void *NOTUSED(data), scene_state_t *ss,
                      exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    cs_push(cs, p_get(ss, pn, a));
}

// Set
static void p_set(scene_state_t *ss, int16_t pn, int16_t idx, int16_t val) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    ss_set_pattern_val(ss, pn, idx, val);
    tele_pattern_updated();
}

static void op_P_set(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_set(ss, pn, a, b);
}

static void op_PN_set(const void *NOTUSED(data), scene_state_t *ss,
                      exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_set(ss, pn, a, b);
}

// Make ops
const tele_op_t op_P = MAKE_GET_SET_OP(P, op_P_get, op_P_set, 1, true);
const tele_op_t op_PN = MAKE_GET_SET_OP(PN, op_PN_get, op_PN_set, 2, true);


////////////////////////////////////////////////////////////////////////////////
// P.L and PN.L ////////////////////////////////////////////////////////////////

// Get
static void op_P_L_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_len(ss, pn));
}

static void op_PN_L_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_len(ss, pn));
}

// Set
static void p_l_set(scene_state_t *ss, int16_t pn, int16_t l) {
    pn = normalise_pn(pn);
    if (l < 0)
        ss_set_pattern_len(ss, pn, 0);
    else if (l > PATTERN_LENGTH)
        ss_set_pattern_len(ss, pn, PATTERN_LENGTH);
    else
        ss_set_pattern_len(ss, pn, l);
    tele_pattern_updated();
}

static void op_P_L_set(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    p_l_set(ss, pn, a);
}

static void op_PN_L_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    p_l_set(ss, pn, a);
}

// Make ops
const tele_op_t op_P_L = MAKE_GET_SET_OP(P.L, op_P_L_get, op_P_L_set, 0, true);
const tele_op_t op_PN_L =
    MAKE_GET_SET_OP(PN.L, op_PN_L_get, op_PN_L_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.WRAP and PN.WRAP //////////////////////////////////////////////////////////

// Get
static void op_P_WRAP_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_wrap(ss, pn));
}

static void op_PN_WRAP_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_wrap(ss, pn));
}

// Set
static void op_P_WRAP_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    ss_set_pattern_wrap(ss, pn, a >= 1);
}

static void op_PN_WRAP_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    ss_set_pattern_wrap(ss, pn, a >= 1);
}

// Make ops
const tele_op_t op_P_WRAP =
    MAKE_GET_SET_OP(P.WRAP, op_P_WRAP_get, op_P_WRAP_set, 0, true);
const tele_op_t op_PN_WRAP =
    MAKE_GET_SET_OP(PN.WRAP, op_PN_WRAP_get, op_PN_WRAP_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.START and PN.START ////////////////////////////////////////////////////////

// Get
static void op_P_START_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_start(ss, pn));
}

static void op_PN_START_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_start(ss, pn));
}

// Set
static void op_P_START_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = normalise_idx(ss, pn, cs_pop(cs));
    ss_set_pattern_start(ss, pn, a);
    tele_pattern_updated();
}

static void op_PN_START_set(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = normalise_idx(ss, pn, cs_pop(cs));
    ss_set_pattern_start(ss, pn, a);
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_START =
    MAKE_GET_SET_OP(P.START, op_P_START_get, op_P_START_set, 0, true);
const tele_op_t op_PN_START =
    MAKE_GET_SET_OP(PN.START, op_PN_START_get, op_PN_START_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.END and PN.END ////////////////////////////////////////////////////////////

// Get
static void op_P_END_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_end(ss, pn));
}

static void op_PN_END_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_end(ss, pn));
}

// Set
static void op_P_END_set(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = normalise_idx(ss, pn, cs_pop(cs));
    ss_set_pattern_end(ss, pn, a);
    tele_pattern_updated();
}

static void op_PN_END_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = normalise_idx(ss, pn, cs_pop(cs));
    ss_set_pattern_end(ss, pn, a);
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_END =
    MAKE_GET_SET_OP(P.END, op_P_END_get, op_P_END_set, 0, true);
const tele_op_t op_PN_END =
    MAKE_GET_SET_OP(PN.END, op_PN_END_get, op_PN_END_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.I and PN.I ////////////////////////////////////////////////////////////////

// Get
static void op_P_I_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_idx(ss, pn));
}

static void op_PN_I_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_idx(ss, pn));
}

// Set
static void p_i_set(scene_state_t *ss, int16_t pn, int16_t i) {
    pn = normalise_pn(pn);
    i = normalise_idx(ss, pn, i);
    int16_t len = ss_get_pattern_len(ss, pn);
    if (i < 0 || len == 0)
        ss_set_pattern_idx(ss, pn, 0);
    else if (i >= len)
        ss_set_pattern_idx(ss, pn, len - 1);
    else
        ss_set_pattern_idx(ss, pn, i);
    // Writing idx invalidates dwell: a fresh stage starts on the next
    // P.STEP, with STEP.NEW=1. Single rule covers init (P.I 0 in #I)
    // and mid-run resets from a reset jack alike.
    ss->p_dwell[pn] = 0;
    ss->p_just_advanced[pn] = 0;
    // Re-seed mode-internal travel direction from the persisted base dir so
    // the next P.STEP starts cleanly (e.g. a pendulum bounce in progress
    // doesn't carry over after a reset).
    ss->p_travel_dir[pn] = ss_get_pattern_dir(ss, pn) ? -1 : 1;
    tele_pattern_updated();
}

static void op_P_I_set(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    p_i_set(ss, pn, a);
}

static void op_PN_I_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    p_i_set(ss, pn, a);
}

// Make ops
const tele_op_t op_P_I = MAKE_GET_SET_OP(P.I, op_P_I_get, op_P_I_set, 0, true);
const tele_op_t op_PN_I =
    MAKE_GET_SET_OP(PN.I, op_PN_I_get, op_PN_I_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.HERE and PN.HERE //////////////////////////////////////////////////////////

// Get
static void op_P_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
}

static void op_PN_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
}

// Set
static void op_P_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

static void op_PN_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_HERE =
    MAKE_GET_SET_OP(P.HERE, op_P_HERE_get, op_P_HERE_set, 0, true);
const tele_op_t op_PN_HERE =
    MAKE_GET_SET_OP(PN.HERE, op_PN_HERE_get, op_PN_HERE_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.NEXT //////////////////////////////////////////////////////////////////////

// Increment I obeying START, END, WRAP and L
static void p_next_inc_i(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);

    const int16_t len = ss_get_pattern_len(ss, pn);
    const int16_t start = ss_get_pattern_start(ss, pn);
    const int16_t end = ss_get_pattern_end(ss, pn);
    const uint16_t wrap = ss_get_pattern_wrap(ss, pn);

    int16_t idx = ss_get_pattern_idx(ss, pn);

    if ((idx == (len - 1)) || (idx == end)) {
        if (wrap) idx = start;
    }
    else
        idx++;

    if (idx > len || idx < 0 || idx >= PATTERN_LENGTH) idx = 0;

    ss_set_pattern_idx(ss, pn, idx);
}

// Get
static void op_P_NEXT_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(ss->variables.p_n);
    p_next_inc_i(ss, pn);
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
    tele_pattern_updated();
}

static void op_PN_NEXT_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(cs_pop(cs));
    p_next_inc_i(ss, pn);
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
    tele_pattern_updated();
}

// Set
static void op_P_NEXT_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    p_next_inc_i(ss, pn);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

static void op_PN_NEXT_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    p_next_inc_i(ss, pn);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_NEXT =
    MAKE_GET_SET_OP(P.NEXT, op_P_NEXT_get, op_P_NEXT_set, 0, true);
const tele_op_t op_PN_NEXT =
    MAKE_GET_SET_OP(PN.NEXT, op_PN_NEXT_get, op_PN_NEXT_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.PREV //////////////////////////////////////////////////////////////////////

// Increment I obeying START, END, WRAP and L
static void p_prev_dec_i(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);

    const int16_t len = ss_get_pattern_len(ss, pn);
    const int16_t start = ss_get_pattern_start(ss, pn);
    const int16_t end = ss_get_pattern_end(ss, pn);
    const uint16_t wrap = ss_get_pattern_wrap(ss, pn);

    int16_t idx = ss_get_pattern_idx(ss, pn);

    if ((idx == 0) || (idx == start)) {
        if (wrap) {
            if (end < len)
                idx = end;
            else
                idx = len - 1;
        }
    }
    else
        idx--;

    ss_set_pattern_idx(ss, pn, idx);
}

// Get
static void op_P_PREV_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(ss->variables.p_n);
    p_prev_dec_i(ss, pn);
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
    tele_pattern_updated();
}

static void op_PN_PREV_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = cs_pop(cs);
    p_prev_dec_i(ss, pn);
    cs_push(cs, ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn)));
    tele_pattern_updated();
}

// Set
static void op_P_PREV_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = normalise_pn(ss->variables.p_n);
    const int16_t a = cs_pop(cs);
    p_prev_dec_i(ss, pn);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

static void op_PN_PREV_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    const int16_t pn = cs_pop(cs);
    const int16_t a = cs_pop(cs);
    p_prev_dec_i(ss, pn);
    ss_set_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_PREV =
    MAKE_GET_SET_OP(P.PREV, op_P_PREV_get, op_P_PREV_set, 0, true);
const tele_op_t op_PN_PREV =
    MAKE_GET_SET_OP(PN.PREV, op_PN_PREV_get, op_PN_PREV_set, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.INS ///////////////////////////////////////////////////////////////////////

// Get
static void p_ins_get(scene_state_t *ss, int16_t pn, int16_t idx, int16_t val) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    const int16_t len = ss_get_pattern_len(ss, pn);

    if (len >= idx) {
        for (int16_t i = len; i > idx; i--) {
            int16_t v = ss_get_pattern_val(ss, pn, i - 1);
            ss_set_pattern_val(ss, pn, i, v);
        }
        if (len < PATTERN_LENGTH - 1) { ss_set_pattern_len(ss, pn, len + 1); }
    }

    ss_set_pattern_val(ss, pn, idx, val);
}

static void op_P_INS_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_ins_get(ss, pn, a, b);

    tele_pattern_updated();
}

static void op_PN_INS_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_ins_get(ss, pn, a, b);

    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_INS = MAKE_GET_OP(P.INS, op_P_INS_get, 2, false);
const tele_op_t op_PN_INS = MAKE_GET_OP(PN.INS, op_PN_INS_get, 3, false);


////////////////////////////////////////////////////////////////////////////////
// P.RM ////////////////////////////////////////////////////////////////////////

// Get
static int16_t p_rm_get(scene_state_t *ss, int16_t pn, int16_t idx) {
    pn = normalise_pn(pn);
    const int16_t len = ss_get_pattern_len(ss, pn);

    if (len > 0) {
        idx = normalise_idx(ss, pn, idx);
        int16_t ret = ss_get_pattern_val(ss, pn, idx);

        if (idx < len) {
            for (int16_t i = idx; i < len; i++) {
                int16_t v = ss_get_pattern_val(ss, pn, i + 1);
                ss_set_pattern_val(ss, pn, i, v);
            }

            ss_set_pattern_len(ss, pn, len - 1);
        }

        return ret;
    }

    return 0;
}

static void op_P_RM_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    cs_push(cs, p_rm_get(ss, pn, a));
    tele_pattern_updated();
}

static void op_PN_RM_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    cs_push(cs, p_rm_get(ss, pn, a));
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_RM = MAKE_GET_OP(P.RM, op_P_RM_get, 1, true);
const tele_op_t op_PN_RM = MAKE_GET_OP(PN.RM, op_PN_RM_get, 2, true);


////////////////////////////////////////////////////////////////////////////////
// P.PUSH //////////////////////////////////////////////////////////////////////

// Get
static void p_push_get(scene_state_t *ss, int16_t pn, int16_t val) {
    pn = normalise_pn(pn);
    const int16_t len = ss_get_pattern_len(ss, pn);

    if (len < PATTERN_LENGTH) {
        ss_set_pattern_val(ss, pn, len, val);
        ss_set_pattern_len(ss, pn, len + 1);
    }

    tele_pattern_updated();
}

static void op_P_PUSH_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    p_push_get(ss, pn, a);
}

static void op_PN_PUSH_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    p_push_get(ss, pn, a);
}

// Make ops
const tele_op_t op_P_PUSH = MAKE_GET_OP(P.PUSH, op_P_PUSH_get, 1, false);
const tele_op_t op_PN_PUSH = MAKE_GET_OP(PN.PUSH, op_PN_PUSH_get, 2, false);

////////////////////////////////////////////////////////////////////////////////
// P.POP ///////////////////////////////////////////////////////////////////////

// Get
static int16_t p_pop_get(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);
    const int16_t len = ss_get_pattern_len(ss, pn);
    if (len > 0) {
        ss_set_pattern_len(ss, pn, len - 1);
        return ss_get_pattern_val(ss, pn, len - 1);
    }
    else
        return 0;
}

static void op_P_POP_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, p_pop_get(ss, ss->variables.p_n));
    tele_pattern_updated();
}

static void op_PN_POP_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    cs_push(cs, p_pop_get(ss, pn));
    tele_pattern_updated();
}

// Make ops
const tele_op_t op_P_POP = MAKE_GET_OP(P.POP, op_P_POP_get, 0, true);
const tele_op_t op_PN_POP = MAKE_GET_OP(PN.POP, op_PN_POP_get, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.MIN ///////////////////////////////////////////////////////////////////////

// Get
static int16_t p_min_get(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);

    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);

    int16_t pos = start;
    int16_t val = ss_get_pattern_val(ss, pn, pos);
    int16_t temp = 0;

    for (int16_t i = start + 1; i <= end; i++) {
        temp = ss_get_pattern_val(ss, pn, i);
        if (temp < val) {
            pos = i;
            val = temp;
        }
    }

    return pos;
}

static void op_P_MIN_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, p_min_get(ss, ss->variables.p_n));
}

static void op_PN_MIN_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    cs_push(cs, p_min_get(ss, pn));
}

// Make ops
const tele_op_t op_P_MIN = MAKE_GET_OP(P.MIN, op_P_MIN_get, 0, true);
const tele_op_t op_PN_MIN = MAKE_GET_OP(PN.MIN, op_PN_MIN_get, 1, true);


////////////////////////////////////////////////////////////////////////////////
// P.MAX ///////////////////////////////////////////////////////////////////////

// Get
static int16_t p_max_get(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);

    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);

    int16_t pos = start;
    int16_t val = ss_get_pattern_val(ss, pn, pos);
    int16_t temp = 0;

    for (int16_t i = start + 1; i <= end; i++) {
        temp = ss_get_pattern_val(ss, pn, i);
        if (temp > val) {
            pos = i;
            val = temp;
        }
    }

    return pos;
}

static void op_P_MAX_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, p_max_get(ss, ss->variables.p_n));
}

static void op_PN_MAX_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    cs_push(cs, p_max_get(ss, pn));
}

// Make ops
const tele_op_t op_P_MAX = MAKE_GET_OP(P.MAX, op_P_MAX_get, 0, true);
const tele_op_t op_PN_MAX = MAKE_GET_OP(PN.MAX, op_PN_MAX_get, 1, true);

////////////////////////////////////////////////////////////////////////////////
// P.SHUF, P.REV, P.ROT /////////////////////////////////////////////////

static void p_shuffle(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    int16_t draw, xchg;
    random_state_t *r = &ss->rand_states.s.pattern.rand;

    if (end < start) { return; }
    for (int16_t i = end; i > start; i--) {
        draw = (random_next(r) % (i - start + 1)) + start;
        xchg = ss_get_pattern_val(ss, pn, draw);
        ss_set_pattern_val(ss, pn, draw, ss_get_pattern_val(ss, pn, i));
        ss_set_pattern_val(ss, pn, i, xchg);
    }

    tele_pattern_updated();
}

static void op_P_SHUF_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es),
                          command_state_t *NOTUSED(cs)) {
    p_shuffle(ss, ss->variables.p_n);
}

static void op_PN_SHUF_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    p_shuffle(ss, cs_pop(cs));
}

const tele_op_t op_P_SHUF = MAKE_GET_OP(P.SHUF, op_P_SHUF_get, 0, false);
const tele_op_t op_PN_SHUF = MAKE_GET_OP(PN.SHUF, op_PN_SHUF_get, 1, false);

static void p_reverse(scene_state_t *ss, int16_t pn, int16_t start,
                      int16_t end) {
    pn = normalise_pn(pn);

    if (end < start) { return; }
    int16_t midpt = (end - start) / 2;
    int16_t xchg;
    for (int16_t i = 0; i <= midpt; i++) {
        xchg = ss_get_pattern_val(ss, pn, end - i);
        ss_set_pattern_val(ss, pn, end - i,
                           ss_get_pattern_val(ss, pn, start + i));
        ss_set_pattern_val(ss, pn, start + i, xchg);
    }

    tele_pattern_updated();
}

static void op_P_REV_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es),
                         command_state_t *NOTUSED(cs)) {
    int16_t pn = ss->variables.p_n;
    p_reverse(ss, pn, ss_get_pattern_start(ss, pn), ss_get_pattern_end(ss, pn));
}

static void op_PN_REV_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    p_reverse(ss, pn, ss_get_pattern_start(ss, pn), ss_get_pattern_end(ss, pn));
}

const tele_op_t op_P_REV = MAKE_GET_OP(P.REV, op_P_REV_get, 0, false);
const tele_op_t op_PN_REV = MAKE_GET_OP(PN.REV, op_PN_REV_get, 1, false);

static void p_rotate(scene_state_t *ss, int16_t pn, int16_t shift) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    if (end < start) { return; }
    int16_t len = end - start + 1;

    if (shift < 0) {
        shift = -shift;
        shift = shift % len;
        if (shift == 0) return;
        p_reverse(ss, pn, start, start + shift - 1);
        p_reverse(ss, pn, start + shift, end);
        p_reverse(ss, pn, start, end);
    }
    else {
        shift = shift % len;
        if (shift == 0) return;
        p_reverse(ss, pn, end - shift + 1, end);
        p_reverse(ss, pn, start, end - shift);
        p_reverse(ss, pn, start, end);
    }
}

static void op_P_ROT_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    p_rotate(ss, ss->variables.p_n, cs_pop(cs));
}

static void op_PN_ROT_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t rot = cs_pop(cs);
    p_rotate(ss, pn, rot);
}

const tele_op_t op_P_ROT = MAKE_GET_OP(P.ROT, op_P_ROT_get, 1, false);
const tele_op_t op_PN_ROT = MAKE_GET_OP(PN.ROT, op_PN_ROT_get, 2, false);

static void p_cycle(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    int16_t len = ss_get_pattern_len(ss, pn);

    if (end < start) { return; }
    for (int16_t i = start; i <= end; i++) {
        ss_set_pattern_val(ss, pn, i,
                           ss_get_pattern_val(ss, pn, (i - start) % len));
    }

    tele_pattern_updated();
}

static void op_P_CYC_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es),
                         command_state_t *NOTUSED(cs)) {
    p_cycle(ss, ss->variables.p_n);
}

static void op_PN_CYC_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    p_cycle(ss, cs_pop(cs));
}

const tele_op_t op_P_CYC = MAKE_GET_OP(P.CYC, op_P_CYC_get, 0, false);
const tele_op_t op_PN_CYC = MAKE_GET_OP(PN.CYC, op_PN_CYC_get, 1, false);


////////////////////////////////////////////////////////////////////////////////
// P.RND ///////////////////////////////////////////////////////////////////////

static int16_t p_rnd_get(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    random_state_t *r = &ss->rand_states.s.pattern.rand;

    if (end < start) return 0;

    return ss_get_pattern_val(ss, pn,
                              random_next(r) % (end - start + 1) + start);
}

static void op_P_RND_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, p_rnd_get(ss, ss->variables.p_n));
}

static void op_PN_RND_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    cs_push(cs, p_rnd_get(ss, pn));
}

// Make ops
const tele_op_t op_P_RND = MAKE_GET_OP(P.RND, op_P_RND_get, 0, true);
const tele_op_t op_PN_RND = MAKE_GET_OP(PN.RND, op_PN_RND_get, 1, true);

////////////////////////////////////////////////////////////////////////////////
// P.MOTIF /////////////////////////////////////////////////////////////////////

// Weighted step generator for motif construction. Returns a signed scale-degree
// step in [-3, +3]. Magnitudes are drawn at weights 50/25/25 for 1/2/3
// (collapsing the upper half of the wider melody distribution into 3, since
// short motifs feel more coherent with small internal intervals). After a leap
// (|prev_step| >= 3) the next step has magnitude 1-2 and opposite direction.
static int16_t motif_step(random_state_t *r, int16_t prev_step) {
    if (prev_step >= 3 || prev_step <= -3) {
        int16_t mag = 1 + (int16_t)(random_next(r) % 2);
        int16_t sign = (prev_step > 0) ? -1 : 1;
        return sign * mag;
    }

    uint32_t roll = random_next(r) % 100;
    int16_t mag;
    if (roll < 50)
        mag = 1;
    else if (roll < 75)
        mag = 2;
    else
        mag = 3;

    int16_t sign = (random_next(r) & 1) ? 1 : -1;
    return sign * mag;
}

// Fill the working pattern's [start..end] window with a motif and its
// variations. Output values are diatonic scale degrees (1..7 = one octave, 0
// and negatives below). length: 2-4 (clamped). variation: 0-7 (wrapped).
// transposition: signed degree shift applied to each successive statement.
static void p_motif(scene_state_t *ss, int16_t pn, int16_t length,
                    int16_t variation, int16_t transposition) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    if (end < start) return;

    if (length < 2) length = 2;
    if (length > 4) length = 4;
    variation = ((variation % 8) + 8) % 8;

    random_state_t *r = &ss->rand_states.s.pattern.rand;

    int16_t motif[4];
    motif[0] = 0;
    int16_t prev_step = 0;
    for (int16_t n = 1; n < length; n++) {
        int16_t step = motif_step(r, prev_step);
        motif[n] = motif[n - 1] + step;
        prev_step = step;
    }

    int16_t L = end - start + 1;
    int16_t num_full = L / length;
    int16_t leftover = L % length;
    int16_t total_statements = num_full + (leftover > 0 ? 1 : 0);

    int16_t out[4];
    for (int16_t s = 0; s < total_statements; s++) {
        int32_t tpose = (int32_t)s * (int32_t)transposition;
        int16_t mode = (s == 0) ? 0 : variation;

        switch (mode) {
            case 1:  // INVERT
                for (int16_t n = 0; n < length; n++) out[n] = -motif[n];
                break;
            case 2:  // RETROGRADE
                for (int16_t n = 0; n < length; n++)
                    out[n] = motif[length - 1 - n];
                break;
            case 3:  // ALTERNATE
                if (s & 1) {
                    for (int16_t n = 0; n < length; n++) out[n] = -motif[n];
                }
                else {
                    for (int16_t n = 0; n < length; n++) out[n] = motif[n];
                }
                break;
            case 4: {  // ORNAMENT
                for (int16_t n = 0; n < length; n++) out[n] = motif[n];
                int16_t nudge_pos =
                    (int16_t)(random_next(r) % (uint32_t)length);
                int16_t nudge_dir = (random_next(r) & 1) ? 1 : -1;
                out[nudge_pos] += nudge_dir;
                break;
            }
            case 5:  // COMPRESS
                for (int16_t n = 0; n < length; n++) out[n] = motif[n] / 2;
                break;
            case 6:  // EXPAND
                for (int16_t n = 0; n < length; n++) out[n] = motif[n] * 2;
                break;
            case 7:  // ROTATE
                for (int16_t n = 0; n < length; n++)
                    out[n] = motif[(n + s) % length];
                break;
            case 0:  // EXACT
            default:
                for (int16_t n = 0; n < length; n++) out[n] = motif[n];
                break;
        }

        for (int16_t n = 0; n < length; n++) {
            int16_t pos = (int16_t)(s * length + n);
            if (pos >= L) break;
            int32_t value = (int32_t)1 + tpose + (int32_t)out[n];
            if (value > INT16_MAX) value = INT16_MAX;
            if (value < INT16_MIN) value = INT16_MIN;
            ss_set_pattern_val(ss, pn, start + pos, (int16_t)value);
        }
    }

    tele_pattern_updated();
}

static void op_P_MOTIF_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t length = cs_pop(cs);
    int16_t variation = cs_pop(cs);
    int16_t transposition = cs_pop(cs);
    p_motif(ss, ss->variables.p_n, length, variation, transposition);
}

static void op_PN_MOTIF_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t length = cs_pop(cs);
    int16_t variation = cs_pop(cs);
    int16_t transposition = cs_pop(cs);
    p_motif(ss, pn, length, variation, transposition);
}

const tele_op_t op_P_MOTIF = MAKE_GET_OP(P.MOTIF, op_P_MOTIF_get, 3, false);
const tele_op_t op_PN_MOTIF = MAKE_GET_OP(PN.MOTIF, op_PN_MOTIF_get, 4, false);

////////////////////////////////////////////////////////////////////////////////
// P.CP / PN.CP — diatonic two-voice counterpoint reader //////////////////////
//
// Stateless read-only op: returns a diatonic degree forming counterpoint
// against pattern[P.I] using the full START..END window for context.
// Input and output are diatonic scale degrees (1..7 = one octave), to pair
// with P.MOTIF and N.S / QT.S.

typedef struct {
    int16_t vmin, vmax;
    int16_t peak_index, valley_index;
    int8_t global_direction;
    int8_t ends_on_tonic;
} cp_window_analysis_t;

static int16_t cp_clamp_i16(int32_t v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static void cp_analyse_window(scene_state_t *ss, int16_t pn, int16_t start,
                              int16_t end, cp_window_analysis_t *w) {
    int16_t first = ss_get_pattern_val(ss, pn, start);
    int16_t last = ss_get_pattern_val(ss, pn, end);
    w->vmin = first;
    w->vmax = first;
    w->peak_index = start;
    w->valley_index = start;
    for (int16_t i = start; i <= end; i++) {
        int16_t v = ss_get_pattern_val(ss, pn, i);
        if (v > w->vmax) {
            w->vmax = v;
            w->peak_index = i;
        }
        if (v < w->vmin) {
            w->vmin = v;
            w->valley_index = i;
        }
    }
    if (last > first)
        w->global_direction = 1;
    else if (last < first)
        w->global_direction = -1;
    else
        w->global_direction = 0;
    int16_t r = ((last - 1) % 7 + 7) % 7;
    w->ends_on_tonic = (r == 0) ? 1 : 0;
}

static int16_t cp_compute(scene_state_t *ss, int16_t pn, int16_t rule,
                          int16_t offset) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    int16_t cur_idx = ss_get_pattern_idx(ss, pn);

    if (end < start) {
        return cp_clamp_i16((int32_t)ss_get_pattern_val(ss, pn, cur_idx) +
                            offset);
    }
    if (end == start) {
        return cp_clamp_i16((int32_t)ss_get_pattern_val(ss, pn, start) +
                            offset);
    }

    int16_t L = end - start + 1;
    int16_t i = cur_idx;
    if (i < start || i > end) i = ((i - start) % L + L) % L + start;

    rule = ((rule % 8) + 8) % 8;

    int16_t cf_now = ss_get_pattern_val(ss, pn, i);

    // CANON: bypass scoring, read from earlier in the window.
    if (rule == 6) {
        int16_t delay = (L >= 4) ? L / 4 : 1;
        int16_t read_pos = ((i - start - delay) % L + L) % L + start;
        return cp_clamp_i16((int32_t)ss_get_pattern_val(ss, pn, read_pos) +
                            offset);
    }

    cp_window_analysis_t w;
    cp_analyse_window(ss, pn, start, end, &w);

    int16_t cf_prev = (i > start) ? ss_get_pattern_val(ss, pn, i - 1)
                                  : ss_get_pattern_val(ss, pn, end);
    int local_motion = (int)cf_now - (int)cf_prev;
    int position_pct = ((i - start) * 100) / (L - 1);
    int near_end = (position_pct > 80) ? 1 : 0;

    // 3rds, 6ths, 5ths, octaves — consonant intervals in diatonic writing.
    static const int8_t intervals[] = { 2, -2, 5, -5, 4, -4, 7, -7 };

    int32_t target = (int32_t)cf_now + (int32_t)offset;
    int32_t best = target + 2;
    int best_score = -10000;

    int axis = (((int)w.vmin + (int)w.vmax) / 2) + (int)offset;
    // Floor-divide cf_now-1 by 7 (C truncates toward zero, which would
    // misplace tonic_octave for cf_now <= 0).
    int t = (int)cf_now - 1;
    int oct = (t >= 0) ? t / 7 : -(((-t) + 6) / 7);
    int tonic_octave = oct * 7 + 1 + (int)offset;

    for (int c = 0; c < 8; c++) {
        int cp_motion = intervals[c];
        int32_t candidate = target + cp_motion;
        int score = 10;

        if ((local_motion > 0 && cp_motion < 0) ||
            (local_motion < 0 && cp_motion > 0))
            score += 15;

        int total_range = (int)w.vmax - (int)w.vmin;
        if (total_range > 0) {
            int melody_pos_in_range = (int)cf_now - (int)w.vmin;
            if (melody_pos_in_range < total_range / 3 && cp_motion > 0)
                score += 10;
            if (melody_pos_in_range > 2 * total_range / 3 && cp_motion < 0)
                score += 10;
        }

        if (near_end && w.ends_on_tonic) {
            if (cp_motion == 2 || cp_motion == 4 || cp_motion == 7) score += 15;
        }

        switch (rule) {
            case 0:  // BALANCED
                break;
            case 1:  // CONTRARY
                if ((local_motion > 0 && cp_motion < 0) ||
                    (local_motion < 0 && cp_motion > 0))
                    score += 25;
                else if (local_motion != 0)
                    score -= 20;
                if (w.global_direction > 0 && cp_motion < 0) score += 10;
                if (w.global_direction < 0 && cp_motion > 0) score += 10;
                break;
            case 2:  // PARALLEL — favour 3rds and 6ths above
                if (cp_motion == 2 || cp_motion == 5) score += 30;
                break;
            case 3:  // BASS — stay below, prefer 5ths and octaves
                if (cp_motion > 0) score -= 100;
                if (cp_motion == -4 || cp_motion == -7) score += 20;
                break;
            case 4: {  // MIRROR — invert around axis (axis shifted by offset)
                int mirrored = 2 * axis - (int)cf_now;
                int diff = (int)candidate - mirrored;
                if (diff < 0) diff = -diff;
                if (diff <= 1) score += 50;
                break;
            }
            case 5: {  // OBLIQUE — pull toward tonic-octave
                int dist = (int)candidate - tonic_octave;
                if (dist < 0) dist = -dist;
                score += (20 - dist * 5);
                break;
            }
            case 7:  // FOLLOW — same direction as melody
                if ((local_motion > 0 && cp_motion > 0) ||
                    (local_motion < 0 && cp_motion < 0))
                    score += 25;
                break;
            default: break;
        }

        if (score > best_score) {
            best_score = score;
            best = candidate;
        }
    }

    return cp_clamp_i16(best);
}

static void op_P_CP_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t rule = cs_pop(cs);
    int16_t offset = cs_pop(cs);
    cs_push(cs, cp_compute(ss, ss->variables.p_n, rule, offset));
}

static void op_PN_CP_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t rule = cs_pop(cs);
    int16_t offset = cs_pop(cs);
    cs_push(cs, cp_compute(ss, pn, rule, offset));
}

const tele_op_t op_P_CP = MAKE_GET_OP(P.CP, op_P_CP_get, 2, true);
const tele_op_t op_PN_CP = MAKE_GET_OP(PN.CP, op_PN_CP_get, 3, true);

////////////////////////////////////////////////////////////////////////////////
// P.FUGUE / PN.FUGUE — fugal voice reader //////////////////////////////////////
//
// Reads the pattern window [start..end] as a fugal subject. Returns one
// diatonic scale degree based on a caller-supplied master clock.
//
// Mode is coerced to PRIME for values outside 0..3 (so mode=4 is *not*
// "RETROGRADE-INVERSION+1"). Note that clock advances via C truncation-
// toward-zero division, so plateaus around clock=0 are asymmetric: with
// division=2, clocks {-1,0,1} all map to the same subject position. This
// matters only when sweeping clock through zero (e.g. a bipolar LFO).
//
// The first arg `voice` opts into clash avoidance. voice=0 (the default for
// scripts that don't care) is fully stateless and identical to v1 behaviour.
// voice=1..4 records its final value in a small per-bank state table; voices
// 2..4 inspect lower-numbered voices that recorded a value at the *same
// clock* (any other clock value is treated as stale and ignored) and adjust
// away from 2nd/7th intervals (|diff| mod 7 ∈ {1, 6}). Adjustment is
// bidirectional — push up when above the other voice, down when below —
// which lands on the stronger consonance (3rd or octave) and avoids the
// upward drift "always +1" would cause. Unisons and octaves are allowed.
// Voices are expected to be called in numerical order within a tick; calling
// out of order produces unreliable avoidance but is not catastrophic. The
// state table is reset whenever ss_init runs (scene load, INIT op,
// firmware boot), so it can never leak voice slots from a prior scene.
//
// After avoidance, the candidate is octave-shifted (±7) until it sits
// within ±14 diatonic degrees of the lowest currently-tracked lower voice.
// This caps inter-voice register separation at two octaves to prevent
// unbounded drift from cascading adjustments. The clamp takes precedence
// over avoidance: a shifted value that lands on a 2nd or 7th is accepted.

#define FUGUE_VOICE_COUNT 5 /* slots 0..4; slot 0 unused (voices are 1..4) */

typedef struct {
    int32_t last_clock;
    int16_t note;
    uint8_t valid;
} fugue_voice_slot_t;

static fugue_voice_slot_t fugue_voice_state[PATTERN_COUNT][FUGUE_VOICE_COUNT];

void fugue_voice_state_reset(void) {
    for (size_t b = 0; b < PATTERN_COUNT; b++) {
        for (size_t v = 0; v < FUGUE_VOICE_COUNT; v++) {
            fugue_voice_state[b][v].last_clock = 0;
            fugue_voice_state[b][v].note = 0;
            fugue_voice_state[b][v].valid = 0;
        }
    }
}

static int16_t fugue_candidate(scene_state_t *ss, int16_t pn, int16_t division,
                               int16_t transpose, int16_t mode, int16_t phase,
                               int16_t clock) {
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    int subject_len = (int)end - (int)start + 1;

    if (mode < 0 || mode > 3) mode = 0;

    int abs_div = (division < 0) ? -(int)division : (int)division;
    int reverse =
        ((division < 0) ? 1 : 0) ^ ((mode == 2 || mode == 3) ? 1 : 0);

    int note_index = ((int)clock / abs_div) + (int)phase;
    int pos = ((note_index % subject_len) + subject_len) % subject_len;
    if (reverse) pos = (subject_len - 1) - pos;

    int32_t note = ss_get_pattern_val(ss, pn, start + pos);
    if (mode == 1 || mode == 3) {
        int32_t first = ss_get_pattern_val(ss, pn, start);
        note = 2 * first - note;
    }
    note += transpose;
    return cp_clamp_i16(note);
}

static int16_t fugue_read(scene_state_t *ss, int16_t pn, int16_t voice,
                          int16_t division, int16_t transpose, int16_t mode,
                          int16_t phase, int16_t clock) {
    pn = normalise_pn(pn);

    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    if (((int)end - (int)start + 1) < 1 || division == 0) return 0;

    int16_t candidate =
        fugue_candidate(ss, pn, division, transpose, mode, phase, clock);

    if (voice < 0) voice = 0;
    if (voice > 4) voice = 4;

    if (voice == 0) return candidate;

    if (voice >= 2) {
        int safety = 8;
        int adjusted = 1;
        while (adjusted && safety-- > 0) {
            adjusted = 0;
            for (int v = 1; v < voice; v++) {
                fugue_voice_slot_t *slot = &fugue_voice_state[pn][v];
                if (!slot->valid || slot->last_clock != (int32_t)clock)
                    continue;
                int diff = (int)candidate - (int)slot->note;
                if (diff == 0) continue;
                int abs_mod7 = (diff > 0 ? diff : -diff) % 7;
                if (abs_mod7 == 1 || abs_mod7 == 6) {
                    candidate = cp_clamp_i16((int32_t)candidate +
                                             (diff > 0 ? 1 : -1));
                    adjusted = 1;
                    break;
                }
            }
        }
    }

    // Octave clamp: shift the post-avoidance candidate down by 7 until it
    // sits within +14 of the lowest currently-tracked lower voice. Caps
    // inter-voice register spread at two octaves above the lowest voice
    // to prevent unbounded drift from cascading adjustments. Takes
    // precedence over avoidance: a shifted value that lands on a 2nd or
    // 7th is accepted as-is.
    //
    // Note: `lowest` is initialised to `candidate` so that with no valid
    // lower voices the bound trivially admits the candidate. Because
    // `lowest <= candidate` always under this init, the candidate can
    // never be below `lowest - 14`; only the upper branch can fire.
    {
        int32_t lowest = candidate;
        for (int v = 1; v < voice; v++) {
            fugue_voice_slot_t *slot = &fugue_voice_state[pn][v];
            if (!slot->valid || slot->last_clock != (int32_t)clock) continue;
            if (slot->note < lowest) lowest = slot->note;
        }
        int32_t max_allowed = lowest + 14;
        int32_t c = candidate;
        while (c > max_allowed) c -= 7;
        candidate = cp_clamp_i16(c);
    }

    fugue_voice_state[pn][voice].note = candidate;
    fugue_voice_state[pn][voice].last_clock = (int32_t)clock;
    fugue_voice_state[pn][voice].valid = 1;
    return candidate;
}

static void op_P_FUGUE_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t voice = cs_pop(cs);
    int16_t division = cs_pop(cs);
    int16_t transpose = cs_pop(cs);
    int16_t mode = cs_pop(cs);
    int16_t phase = cs_pop(cs);
    int16_t clock = cs_pop(cs);
    cs_push(cs, fugue_read(ss, ss->variables.p_n, voice, division, transpose,
                           mode, phase, clock));
}

static void op_PN_FUGUE_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t voice = cs_pop(cs);
    int16_t division = cs_pop(cs);
    int16_t transpose = cs_pop(cs);
    int16_t mode = cs_pop(cs);
    int16_t phase = cs_pop(cs);
    int16_t clock = cs_pop(cs);
    cs_push(cs, fugue_read(ss, pn, voice, division, transpose, mode, phase,
                           clock));
}

const tele_op_t op_P_FUGUE = MAKE_GET_OP(P.FUGUE, op_P_FUGUE_get, 6, true);
const tele_op_t op_PN_FUGUE = MAKE_GET_OP(PN.FUGUE, op_PN_FUGUE_get, 7, true);

////////////////////////////////////////////////////////////////////////////////
// P.+ P.+W ////////////////////////////////////////////////////////////////////

static void p_add_get(scene_state_t *ss, int16_t pn, int16_t idx, int16_t delta,
                      uint8_t wrap_value, int16_t min, int16_t max) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    int16_t value = ss_get_pattern_val(ss, pn, idx) + delta;
    if (wrap_value) value = wrap(value, min, max);
    ss_set_pattern_val(ss, pn, idx, value);
}

static void op_P_ADD_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    p_add_get(ss, ss->variables.p_n, idx, delta, 0, 0, 0);
    tele_pattern_updated();
}

static void op_PN_ADD_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    p_add_get(ss, pn, idx, delta, 0, 0, 0);
    tele_pattern_updated();
}

static void op_P_ADDW_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    p_add_get(ss, ss->variables.p_n, idx, delta, 1, min, max);
    tele_pattern_updated();
}

static void op_PN_ADDW_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    p_add_get(ss, pn, idx, delta, 1, min, max);
    tele_pattern_updated();
}

// Make ops
// clang-format off
const tele_op_t op_P_ADD = MAKE_GET_OP(P.+, op_P_ADD_get, 2, false);
const tele_op_t op_PN_ADD = MAKE_GET_OP(PN.+, op_PN_ADD_get, 3, false);
const tele_op_t op_P_ADDW = MAKE_GET_OP(P.+W, op_P_ADDW_get, 4, false);
const tele_op_t op_PN_ADDW = MAKE_GET_OP(PN.+W, op_PN_ADDW_get, 5, false);
// clang-format on

////////////////////////////////////////////////////////////////////////////////
// P.- P.-W ////////////////////////////////////////////////////////////////////

static void p_sub_get(scene_state_t *ss, int16_t pn, int16_t idx, int16_t delta,
                      uint8_t wrap_value, int16_t min, int16_t max) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    int16_t value = ss_get_pattern_val(ss, pn, idx) - delta;
    if (wrap_value) value = wrap(value, min, max);
    ss_set_pattern_val(ss, pn, idx, value);
}

static void op_P_SUB_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    p_sub_get(ss, ss->variables.p_n, idx, delta, 0, 0, 0);
    tele_pattern_updated();
}

static void op_PN_SUB_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    p_sub_get(ss, pn, idx, delta, 0, 0, 0);
    tele_pattern_updated();
}

static void op_P_SUBW_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    p_sub_get(ss, ss->variables.p_n, idx, delta, 1, min, max);
    tele_pattern_updated();
}

static void op_PN_SUBW_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t delta = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    p_sub_get(ss, pn, idx, delta, 1, min, max);
    tele_pattern_updated();
}

// Make ops
// clang-format off
const tele_op_t op_P_SUB = MAKE_GET_OP(P.-, op_P_SUB_get, 2, false);
const tele_op_t op_PN_SUB = MAKE_GET_OP(PN.-, op_PN_SUB_get, 3, false);
const tele_op_t op_P_SUBW = MAKE_GET_OP(P.-W, op_P_SUBW_get, 4, false);
const tele_op_t op_PN_SUBW = MAKE_GET_OP(PN.-W, op_PN_SUBW_get, 5, false);
// clang-format on


////////////////////////////////////////////////////////////////////////////////
// P.D, PN.D, P.D.HERE, PN.D.HERE, P.D.RND, PN.D.RND ///////////////////////////
// Per-cell dwell duration (ticks) used by P.STEP. Mirrors the val[]
// accessor family (P, P.HERE) plus an in-place randomiser.

static int16_t p_d_get(scene_state_t *ss, int16_t pn, int16_t idx) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    return ss_get_pattern_dur(ss, pn, idx);
}

static void p_d_set(scene_state_t *ss, int16_t pn, int16_t idx, int16_t dur) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);
    // ss_set_pattern_dur clamps dur < 1 to 1.
    ss_set_pattern_dur(ss, pn, idx, dur);
    tele_pattern_updated();
}

static void op_P_D_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    cs_push(cs, p_d_get(ss, pn, a));
}

static void op_PN_D_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    cs_push(cs, p_d_get(ss, pn, a));
}

static void op_P_D_set(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_d_set(ss, pn, a, b);
}

static void op_PN_D_set(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    p_d_set(ss, pn, a, b);
}

static void op_P_D_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_dur(ss, pn, ss_get_pattern_idx(ss, pn)));
}

static void op_PN_D_HERE_get(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_dur(ss, pn, ss_get_pattern_idx(ss, pn)));
}

static void op_P_D_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    ss_set_pattern_dur(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

static void op_PN_D_HERE_set(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    ss_set_pattern_dur(ss, pn, ss_get_pattern_idx(ss, pn), a);
    tele_pattern_updated();
}

// Randomise dur[] for every cell in [start..end]. lo/hi are clamped >= 1
// and swapped if lo > hi, so a freeze-the-sequencer 0 is impossible.
static void p_d_rnd(scene_state_t *ss, int16_t pn, int16_t lo, int16_t hi) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    if (end < start) return;

    if (lo < 1) lo = 1;
    if (hi < 1) hi = 1;
    if (lo > hi) {
        int16_t t = lo;
        lo = hi;
        hi = t;
    }

    random_state_t *r = &ss->rand_states.s.pattern.rand;
    int16_t range = hi - lo + 1;
    for (int16_t i = start; i <= end; i++) {
        int16_t v = (int16_t)(random_next(r) % range) + lo;
        ss_set_pattern_dur(ss, pn, i, v);
    }
    tele_pattern_updated();
}

static void op_P_D_RND_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t hi = cs_pop(cs);
    int16_t lo = cs_pop(cs);
    p_d_rnd(ss, ss->variables.p_n, lo, hi);
}

static void op_PN_D_RND_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t hi = cs_pop(cs);
    int16_t lo = cs_pop(cs);
    p_d_rnd(ss, pn, lo, hi);
}

// clang-format off
const tele_op_t op_P_D        = MAKE_GET_SET_OP(P.D,        op_P_D_get,        op_P_D_set,        1, true);
const tele_op_t op_PN_D       = MAKE_GET_SET_OP(PN.D,       op_PN_D_get,       op_PN_D_set,       2, true);
const tele_op_t op_P_D_HERE   = MAKE_GET_SET_OP(P.D.HERE,   op_P_D_HERE_get,   op_P_D_HERE_set,   0, true);
const tele_op_t op_PN_D_HERE  = MAKE_GET_SET_OP(PN.D.HERE,  op_PN_D_HERE_get,  op_PN_D_HERE_set,  1, true);
const tele_op_t op_P_D_RND    = MAKE_GET_OP(P.D.RND,        op_P_D_RND_get,    2, false);
const tele_op_t op_PN_D_RND   = MAKE_GET_OP(PN.D.RND,       op_PN_D_RND_get,   3, false);
// clang-format on


////////////////////////////////////////////////////////////////////////////////
// P.MODE, PN.MODE, P.DIR, PN.DIR, P.STRIDE, PN.STRIDE /////////////////////////
// Per-pattern playback-mode configuration consumed by p_mode_advance.
// These ops are pure config — they never advance the playhead.

static void op_P_MODE_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_mode(ss, pn));
}

static void op_PN_MODE_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_mode(ss, pn));
}

static void op_P_MODE_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    if (a < 0) a = 0;
    if (a >= PATTERN_MODE_COUNT) a = PATTERN_MODE_COUNT - 1;
    ss_set_pattern_mode(ss, pn, (uint8_t)a);
    tele_pattern_updated();
}

static void op_PN_MODE_set(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    if (a < 0) a = 0;
    if (a >= PATTERN_MODE_COUNT) a = PATTERN_MODE_COUNT - 1;
    ss_set_pattern_mode(ss, pn, (uint8_t)a);
    tele_pattern_updated();
}

static void op_P_DIR_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_dir(ss, pn));
}

static void op_PN_DIR_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_dir(ss, pn));
}

static void op_P_DIR_set(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    ss_set_pattern_dir(ss, pn, a ? 1 : 0);
    // Re-seed travel direction so a mid-bounce flip starts the next
    // P.STEP travelling in the new base direction.
    ss->p_travel_dir[pn] = a ? -1 : 1;
    tele_pattern_updated();
}

static void op_PN_DIR_set(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    ss_set_pattern_dir(ss, pn, a ? 1 : 0);
    ss->p_travel_dir[pn] = a ? -1 : 1;
    tele_pattern_updated();
}

static void op_P_STRIDE_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss_get_pattern_stride(ss, pn));
}

static void op_PN_STRIDE_get(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss_get_pattern_stride(ss, pn));
}

static void op_P_STRIDE_set(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    int16_t a = cs_pop(cs);
    if (a < 1) a = 1;
    if (a > INT8_MAX) a = INT8_MAX;
    ss_set_pattern_stride(ss, pn, (int8_t)a);
    tele_pattern_updated();
}

static void op_PN_STRIDE_set(const void *NOTUSED(data), scene_state_t *ss,
                             exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    int16_t a = cs_pop(cs);
    if (a < 1) a = 1;
    if (a > INT8_MAX) a = INT8_MAX;
    ss_set_pattern_stride(ss, pn, (int8_t)a);
    tele_pattern_updated();
}

// clang-format off
const tele_op_t op_P_MODE     = MAKE_GET_SET_OP(P.MODE,     op_P_MODE_get,     op_P_MODE_set,     0, true);
const tele_op_t op_PN_MODE    = MAKE_GET_SET_OP(PN.MODE,    op_PN_MODE_get,    op_PN_MODE_set,    1, true);
const tele_op_t op_P_DIR      = MAKE_GET_SET_OP(P.DIR,      op_P_DIR_get,      op_P_DIR_set,      0, true);
const tele_op_t op_PN_DIR     = MAKE_GET_SET_OP(PN.DIR,     op_PN_DIR_get,     op_PN_DIR_set,     1, true);
const tele_op_t op_P_STRIDE   = MAKE_GET_SET_OP(P.STRIDE,   op_P_STRIDE_get,   op_P_STRIDE_set,   0, true);
const tele_op_t op_PN_STRIDE  = MAKE_GET_SET_OP(PN.STRIDE,  op_PN_STRIDE_get,  op_PN_STRIDE_set,  1, true);
// clang-format on


////////////////////////////////////////////////////////////////////////////////
// p_mode_advance //////////////////////////////////////////////////////////////
// Dispatches the next-cell choice for P.STEP based on the per-pattern mode
// (LINEAR / PINGPONG / PENDULUM / JUMP / RANDOM / BROWNIAN) and base
// direction. PINGPONG/PENDULUM/BROWNIAN consult and update
// ss->p_travel_dir[pn]; RANDOM/BROWNIAN draw from the shared pattern RNG.

static void p_mode_advance(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);

    const uint8_t mode = ss_get_pattern_mode(ss, pn);
    const uint8_t base_dir = ss_get_pattern_dir(ss, pn);
    const int16_t start = ss_get_pattern_start(ss, pn);
    const int16_t end_raw = ss_get_pattern_end(ss, pn);
    const int16_t len = ss_get_pattern_len(ss, pn);

    // Clamp end into [start..len-1] so all modes share one safe range.
    int16_t end = end_raw;
    if (len > 0 && end >= len) end = len - 1;
    if (end < start) {
        // Degenerate range: leave idx where it is.
        return;
    }
    const int16_t range = end - start + 1;
    int16_t idx = ss_get_pattern_idx(ss, pn);
    if (idx < start || idx > end) idx = start;

    switch (mode) {
        case PATTERN_MODE_LINEAR: {
            if (base_dir)
                p_prev_dec_i(ss, pn);
            else
                p_next_inc_i(ss, pn);
            return;
        }

        case PATTERN_MODE_PINGPONG: {
            // Endpoints repeat: on reaching an end, stay one tick and
            // reverse travel direction.
            int16_t dir = ss->p_travel_dir[pn];
            if (dir != 1 && dir != -1) dir = base_dir ? -1 : 1;
            if (range == 1) {
                idx = start;
                ss->p_travel_dir[pn] = dir;
                ss_set_pattern_idx(ss, pn, idx);
                return;
            }
            if (dir > 0 && idx >= end) {
                dir = -1;  // hold idx == end, reverse
            }
            else if (dir < 0 && idx <= start) {
                dir = 1;  // hold idx == start, reverse
            }
            else {
                idx += dir;
            }
            ss->p_travel_dir[pn] = dir;
            ss_set_pattern_idx(ss, pn, idx);
            return;
        }

        case PATTERN_MODE_PENDULUM: {
            // Endpoints play once: on reaching an end, flip and step in
            // the new direction immediately. range==1 has nowhere to go.
            int16_t dir = ss->p_travel_dir[pn];
            if (dir != 1 && dir != -1) dir = base_dir ? -1 : 1;
            if (range == 1) {
                idx = start;
                ss->p_travel_dir[pn] = dir;
                ss_set_pattern_idx(ss, pn, idx);
                return;
            }
            if (dir > 0 && idx >= end) {
                dir = -1;
                idx = end - 1;
            }
            else if (dir < 0 && idx <= start) {
                dir = 1;
                idx = start + 1;
            }
            else {
                idx += dir;
            }
            ss->p_travel_dir[pn] = dir;
            ss_set_pattern_idx(ss, pn, idx);
            return;
        }

        case PATTERN_MODE_JUMP: {
            int8_t stride = ss_get_pattern_stride(ss, pn);
            if (stride < 1) stride = 1;
            int16_t step = base_dir ? -(int16_t)stride : (int16_t)stride;
            // Modular add within [start..end].
            int16_t offset = (int16_t)(idx - start + step);
            offset %= range;
            if (offset < 0) offset += range;
            idx = (int16_t)(start + offset);
            ss_set_pattern_idx(ss, pn, idx);
            return;
        }

        case PATTERN_MODE_RANDOM: {
            random_state_t *r = &ss->rand_states.s.pattern.rand;
            idx = (int16_t)(random_next(r) % (uint32_t)range) + start;
            ss_set_pattern_idx(ss, pn, idx);
            return;
        }

        case PATTERN_MODE_BROWNIAN: {
            // Drunken walk: 50% +dir, 25% stay, 25% -dir. Clamps at the
            // endpoints rather than wrapping — a step off either end is
            // suppressed so the walk has a natural restoring force at
            // [start, end] (matches Metropolix-style brownian).
            int16_t dir = ss->p_travel_dir[pn];
            if (dir != 1 && dir != -1) dir = base_dir ? -1 : 1;
            random_state_t *r = &ss->rand_states.s.pattern.rand;
            uint32_t roll = random_next(r) & 0x3;
            int16_t step;
            if (roll == 0)
                step = 0;
            else if (roll == 1)
                step = -dir;
            else
                step = dir;  // rolls 2 and 3 both move +dir (50%)
            int16_t next = idx + step;
            if (next < start) next = start;
            if (next > end) next = end;
            ss->p_travel_dir[pn] = dir;
            ss_set_pattern_idx(ss, pn, next);
            return;
        }

        default:
            // Unknown mode — fall back to LINEAR FWD.
            p_next_inc_i(ss, pn);
            return;
    }
}


////////////////////////////////////////////////////////////////////////////////
// P.STEP, PN.STEP, P.STEP.NEW, PN.STEP.NEW ////////////////////////////////////
// Duration-aware playhead advance. P.STEP bumps an internal dwell counter
// and only advances idx (via p_mode_advance) when dwell reaches the
// current cell's dur[]. P.STEP.NEW returns 1 on the tick a stage just
// began — including the first STEP after init or a P.I reset.

// Advance Saga by one tick. Returns the val[] at the current idx after
// the step. Sets p_just_advanced[pn] = 1 iff idx just moved (or this is
// the first tick of a fresh stage after init/reset).
static int16_t p_step(scene_state_t *ss, int16_t pn) {
    pn = normalise_pn(pn);
    const int16_t dur = ss_get_pattern_dur(ss, pn, ss_get_pattern_idx(ss, pn));

    if (ss->p_dwell[pn] == 0) {
        // entering a stage fresh (after init or a P.I write).
        ss->p_dwell[pn] = 1;
        ss->p_just_advanced[pn] = 1;
    }
    else if (ss->p_dwell[pn] + 1 > dur) {
        // current stage is over — advance to the next cell.
        p_mode_advance(ss, pn);
        ss->p_dwell[pn] = 1;
        ss->p_just_advanced[pn] = 1;
    }
    else {
        ss->p_dwell[pn] = ss->p_dwell[pn] + 1;
        ss->p_just_advanced[pn] = 0;
    }
    return ss_get_pattern_val(ss, pn, ss_get_pattern_idx(ss, pn));
}

static void op_P_STEP_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, p_step(ss, ss->variables.p_n));
    tele_pattern_updated();
}

static void op_PN_STEP_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    cs_push(cs, p_step(ss, pn));
    tele_pattern_updated();
}

static void op_P_STEP_NEW_get(const void *NOTUSED(data), scene_state_t *ss,
                              exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    cs_push(cs, ss->p_just_advanced[pn] ? 1 : 0);
}

static void op_PN_STEP_NEW_get(const void *NOTUSED(data), scene_state_t *ss,
                               exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    cs_push(cs, ss->p_just_advanced[pn] ? 1 : 0);
}

// P.STEP? / PN.STEP? — advance like P.STEP but push the just_advanced
// flag instead of val[]. Lets a script gate something with IF in a
// single op call: `IF P.STEP?: SCRIPT 2` advances and only triggers when
// the playhead just entered a new stage.
static void op_P_STEPQ_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(ss->variables.p_n);
    p_step(ss, pn);
    cs_push(cs, ss->p_just_advanced[pn] ? 1 : 0);
    tele_pattern_updated();
}

static void op_PN_STEPQ_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = normalise_pn(cs_pop(cs));
    p_step(ss, pn);
    cs_push(cs, ss->p_just_advanced[pn] ? 1 : 0);
    tele_pattern_updated();
}

// clang-format off
const tele_op_t op_P_STEP        = MAKE_GET_OP(P.STEP,        op_P_STEP_get,        0, true);
const tele_op_t op_PN_STEP       = MAKE_GET_OP(PN.STEP,       op_PN_STEP_get,       1, true);
const tele_op_t op_P_STEP_NEW    = MAKE_GET_OP(P.STEP.NEW,    op_P_STEP_NEW_get,    0, true);
const tele_op_t op_PN_STEP_NEW   = MAKE_GET_OP(PN.STEP.NEW,   op_PN_STEP_NEW_get,   1, true);
const tele_op_t op_P_STEPQ       = MAKE_GET_OP(P.STEP?,       op_P_STEPQ_get,       0, true);
const tele_op_t op_PN_STEPQ      = MAKE_GET_OP(PN.STEP?,      op_PN_STEPQ_get,      1, true);
// clang-format on


////////////////////////////////////////////////////////////////////////////////
// P.A family //////////////////////////////////////////////////////////////////
//
// Non-destructive per-cell accumulator. Returns P[i] + offset[i] (or a wrapped
// variant thereof), then advances offset[i] by `step`. If `n > 0`, snaps both
// offset[i] and the per-cell trigger counter to 0 every n-th call. Pattern
// values are never modified.

// `wrap_offset == true` wraps the per-cell offset into [min, max] each tick;
// otherwise it grows unbounded (subject to int16_t saturation after ~32 K
// iterations at step=1 — documented user behavior, matching P.+).
static int16_t p_acc_step(scene_state_t *ss, int16_t pn, int16_t idx,
                          int16_t step, int16_t n, bool wrap_offset,
                          int16_t min, int16_t max) {
    pn = normalise_pn(pn);
    idx = normalise_idx(ss, pn, idx);

    int16_t offset = ss->p_acc_offset[pn][idx];
    int16_t result = ss_get_pattern_val(ss, pn, idx) + offset;

    ss->p_acc_count[pn][idx]++;
    if (n > 0 && ss->p_acc_count[pn][idx] >= (uint16_t)n) {
        ss->p_acc_offset[pn][idx] = 0;
        ss->p_acc_count[pn][idx] = 0;
    }
    else {
        int16_t next = offset + step;
        if (wrap_offset) next = wrap(next, min, max);
        ss->p_acc_offset[pn][idx] = next;
    }

    return result;
}

// P.A i step n  — unbounded offset
static void op_P_ACC_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t step = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cs_push(cs, p_acc_step(ss, ss->variables.p_n, idx, step, n, false, 0, 0));
}

// PN.A pn i step n
static void op_PN_ACC_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t step = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cs_push(cs, p_acc_step(ss, pn, idx, step, n, false, 0, 0));
}

// P.A.W i step min max n  — offset wraps in [min, max]. `n` is last to
// keep the wrap bounds in the same positions as P.+W.
static void op_P_ACC_W_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t idx = cs_pop(cs);
    int16_t step = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cs_push(cs,
            p_acc_step(ss, ss->variables.p_n, idx, step, n, true, min, max));
}

// PN.A.W pn i step min max n
static void op_PN_ACC_W_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t idx = cs_pop(cs);
    int16_t step = cs_pop(cs);
    int16_t min = cs_pop(cs);
    int16_t max = cs_pop(cs);
    int16_t n = cs_pop(cs);
    cs_push(cs, p_acc_step(ss, pn, idx, step, n, true, min, max));
}

// ACC.CLR — zero all accumulator offsets and counters across all patterns.
static void op_ACC_CLR_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es),
                           command_state_t *NOTUSED(cs)) {
    memset(ss->p_acc_offset, 0, sizeof(ss->p_acc_offset));
    memset(ss->p_acc_count, 0, sizeof(ss->p_acc_count));
}

// clang-format off
const tele_op_t op_P_ACC    = MAKE_GET_OP(P.A,     op_P_ACC_get,    3, true);
const tele_op_t op_PN_ACC   = MAKE_GET_OP(PN.A,    op_PN_ACC_get,   4, true);
const tele_op_t op_P_ACC_W  = MAKE_GET_OP(P.A.W,   op_P_ACC_W_get,  5, true);
const tele_op_t op_PN_ACC_W = MAKE_GET_OP(PN.A.W,  op_PN_ACC_W_get, 6, true);
const tele_op_t op_ACC_CLR  = MAKE_GET_OP(ACC.CLR, op_ACC_CLR_get,  0, false);
// clang-format on

////////////////////////////////////////////////////////////////////////////////
// mods: P.MAP, PN.MAP /////////////////////////////////////////////////////////

static void p_map(scene_state_t *ss, exec_state_t *es,
                  const tele_command_t *post_command, int16_t pn) {
    pn = normalise_pn(pn);
    int16_t start = ss_get_pattern_start(ss, pn);
    int16_t end = ss_get_pattern_end(ss, pn);
    int16_t *i = &es_variables(es)->i;
    process_result_t output;

    if (start >= end) { return; }

    for (int16_t idx = start; idx <= end; idx++) {
        *i = ss_get_pattern_val(ss, pn, idx);
        output = process_command(ss, es, post_command);
        if (output.has_value) { ss_set_pattern_val(ss, pn, idx, output.value); }
    }

    tele_pattern_updated();
}

static void mod_P_MAP_func(scene_state_t *ss, exec_state_t *es,
                           command_state_t *cs,
                           const tele_command_t *post_command) {
    p_map(ss, es, post_command, ss->variables.p_n);
}

static void mod_PN_MAP_func(scene_state_t *ss, exec_state_t *es,
                            command_state_t *cs,
                            const tele_command_t *post_command) {
    p_map(ss, es, post_command, cs_pop(cs));
}

const tele_mod_t mod_P_MAP = MAKE_MOD(P.MAP, mod_P_MAP_func, 0);
const tele_mod_t mod_PN_MAP = MAKE_MOD(PN.MAP, mod_PN_MAP_func, 1);
