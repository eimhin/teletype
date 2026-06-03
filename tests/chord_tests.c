#include "chord_tests.h"

#include <stdio.h>

#include "greatest/greatest.h"
#include "teletype.h"

// Run a single command line against a persistent scene and return its value.
// A parse/validate failure surfaces as this sentinel, outside every range the
// tests assert on.
#define RUN_PARSE_FAIL ((int16_t)-32768)

static int16_t run(scene_state_t* ss, char* line) {
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;

    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    if (parse(line, &cmd, error_msg) != E_OK) {
        printf("\n  parse failed: '%s'\n", line);
        return RUN_PARSE_FAIL;
    }
    if (validate(&cmd, error_msg) != E_OK) {
        printf("\n  validate failed: '%s'\n", line);
        return RUN_PARSE_FAIL;
    }
    process_result_t r = process_command(ss, &es, &cmd);
    return r.value;
}

// Loud guard: every KEY op must parse and validate. A rename, a missing token,
// or an arity change fails here with a clear message.
TEST key_ops_parse() {
    char* lines[3] = { "KEY 0", "KEY.C 0", "KEY.V 0" };
    for (int i = 0; i < 3; i++) {
        tele_command_t cmd;
        char error_msg[TELE_ERROR_MSG_LENGTH];
        if (parse(lines[i], &cmd, error_msg) != E_OK) FAILm(lines[i]);
        if (validate(&cmd, error_msg) != E_OK) FAILm(lines[i]);
    }
    PASS();
}

// KEY returns the bank's scale and sets the active context that KEY.C / KEY.V
// read from.
TEST key_selects_and_returns_scale() {
    scene_state_t ss;
    ss_init(&ss);
    ASSERT_EQ(2741, run(&ss, "KEY 0"));   // Major scale
    ASSERT_EQ(145, run(&ss, "KEY.C 0"));  // C maj triad
    ASSERT_EQ(145, run(&ss, "KEY.V 0"));  // tonic over C
    PASS();
}

// Selecting a different bank re-points KEY.C / KEY.V.
TEST key_switches_context() {
    scene_state_t ss;
    ss_init(&ss);
    ASSERT_EQ(1453, run(&ss, "KEY 1"));   // Natural minor scale
    ASSERT_EQ(137, run(&ss, "KEY.C 0"));  // C min triad
    ASSERT_EQ(137, run(&ss, "KEY.V 0"));  // tonic over C
    PASS();
}

// Minor pentatonic: first and last voicing of the active bank.
TEST key_minor_pent_voicings() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "KEY 3");
    ASSERT_EQ(137, run(&ss, "KEY.V 0"));    // C Eb G
    ASSERT_EQ(5281, run(&ss, "KEY.V 13"));  // last entry, C F G Bb C
    PASS();
}

// KEY.V is ordered intimate -> lush -> wide across the index.
TEST key_v_intensity_sweep() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "KEY 0");
    ASSERT_EQ(145, run(&ss, "KEY.V 0"));    // intimate (C E G)
    ASSERT_EQ(2193, run(&ss, "KEY.V 7"));   // lush (Cmaj7)
    ASSERT_EQ(4641, run(&ss, "KEY.V 18"));  // wide (C F A C), last entry
    PASS();
}

// Out-of-range indices clamp to the ends of the active bank's lists.
TEST key_indices_clamp() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "KEY 0");
    ASSERT_EQ(4641, run(&ss, "KEY.V 99"));   // -> last voicing
    ASSERT_EQ(145, run(&ss, "KEY.V -1"));    // -> first voicing
    ASSERT_EQ(145, run(&ss, "KEY.C -1"));    // -> first shape
    ASSERT_EQ(2197, run(&ss, "KEY.C 99"));   // -> last shape (maj9)
    PASS();
}

// A bank index out of range clamps; KEY -1 selects bank 0 (Major).
TEST key_bank_clamps() {
    scene_state_t ss;
    ss_init(&ss);
    ASSERT_EQ(2741, run(&ss, "KEY -1"));  // clamp low -> bank 0
    ASSERT_EQ(145, run(&ss, "KEY.C 0"));
    ASSERT_EQ(1193, run(&ss, "KEY 99"));  // clamp high -> bank 3 (minor pent)
    ASSERT_EQ(137, run(&ss, "KEY.C 0"));
    PASS();
}

// Before any KEY call the active bank defaults to 0, so KEY.C / KEY.V work.
TEST key_default_bank_is_zero() {
    scene_state_t ss;
    ss_init(&ss);
    ASSERT_EQ(145, run(&ss, "KEY.C 0"));
    ASSERT_EQ(145, run(&ss, "KEY.V 0"));
    PASS();
}

// Bank 2 (major pentatonic) is selectable and its shapes/voicings read back.
TEST key_major_pent_selectable() {
    scene_state_t ss;
    ss_init(&ss);
    ASSERT_EQ(661, run(&ss, "KEY 2"));    // Major pentatonic scale
    ASSERT_EQ(145, run(&ss, "KEY.C 0"));  // C maj triad
    ASSERT_EQ(145, run(&ss, "KEY.V 0"));  // tonic over C
    PASS();
}

// Clamp-from-above is wired per bank: a large index must return each bank's
// OWN last shape / voicing (guards against the ops reading bank 0's counts).
TEST key_clamp_high_per_bank() {
    scene_state_t ss;
    ss_init(&ss);
    // bank 0 Major: 7 shapes (last maj9 2197), 19 voicings (last 4641)
    run(&ss, "KEY 0");
    ASSERT_EQ(2197, run(&ss, "KEY.C 99"));
    ASSERT_EQ(4641, run(&ss, "KEY.V 99"));
    // bank 1 Natural minor: 7 shapes (last 1193), 18 voicings (last 17537)
    run(&ss, "KEY 1");
    ASSERT_EQ(1193, run(&ss, "KEY.C 99"));
    ASSERT_EQ(17537, run(&ss, "KEY.V 99"));
    // bank 2 Major pentatonic: 7 shapes (last 529), 14 voicings (last 16913)
    run(&ss, "KEY 2");
    ASSERT_EQ(529, run(&ss, "KEY.C 99"));
    ASSERT_EQ(16913, run(&ss, "KEY.V 99"));
    // bank 3 Minor pentatonic: 7 shapes (last 1057), 14 voicings (last 5281)
    run(&ss, "KEY 3");
    ASSERT_EQ(1057, run(&ss, "KEY.C 99"));
    ASSERT_EQ(5281, run(&ss, "KEY.V 99"));
    PASS();
}

SUITE(chord_suite) {
    RUN_TEST(key_ops_parse);
    RUN_TEST(key_selects_and_returns_scale);
    RUN_TEST(key_switches_context);
    RUN_TEST(key_minor_pent_voicings);
    RUN_TEST(key_major_pent_selectable);
    RUN_TEST(key_v_intensity_sweep);
    RUN_TEST(key_indices_clamp);
    RUN_TEST(key_clamp_high_per_bank);
    RUN_TEST(key_bank_clamps);
    RUN_TEST(key_default_bank_is_zero);
}
