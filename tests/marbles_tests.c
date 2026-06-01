#include "marbles_tests.h"

#include <stdio.h>
#include <string.h>

#include "greatest/greatest.h"
#include "teletype.h"

// Run a single command line against a persistent scene and return its value.
// Parse/validate failures are reported to stdout (see mrbl_ops_parse for the
// loud wiring guard) and surface as the unmistakable sentinel below, which is
// outside every range these tests assert on.
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

// Loud guard: every MRBL op must parse and validate. An op rename, a missing
// token, or an arity change fails here with a clear message instead of
// silently degrading other tests.
TEST mrbl_ops_parse() {
    char* lines[4] = { "MRBL.V 0 0 1", "MRBL.D 0 0 1", "MRBL.T 0 0 1",
                       "MRBL.SD 1" };
    for (int i = 0; i < 4; i++) {
        tele_command_t cmd;
        char error_msg[TELE_ERROR_MSG_LENGTH];
        if (parse(lines[i], &cmd, error_msg) != E_OK) FAILm(lines[i]);
        if (validate(&cmd, error_msg) != E_OK) FAILm(lines[i]);
    }
    PASS();
}

// dv 100 = locked loop: MRBL never refreshes, it just reads the pattern cells
// in order and wraps at P.L. Independent of the rng entirely.
TEST mrbl_locked_loop() {
    scene_state_t ss;
    ss_init(&ss);

    ss_set_pattern_len(&ss, 0, 4);
    ss_set_pattern_val(&ss, 0, 0, 11);
    ss_set_pattern_val(&ss, 0, 1, 22);
    ss_set_pattern_val(&ss, 0, 2, 33);
    ss_set_pattern_val(&ss, 0, 3, 44);
    ss_set_pattern_idx(&ss, 0, 0);

    int16_t expected[8] = { 11, 22, 33, 44, 11, 22, 33, 44 };
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(expected[i], run(&ss, "MRBL.V 0 100 1000"));
    }
    PASS();
}

// An empty loop (P.L 0) is a no-op that returns 0.
TEST mrbl_zero_length() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 0);
    ASSERT_EQ(0, run(&ss, "MRBL.V 0 50 1000"));
    PASS();
}

// MRBL.V refresh values stay within 0..spr AND both endpoints are reachable
// (guards against an off-by-one such as `% spr` instead of `% (spr + 1)`).
TEST mrbl_value_endpoints_reachable() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 12345");
    int saw_lo = 0, saw_hi = 0;
    for (int i = 0; i < 500; i++) {
        int16_t v = run(&ss, "MRBL.V 0 0 3");
        ASSERT(v >= 0 && v <= 3);
        if (v == 0) saw_lo = 1;
        if (v == 3) saw_hi = 1;  // 3 unreachable if the code used `% spr`
    }
    ASSERT(saw_lo);
    ASSERT(saw_hi);
    PASS();
}

// MRBL.D degrees are 1-indexed and both endpoints (1 and deg) are reachable.
TEST mrbl_degree_endpoints_reachable() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 999");
    int saw_lo = 0, saw_hi = 0;
    for (int i = 0; i < 500; i++) {
        int16_t v = run(&ss, "MRBL.D 0 0 5");
        ASSERT(v >= 1 && v <= 5);
        if (v == 1) saw_lo = 1;
        if (v == 5) saw_hi = 1;
    }
    ASSERT(saw_lo);
    ASSERT(saw_hi);
    PASS();
}

// MRBL.T is binary and, at bias 50, both 0 and 1 occur.
TEST mrbl_gate_is_binary() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 7");
    int saw_off = 0, saw_on = 0;
    for (int i = 0; i < 500; i++) {
        int16_t v = run(&ss, "MRBL.T 0 0 50");
        ASSERT(v == 0 || v == 1);
        if (v == 0) saw_off = 1;
        if (v == 1) saw_on = 1;
    }
    ASSERT(saw_off);
    ASSERT(saw_on);
    PASS();
}

// Gate bias extremes: bias 0 is always off, bias 100 is always on.
TEST mrbl_gate_bias_extremes() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 3");
    for (int i = 0; i < 100; i++) ASSERT_EQ(0, run(&ss, "MRBL.T 0 0 0"));
    for (int i = 0; i < 100; i++) ASSERT_EQ(1, run(&ss, "MRBL.T 0 0 100"));
    PASS();
}

// spr < 0 clamps to 0, so MRBL.V emits a constant 0.
TEST mrbl_negative_spread_is_zero() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 5");
    for (int i = 0; i < 50; i++) ASSERT_EQ(0, run(&ss, "MRBL.V 0 0 -5"));
    PASS();
}

// deg < 1 clamps to 1, so MRBL.D emits a constant degree 1.
TEST mrbl_degree_below_one_is_one() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 5");
    for (int i = 0; i < 50; i++) ASSERT_EQ(1, run(&ss, "MRBL.D 0 0 0"));
    PASS();
}

// dv > 100 clamps to 100 (stays locked) — same behaviour as the locked loop.
TEST mrbl_dv_above_100_stays_locked() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 3);
    ss_set_pattern_val(&ss, 0, 0, 7);
    ss_set_pattern_val(&ss, 0, 1, 8);
    ss_set_pattern_val(&ss, 0, 2, 9);
    int16_t expected[6] = { 7, 8, 9, 7, 8, 9 };
    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(expected[i], run(&ss, "MRBL.V 0 200 1000"));
    }
    PASS();
}

// dv < 0 clamps to 0 (always refresh) — identical to dv 0 given the same seed
// and starting buffer.
TEST mrbl_dv_below_0_matches_zero() {
    scene_state_t a, b;
    ss_init(&a);
    ss_init(&b);
    ss_set_pattern_len(&a, 0, 4);
    ss_set_pattern_len(&b, 0, 4);
    run(&a, "MRBL.SD 4242");
    run(&b, "MRBL.SD 4242");
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(run(&a, "MRBL.V 0 0 1000"), run(&b, "MRBL.V 0 -50 1000"));
    }
    PASS();
}

// An out-of-range pattern bank clamps to 0..7 (and never writes out of bounds):
// p 99 addresses bank 7, p -3 addresses bank 0.
TEST mrbl_bank_clamped() {
    scene_state_t ss;
    ss_init(&ss);

    ss_set_pattern_len(&ss, 7, 2);
    ss_set_pattern_val(&ss, 7, 0, 41);
    ss_set_pattern_val(&ss, 7, 1, 42);
    ASSERT_EQ(41, run(&ss, "MRBL.V 99 100 1000"));
    ASSERT_EQ(42, run(&ss, "MRBL.V 99 100 1000"));

    ss_set_pattern_len(&ss, 0, 2);
    ss_set_pattern_val(&ss, 0, 0, 51);
    ss_set_pattern_val(&ss, 0, 1, 52);
    ASSERT_EQ(51, run(&ss, "MRBL.V -3 100 1000"));
    ASSERT_EQ(52, run(&ss, "MRBL.V -3 100 1000"));
    PASS();
}

// Intermediate dv evolves a loop: over one pass at dv 50, some cells are
// refreshed and some are recalled (a mix, not all-or-nothing). Fresh values
// are 0..100, so a sentinel of 12345 marks a cell that was never refreshed.
TEST mrbl_intermediate_dv_mixes() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 16);
    for (int i = 0; i < 16; i++) ss_set_pattern_val(&ss, 0, i, 12345);
    ss_set_pattern_idx(&ss, 0, 0);
    run(&ss, "MRBL.SD 808");

    for (int i = 0; i < 16; i++) run(&ss, "MRBL.V 0 50 100");

    int refreshed = 0, recalled = 0;
    for (int i = 0; i < 16; i++) {
        if (ss_get_pattern_val(&ss, 0, i) == 12345)
            recalled++;
        else
            refreshed++;
    }
    ASSERT(refreshed > 0);
    ASSERT(recalled > 0);
    PASS();
}

// The playhead resumes from the current P.I, not always from 0.
TEST mrbl_playhead_starts_at_idx() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    ss_set_pattern_val(&ss, 0, 0, 10);
    ss_set_pattern_val(&ss, 0, 1, 20);
    ss_set_pattern_val(&ss, 0, 2, 30);
    ss_set_pattern_val(&ss, 0, 3, 40);
    ss_set_pattern_idx(&ss, 0, 2);
    ASSERT_EQ(30, run(&ss, "MRBL.V 0 100 0"));  // resumes at cell 2
    ASSERT_EQ(40, run(&ss, "MRBL.V 0 100 0"));
    ASSERT_EQ(10, run(&ss, "MRBL.V 0 100 0"));  // wraps to cell 0
    PASS();
}

// An out-of-range starting index (idx >= len) is reset to cell 0.
TEST mrbl_out_of_range_idx_resets() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    ss_set_pattern_val(&ss, 0, 0, 99);
    ss_set_pattern_idx(&ss, 0, 9);  // > len
    ASSERT_EQ(99, run(&ss, "MRBL.V 0 100 0"));
    PASS();
}

// Same seed + same starting buffer + same call sequence => identical evolution;
// a different seed yields a different trajectory.
TEST mrbl_reproducible() {
    scene_state_t a, b, c;
    ss_init(&a);
    ss_init(&b);
    ss_init(&c);
    ss_set_pattern_len(&a, 0, 4);
    ss_set_pattern_len(&b, 0, 4);
    ss_set_pattern_len(&c, 0, 4);
    run(&a, "MRBL.SD 12345");
    run(&b, "MRBL.SD 12345");
    run(&c, "MRBL.SD 54321");

    int diff_from_c = 0;
    for (int i = 0; i < 16; i++) {
        int16_t va = run(&a, "MRBL.V 0 0 1000");
        int16_t vb = run(&b, "MRBL.V 0 0 1000");
        int16_t vc = run(&c, "MRBL.V 0 0 1000");
        ASSERT_EQ(va, vb);
        if (va != vc) diff_from_c++;
    }
    // 16 independent draws over 0..1000 with a different seed will not coincide.
    ASSERT(diff_from_c > 0);
    PASS();
}

// Re-applying the same seed restarts the stream: with dv 0 (always refresh) the
// returned value is the freshly drawn one, so the first post-reseed value equals
// the very first value, regardless of where the playhead sits.
TEST mrbl_reseed_resets_stream() {
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_len(&ss, 0, 4);
    run(&ss, "MRBL.SD 12345");
    int16_t first = run(&ss, "MRBL.V 0 0 1000");
    run(&ss, "MRBL.V 0 0 1000");
    run(&ss, "MRBL.V 0 0 1000");
    run(&ss, "MRBL.SD 12345");  // reseed identical
    ASSERT_EQ(first, run(&ss, "MRBL.V 0 0 1000"));
    PASS();
}

// MRBL draws from its own rng stream: using it does not perturb the sequences
// RAND or TOSS produce from their (separately seeded) streams.
TEST mrbl_independent_of_rand_and_toss() {
    scene_state_t a, b;
    ss_init(&a);
    ss_init(&b);

    // Reference RAND/TOSS sequences, no MRBL in the mix.
    run(&a, "R.SD 42");
    run(&a, "TOSS.SD 99");
    int16_t ref_rand[10], ref_toss[10];
    for (int i = 0; i < 10; i++) {
        ref_rand[i] = run(&a, "RAND 1000");
        ref_toss[i] = run(&a, "TOSS");
    }

    // Same RAND/TOSS seeds, but interleave (seeded) MRBL calls.
    run(&b, "R.SD 42");
    run(&b, "TOSS.SD 99");
    run(&b, "MRBL.SD 777");
    ss_set_pattern_len(&b, 0, 4);
    for (int i = 0; i < 10; i++) {
        run(&b, "MRBL.V 0 0 1000");
        ASSERT_EQ(ref_rand[i], run(&b, "RAND 1000"));
        run(&b, "MRBL.T 0 0 50");
        ASSERT_EQ(ref_toss[i], run(&b, "TOSS"));
    }
    PASS();
}

// The converse: interleaving RAND/TOSS does not perturb MRBL's own sequence.
TEST mrbl_unperturbed_by_rand_and_toss() {
    scene_state_t a, b;
    ss_init(&a);
    ss_init(&b);
    ss_set_pattern_len(&a, 0, 4);
    ss_set_pattern_len(&b, 0, 4);
    run(&a, "MRBL.SD 555");
    run(&b, "MRBL.SD 555");
    run(&b, "R.SD 1");
    run(&b, "TOSS.SD 2");

    for (int i = 0; i < 16; i++) {
        int16_t va = run(&a, "MRBL.V 0 0 1000");
        run(&b, "RAND 1000");
        run(&b, "TOSS");
        ASSERT_EQ(va, run(&b, "MRBL.V 0 0 1000"));
    }
    PASS();
}

SUITE(marbles_suite) {
    RUN_TEST(mrbl_ops_parse);
    RUN_TEST(mrbl_locked_loop);
    RUN_TEST(mrbl_zero_length);
    RUN_TEST(mrbl_value_endpoints_reachable);
    RUN_TEST(mrbl_degree_endpoints_reachable);
    RUN_TEST(mrbl_gate_is_binary);
    RUN_TEST(mrbl_gate_bias_extremes);
    RUN_TEST(mrbl_negative_spread_is_zero);
    RUN_TEST(mrbl_degree_below_one_is_one);
    RUN_TEST(mrbl_dv_above_100_stays_locked);
    RUN_TEST(mrbl_dv_below_0_matches_zero);
    RUN_TEST(mrbl_bank_clamped);
    RUN_TEST(mrbl_intermediate_dv_mixes);
    RUN_TEST(mrbl_playhead_starts_at_idx);
    RUN_TEST(mrbl_out_of_range_idx_resets);
    RUN_TEST(mrbl_reproducible);
    RUN_TEST(mrbl_reseed_resets_stream);
    RUN_TEST(mrbl_independent_of_rand_and_toss);
    RUN_TEST(mrbl_unperturbed_by_rand_and_toss);
}
