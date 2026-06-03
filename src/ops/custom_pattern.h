#ifndef _OPS_CUSTOM_PATTERN_H_
#define _OPS_CUSTOM_PATTERN_H_

#include "ops/op.h"

// XP: the single 8-column x 16-step custom pattern. Each column is an
// independent track with its own idx/len/wrap/start/end. Explicit-column
// single op family — every op takes a <col> (0..CUSTOM_PATTERN_WIDTH-1) first.
extern const tele_op_t op_XP;
extern const tele_op_t op_XP_HERE;
extern const tele_op_t op_XP_I;
extern const tele_op_t op_XP_L;
extern const tele_op_t op_XP_WRAP;
extern const tele_op_t op_XP_START;
extern const tele_op_t op_XP_END;
extern const tele_op_t op_XP_NEXT;
extern const tele_op_t op_XP_NEXT_ALL;

#endif
