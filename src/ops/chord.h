#ifndef _OPS_CHORD_H_
#define _OPS_CHORD_H_

#include "ops/op.h"

// KEY / KEY.C / KEY.V — context-coupled chord ops for i2c2midi.
// KEY selects an active "bank" (key) and returns its scale; KEY.C and KEY.V
// read C-rooted chord shapes / voice-led voicings from that active bank. All
// three return the reverse-binary decimals consumed by I2M.C.B.
extern const tele_op_t op_KEY;
extern const tele_op_t op_KEY_C;
extern const tele_op_t op_KEY_V;

#endif
