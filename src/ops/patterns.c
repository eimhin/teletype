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
