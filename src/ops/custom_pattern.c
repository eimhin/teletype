#include "ops/custom_pattern.h"

#include "helpers.h"
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
