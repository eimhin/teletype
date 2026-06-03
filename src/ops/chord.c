#include "ops/chord.h"

#include "helpers.h"  // normalise_value

// Chord/key banks for i2c2midi. Each bank bundles, all rooted on C (semitone
// 0): a scale, a palette of C-rooted chord shapes that fit that scale (KEY.C,
// for transposing), and an ordered list of voice-led voicings over a C pedal
// (KEY.V, sequenced intimate -> wide for browsing). Every value is a
// reverse-binary decimal (sum of 2^semitone) as consumed by I2M.C.B.
//
// The op set holds one piece of state, ss->chord_bank — the active bank index.
// KEY n selects the bank (and returns its scale); KEY.C / KEY.V read from
// whichever bank is active. Expanding a bank later is just an array edit.

typedef struct {
    uint16_t scale;          // reverse-binary decimal of the scale
    uint8_t shape_count;     // KEY.C palette length
    const uint16_t *shapes;  // C-rooted shapes that fit the scale
    uint8_t voicing_count;   // KEY.V list length
    const uint16_t *voicings;  // voice-led voicings over a C pedal
} chord_bank_t;

// --- Bank 0: Major (scale 2741 = 0,2,4,5,7,9,11) ---
static const uint16_t major_shapes[] = {
    145,   // maj triad
    2193,  // maj7
    657,   // maj6
    133,   // sus2
    161,   // sus4
    149,   // add9
    2197,  // maj9
};
static const uint16_t major_voicings[] = {
    // intimate
    145, 161, 545, 529, 517, 133,
    // lush
    657, 2193, 2065, 2177,
    // wide
    4225, 4241, 16529, 16513, 18561, 18577, 17041, 16913, 4641,
};

// --- Bank 1: Natural minor (scale 1453 = 0,2,3,5,7,8,10) ---
static const uint16_t minor_shapes[] = {
    137,   // min triad
    1161,  // min7
    141,   // min add9
    133,   // sus2
    161,   // sus4
    1165,  // min9
    1193,  // min11
};
static const uint16_t minor_voicings[] = {
    // intimate
    137, 265, 289, 161, 133,
    // lush
    1161, 1033, 1057, 1153,
    // wide
    4225, 4233, 4361, 4385, 16513, 16521, 17545, 5257, 17537,
};

// --- Bank 2: Major pentatonic (scale 661 = 0,2,4,7,9) ---
static const uint16_t major_pent_shapes[] = {
    145,  // maj triad
    657,  // maj6
    133,  // sus2
    149,  // add9
    661,  // 6/9
    645,  // open (no 3)
    529,  // C E A
};
static const uint16_t major_pent_voicings[] = {
    // intimate
    145, 529, 517, 133, 641,
    // lush
    657, 645,
    // wide
    17041, 16529, 4241, 4225, 16513, 17025, 16913,
};

// --- Bank 3: Minor pentatonic (scale 1193 = 0,3,5,7,10) ---
static const uint16_t minor_pent_shapes[] = {
    137,   // min triad
    1161,  // min7
    161,   // sus4
    169,   // min add11
    1193,  // min11
    1185,  // 7sus4
    1057,  // quartal
};
static const uint16_t minor_pent_voicings[] = {
    // intimate
    137, 161,
    // lush
    1185, 1153, 1057, 1033, 1161,
    // wide
    4233, 4225, 4129, 5153, 5249, 5257, 5281,
};

#define BANK_COUNT 4
#define ARRAY_LEN(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

// Every bank must expose at least one shape and one voicing: the `count - 1`
// clamp in the ops below would otherwise yield a negative index. Lock the
// invariant at compile time so adding/editing a bank can't silently break it
// (compiles to nothing; a zero-length array makes the typedef size negative).
#define CHORD_ASSERT_NONEMPTY(a) \
    typedef char chord_nonempty_##a[ARRAY_LEN(a) >= 1 ? 1 : -1]
CHORD_ASSERT_NONEMPTY(major_shapes);
CHORD_ASSERT_NONEMPTY(major_voicings);
CHORD_ASSERT_NONEMPTY(minor_shapes);
CHORD_ASSERT_NONEMPTY(minor_voicings);
CHORD_ASSERT_NONEMPTY(major_pent_shapes);
CHORD_ASSERT_NONEMPTY(major_pent_voicings);
CHORD_ASSERT_NONEMPTY(minor_pent_shapes);
CHORD_ASSERT_NONEMPTY(minor_pent_voicings);

static const chord_bank_t banks[BANK_COUNT] = {
    { .scale = 2741,
      .shape_count = ARRAY_LEN(major_shapes),
      .shapes = major_shapes,
      .voicing_count = ARRAY_LEN(major_voicings),
      .voicings = major_voicings },
    { .scale = 1453,
      .shape_count = ARRAY_LEN(minor_shapes),
      .shapes = minor_shapes,
      .voicing_count = ARRAY_LEN(minor_voicings),
      .voicings = minor_voicings },
    { .scale = 661,
      .shape_count = ARRAY_LEN(major_pent_shapes),
      .shapes = major_pent_shapes,
      .voicing_count = ARRAY_LEN(major_pent_voicings),
      .voicings = major_pent_voicings },
    { .scale = 1193,
      .shape_count = ARRAY_LEN(minor_pent_shapes),
      .shapes = minor_pent_shapes,
      .voicing_count = ARRAY_LEN(minor_pent_voicings),
      .voicings = minor_pent_voicings },
};

// KEY n: select bank n as the active context (side effect) and return its
// scale decimal. n is clamped to a valid bank.
static void op_KEY_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t n = normalise_value(0, BANK_COUNT - 1, 0, cs_pop(cs));
    ss->chord_bank = (int8_t)n;
    cs_push(cs, banks[n].scale);
}

// KEY.C t: return the t-th C-rooted chord shape from the active bank.
static void op_KEY_C_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    const chord_bank_t *b = &banks[ss->chord_bank];
    int16_t t = normalise_value(0, b->shape_count - 1, 0, cs_pop(cs));
    cs_push(cs, b->shapes[t]);
}

// KEY.V i: return the i-th voicing from the active bank (intimate -> wide).
static void op_KEY_V_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    const chord_bank_t *b = &banks[ss->chord_bank];
    int16_t i = normalise_value(0, b->voicing_count - 1, 0, cs_pop(cs));
    cs_push(cs, b->voicings[i]);
}

const tele_op_t op_KEY = MAKE_GET_OP(KEY, op_KEY_get, 1, true);
const tele_op_t op_KEY_C = MAKE_GET_OP(KEY.C, op_KEY_C_get, 1, true);
const tele_op_t op_KEY_V = MAKE_GET_OP(KEY.V, op_KEY_V_get, 1, true);
