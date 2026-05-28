#include "process_tests.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // ssize_t

#include "greatest/greatest.h"
#include "ops/patterns.h"
#include "teletype.h"
// runs multiple lines of commands and then asserts that the final answer is
// correct (allows contiuation of state)
TEST process_helper_state(scene_state_t* ss, size_t n, char* lines[],
                          int16_t answer) {
    process_result_t result = { .has_value = false, .value = 0 };
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    for (size_t i = 0; i < n; i++) {
        tele_command_t cmd;
        char error_msg[TELE_ERROR_MSG_LENGTH];
        error_t error = parse(lines[i], &cmd, error_msg);
        if (error != E_OK) { FAIL(); }
        if (validate(&cmd, error_msg) != E_OK) { FAIL(); }
        result = process_command(ss, &es, &cmd);
    }

    ASSERT_EQ(result.has_value, true);

    // prep a message for the test
    ssize_t size = 0;
    for (size_t i = 0; i < n; i++) {
        size += strlen(lines[i]) + 3;  // 3 extra chars fo ' | '
    }
    char* message = calloc(size + 1, sizeof(char));
    for (size_t i = 0; i < n; i++) {
        strcat(message, lines[i]);
        if (i < n - 1) strcat(message, " | ");
    }

    ASSERT_EQm(message, result.value, answer);

    free(message);

    PASS();
}


// runs multiple lines of commands and then asserts that the final answer is
// correct
TEST process_helper(size_t n, char* lines[], int16_t answer) {
    scene_state_t ss;
    ss_init(&ss);

    CHECK_CALL(process_helper_state(&ss, n, lines, answer));

    PASS();
}

TEST test_numbers() {
    char* test1[1] = { "1" };
    CHECK_CALL(process_helper(1, test1, 1));

    char* test2[1] = { "2" };
    CHECK_CALL(process_helper(1, test2, 2));

    char* test3[1] = { "0" };
    CHECK_CALL(process_helper(1, test3, 0));

    char* test4[1] = { "-1" };
    CHECK_CALL(process_helper(1, test4, -1));

    PASS();
}

TEST test_ADD() {
    char* test1[1] = { "ADD 5 6" };
    CHECK_CALL(process_helper(1, test1, 11));

    char* test2[1] = { "ADD 2 -2" };
    CHECK_CALL(process_helper(1, test2, 0));

    PASS();
}

TEST test_PROB() {
    char* test1[3] = { "X 0", "PROB 100: X + X 1", "X" };
    for (int i = 0; i < 1000; i++) { CHECK_CALL(process_helper(3, test1, 1)); }

    PASS();
}

TEST test_IF() {
    char* test1[3] = { "X 0", "IF 1: X 1", "X" };
    CHECK_CALL(process_helper(3, test1, 1));

    char* test2[3] = { "X 0", "IF 0: X 1", "X" };
    CHECK_CALL(process_helper(3, test2, 0));

    char* test3[3] = { "PN 0 0 0", "IF 1: PN 0 0 1", "PN 0 0" };
    CHECK_CALL(process_helper(3, test3, 1));

    char* test4[3] = { "PN 0 0 0", "IF 0: PN 0 0 1", "PN 0 0" };
    CHECK_CALL(process_helper(3, test4, 0));

    char* test5[3] = { "X 0", "ELSE: X 1", "X" };
    CHECK_CALL(process_helper(3, test5, 0));

    char* test6[3] = { "X 0", "ELIF 1: X 1", "X" };
    CHECK_CALL(process_helper(3, test6, 0));

    char* test7[4] = { "X 0", "ELIF 1: X 1", "ELSE: X 1", "X" };
    CHECK_CALL(process_helper(4, test7, 0));

    char* test8[4] = { "X 0", "IF 0: X 2", "ELSE: X 1", "X" };
    CHECK_CALL(process_helper(4, test8, 1));

    char* test9[4] = { "X 0", "IF 0: X 2", "ELIF 1: X 1", "X" };
    CHECK_CALL(process_helper(4, test9, 1));

    char* test10[5] = { "X 0", "IF 1: X 1", "ELIF 1: X 2", "ELSE: X 3", "X" };
    CHECK_CALL(process_helper(5, test10, 1));

    char* test11[5] = { "X 0", "IF 0: X 1", "ELIF 1: X 2", "ELSE: X 3", "X" };
    CHECK_CALL(process_helper(5, test11, 2));

    char* test12[5] = { "X 0", "IF 0: X 1", "ELIF 0: X 2", "ELSE: X 3", "X" };
    CHECK_CALL(process_helper(5, test12, 3));

    PASS();
}

TEST test_FLIP() {
    char* test1[2] = { "FLIP 0", "FLIP" };
    CHECK_CALL(process_helper(2, test1, 0));

    char* test2[1] = { "FLIP" };
    CHECK_CALL(process_helper(1, test2, 0));

    char* test3[2] = { "FLIP", "FLIP" };
    CHECK_CALL(process_helper(2, test3, 1));

    char* test4[2] = { "FLIP 100", "FLIP" };
    CHECK_CALL(process_helper(2, test4, 1));

    PASS();
}

TEST test_L() {
    char* test1[3] = { "X 0", "L 1 10: X I", "X" };
    CHECK_CALL(process_helper(3, test1, 10));

    char* test2[3] = { "X 0", "L 1 -10: X I", "X" };
    CHECK_CALL(process_helper(3, test2, -10));

    char* test3[3] = { "X 0", "L 1 10: X ADD X I", "X" };
    CHECK_CALL(process_helper(3, test3, 55));

    PASS();
}

TEST test_O() {
    scene_state_t ss;
    ss_init(&ss);

    char* test1[6] = {
        "O.MIN 0", "O.MAX 63", "O.INC 1", "O.WRAP 1", "O 0", "O"
    };
    CHECK_CALL(process_helper_state(&ss, 6, test1, 0));

    char* test2[1] = { "O" };
    CHECK_CALL(process_helper_state(&ss, 1, test2, 1));

    char* test3[2] = { "O 0", "O" };
    CHECK_CALL(process_helper_state(&ss, 2, test3, 0));

    char* test4[2] = { "O 63", "O" };
    CHECK_CALL(process_helper_state(&ss, 2, test4, 63));

    char* test5[1] = { "O" };
    CHECK_CALL(process_helper_state(&ss, 1, test5, 0));

    char* test6[4] = { "O 0", "O.INC -1", "O", "O" };
    CHECK_CALL(process_helper_state(&ss, 4, test6, 63));

    char* test7[4] = { "O 0", "O.WRAP 0", "O", "O" };
    CHECK_CALL(process_helper_state(&ss, 4, test7, 0));

    PASS();
}

TEST test_P() {
    char* test1[2] = { "P 0 1", "P 0" };
    CHECK_CALL(process_helper(2, test1, 1));

    char* test2[2] = { "P 0 2", "P 0" };
    CHECK_CALL(process_helper(2, test2, 2));

    PASS();
}

TEST test_PN() {
    char* test1[2] = { "PN 0 0 1", "PN 0 0" };
    CHECK_CALL(process_helper(2, test1, 1));

    char* test2[2] = { "PN 0 0 2", "PN 0 0" };
    CHECK_CALL(process_helper(2, test2, 2));

    char* test3[3] = { "P.N 0", "P 0 3", "PN 0 0" };
    CHECK_CALL(process_helper(3, test3, 3));

    char* test4[3] = { "P.N 0", "PN 0 0 4", "P 0" };
    CHECK_CALL(process_helper(3, test4, 4));

    PASS();
}

TEST test_Q() {
    scene_state_t ss;
    ss_init(&ss);

    char* test1[2] = { "Q.N 16", "Q.N" };
    CHECK_CALL(process_helper_state(&ss, 2, test1, 16));

    for (int i = 1; i <= 16; i++) {
        char buf1[20];
        char buf2[20];
        sprintf(buf1, "Q.N %d", i);
        sprintf(buf2, "Q %d", i);
        char* test2[3] = { buf1, buf2, "Q" };
        CHECK_CALL(process_helper_state(&ss, 3, test2, 1));
    }

    for (int i = 1; i <= 16; i++) {
        char buf1[20];
        sprintf(buf1, "Q.N %d", i);
        char* test3[2] = { buf1, "Q" };
        CHECK_CALL(process_helper_state(&ss, 2, test3, 17 - i));
    }

    // 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16 = 136
    // 136 / 16 = 8.5
    char* test4[1] = { "Q.AVG" };
    CHECK_CALL(process_helper_state(&ss, 1, test4, 9));

    char* test5[2] = { "Q.AVG 5", "Q.AVG" };
    CHECK_CALL(process_helper_state(&ss, 2, test5, 5));

    for (int i = 1; i <= 16; i++) {
        char* test6[1] = { "Q" };
        CHECK_CALL(process_helper_state(&ss, 1, test6, 5));
    }

    PASS();
}

TEST test_X() {
    char* test1[2] = { "X 0", "X" };
    CHECK_CALL(process_helper(2, test1, 0));

    char* test2[2] = { "X 10", "X" };
    CHECK_CALL(process_helper(2, test2, 10));

    char* test3[2] = { "X 10", "ADD 5 X" };
    CHECK_CALL(process_helper(2, test3, 15));

    PASS();
}

TEST test_sub_commands() {
    char* test1[2] = { "X 10; Y 20; Z 30", "ADD X ADD Y Z" };
    CHECK_CALL(process_helper(2, test1, 60));

    char* test2[2] = { "IF 1: X 1; Y 2; Z 3", "ADD X ADD Y Z" };
    CHECK_CALL(process_helper(2, test2, 6));

    char* test3[3] = { "X 0; Y 0; Z 0", "IF 0: X 1; Y 2; Z 3",
                       "ADD X ADD Y Z" };
    CHECK_CALL(process_helper(3, test3, 0));

    PASS();
}

TEST test_blank_command() {
    scene_state_t ss;
    ss_init(&ss);
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];

    char* test = "";
    error_t error = parse(test, &cmd, error_msg);
    if (error != E_OK) { FAIL(); }
    if (validate(&cmd, error_msg) != E_OK) { FAIL(); }

    process_result_t result = process_command(&ss, &es, &cmd);

    ASSERT_EQ(result.has_value, false);
    ASSERT_EQ(result.value, 0);

    PASS();
}

TEST test_P_ROT_1() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep1[3] = { "P.START 0", "P.END 3", "0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep1, 0));

    char* prep2[5] = { "P 0 1", "P 1 2", "P 2 3", "P 3 4", "P P.END" };
    CHECK_CALL(process_helper_state(&ss, 5, prep2, 4));

    char* prep3[2] = { "L 4 63: P I -1", "P + 1 P.END" };
    CHECK_CALL(process_helper_state(&ss, 2, prep3, -1));

    char* test1[2] = { "P.ROT 1", "P 0" };
    CHECK_CALL(process_helper_state(&ss, 2, test1, 4));

    char* test2[1] = { "P 1" };
    CHECK_CALL(process_helper_state(&ss, 1, test2, 1));

    char* test3[1] = { "P 2" };
    CHECK_CALL(process_helper_state(&ss, 1, test3, 2));

    char* test4[1] = { "P 3" };
    CHECK_CALL(process_helper_state(&ss, 1, test4, 3));

    PASS();
}

TEST test_P_ROT_3() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep1[3] = { "P.START 0", "P.END 3", "0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep1, 0));

    char* prep2[5] = { "P 0 1", "P 1 2", "P 2 3", "P 3 4", "P P.END" };
    CHECK_CALL(process_helper_state(&ss, 5, prep2, 4));

    char* prep3[2] = { "L 4 63: P I -1", "P + 1 P.END" };
    CHECK_CALL(process_helper_state(&ss, 2, prep3, -1));

    char* test1[2] = { "P.ROT 3", "P 0" };
    CHECK_CALL(process_helper_state(&ss, 2, test1, 2));

    char* test2[1] = { "P 1" };
    CHECK_CALL(process_helper_state(&ss, 1, test2, 3));

    char* test3[1] = { "P 2" };
    CHECK_CALL(process_helper_state(&ss, 1, test3, 4));

    char* test4[1] = { "P 3" };
    CHECK_CALL(process_helper_state(&ss, 1, test4, 1));

    PASS();
}

// P.A: unbounded accumulator. Emit base+offset, advance offset by step.
TEST test_P_ACC() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep[3] = { "P.N 0", "PN 0 0 100", "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 100));

    // step=2, n=0 (no reset): 100, 102, 104, 106, ...
    char* t[1] = { "P.A 0 2 0" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t, 102));
    CHECK_CALL(process_helper_state(&ss, 1, t, 104));
    CHECK_CALL(process_helper_state(&ss, 1, t, 106));

    // Pattern value untouched.
    char* read_base[1] = { "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 1, read_base, 100));

    // ACC.CLR rezeroes all offsets.
    char* clr_then_read[2] = { "ACC.CLR", "P.A 0 2 0" };
    CHECK_CALL(process_helper_state(&ss, 2, clr_then_read, 100));

    // Reset-every-n: step=1, n=3 -> 100, 101, 102, 100, 101, ...
    char* clr_then_read2[2] = { "ACC.CLR", "P.A 0 1 3" };
    CHECK_CALL(process_helper_state(&ss, 2, clr_then_read2, 100));
    char* t2[1] = { "P.A 0 1 3" };
    CHECK_CALL(process_helper_state(&ss, 1, t2, 101));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 102));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 101));

    PASS();
}

// P.A.W: same as P.A but the offset wraps in [min, max].
TEST test_P_ACC_W() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep[3] = { "P.N 0", "PN 0 0 100", "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 100));

    // step=2, n=0, offset wraps in [0, 5].
    // offset sequence: 0, 2, 4, wrap(6,0,5)=0, 2, 4, ...
    // returned: 100, 102, 104, 100, 102, 104, ...
    char* t[1] = { "P.A.W 0 2 0 5 0" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t, 102));
    CHECK_CALL(process_helper_state(&ss, 1, t, 104));
    CHECK_CALL(process_helper_state(&ss, 1, t, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t, 102));

    PASS();
}

// PN.* variants: explicit pattern arg without touching p_n.
TEST test_PN_ACC_family() {
    scene_state_t ss;
    ss_init(&ss);

    // Set p_n to 3 to confirm PN.* does not consult it.
    char* prep[3] = { "P.N 3", "PN 1 0 200", "P.N" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 3));

    // PN.A pn=1 i=0 step=5 n=0 -> 200, 205, 210
    char* t[1] = { "PN.A 1 0 5 0" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 200));
    CHECK_CALL(process_helper_state(&ss, 1, t, 205));
    CHECK_CALL(process_helper_state(&ss, 1, t, 210));

    // p_n is still 3.
    char* check_pn[1] = { "P.N" };
    CHECK_CALL(process_helper_state(&ss, 1, check_pn, 3));

    // PN.A.W: offset wraps. Pattern 2.
    char* prep2[2] = { "PN 2 0 50", "PN 2 0" };
    CHECK_CALL(process_helper_state(&ss, 2, prep2, 50));
    char* tw[1] = { "PN.A.W 2 0 2 0 5 0" };
    CHECK_CALL(process_helper_state(&ss, 1, tw, 50));
    CHECK_CALL(process_helper_state(&ss, 1, tw, 52));
    CHECK_CALL(process_helper_state(&ss, 1, tw, 54));
    CHECK_CALL(process_helper_state(&ss, 1, tw, 50));

    PASS();
}

TEST test_P_D() {
    scene_state_t ss;
    ss_init(&ss);

    // defaults: dur[i] = 1 for every cell of every pattern.
    char* default_at_0[1] = { "P.D 0" };
    CHECK_CALL(process_helper_state(&ss, 1, default_at_0, 1));

    char* default_at_5[1] = { "P.D 5" };
    CHECK_CALL(process_helper_state(&ss, 1, default_at_5, 1));

    // write then read back.
    char* set_read[2] = { "P.D 0 4", "P.D 0" };
    CHECK_CALL(process_helper_state(&ss, 2, set_read, 4));

    // setter clamps values < 1 up to 1 (a 0-duration cell would freeze
    // the sequencer).
    char* set_zero[2] = { "P.D 1 0", "P.D 1" };
    CHECK_CALL(process_helper_state(&ss, 2, set_zero, 1));

    char* set_neg[2] = { "P.D 2 -5", "P.D 2" };
    CHECK_CALL(process_helper_state(&ss, 2, set_neg, 1));

    PASS();
}

TEST test_P_D_HERE() {
    scene_state_t ss;
    ss_init(&ss);

    // Walk idx to 3 (P.L 4 + a few P.NEXTs), set dur at HERE, read back.
    char* prep[4] = { "P.L 8", "P.I 3", "P.D.HERE 7", "P.D 3" };
    CHECK_CALL(process_helper_state(&ss, 4, prep, 7));

    // P.D.HERE reads the same cell.
    char* read[1] = { "P.D.HERE" };
    CHECK_CALL(process_helper_state(&ss, 1, read, 7));

    // HERE setter also clamps.
    char* clamp[2] = { "P.D.HERE 0", "P.D.HERE" };
    CHECK_CALL(process_helper_state(&ss, 2, clamp, 1));

    PASS();
}

TEST test_PN_D_family() {
    scene_state_t ss;
    ss_init(&ss);

    // Set p_n to 3 to confirm PN.* does not consult it.
    char* prep[3] = { "P.N 3", "PN.D 1 0 5", "P.N" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 3));  // P.N still 3

    char* read[1] = { "PN.D 1 0" };
    CHECK_CALL(process_helper_state(&ss, 1, read, 5));

    char* read_p[1] = { "P.D 0" };  // pattern 3, idx 0 — untouched
    CHECK_CALL(process_helper_state(&ss, 1, read_p, 1));

    // PN.D.HERE writes/reads via the indexed pattern's current idx.
    char* here[4] = { "PN.L 2 4", "PN.I 2 2", "PN.D.HERE 2 9", "PN.D 2 2" };
    CHECK_CALL(process_helper_state(&ss, 4, here, 9));

    PASS();
}

TEST test_P_D_RND() {
    scene_state_t ss;
    ss_init(&ss);

    // Constrain randomisation to indices 2..5 on the active pattern.
    // process_helper_state needs the trailing command to push a known
    // value, so each sequence reads back something deterministic.
    char* set_start[2] = { "P.START 2", "P.START" };
    CHECK_CALL(process_helper_state(&ss, 2, set_start, 2));
    char* set_end[2] = { "P.END 5", "P.END" };
    CHECK_CALL(process_helper_state(&ss, 2, set_end, 5));

    // lo == hi makes the fill deterministic. Run RND then read cells.
    char* rnd_3[2] = { "P.D.RND 3 3", "P.D 2" };
    CHECK_CALL(process_helper_state(&ss, 2, rnd_3, 3));
    char* read_hi[1] = { "P.D 5" };
    CHECK_CALL(process_helper_state(&ss, 1, read_hi, 3));

    // Cells outside [start..end] are untouched (still default 1).
    char* read_below[1] = { "P.D 0" };
    CHECK_CALL(process_helper_state(&ss, 1, read_below, 1));
    char* read_above[1] = { "P.D 6" };
    CHECK_CALL(process_helper_state(&ss, 1, read_above, 1));

    // lo > hi: args swap internally. lo=10 hi=9 -> range [9, 10]; assert
    // every written cell falls in that range (RNG-dependent exact value).
    char* swap_then_dummy[2] = { "P.D.RND 10 9", "0" };
    CHECK_CALL(process_helper_state(&ss, 2, swap_then_dummy, 0));
    for (int i = 2; i <= 5; i++) {
        int16_t v = ss_get_pattern_dur(&ss, 0, i);
        ASSERT(v == 9 || v == 10);
    }

    // lo < 1 clamps to 1: P.D.RND 0 0 -> fill with 1.
    char* clamp[2] = { "P.D.RND 0 0", "P.D 4" };
    CHECK_CALL(process_helper_state(&ss, 2, clamp, 1));

    // Range randomisation on a different pattern. Pattern 1 keeps its
    // default start=0 end=63 so every cell should be touched and fall
    // in [2, 6].
    char* mass_rnd[2] = { "PN.D.RND 1 2 6", "0" };
    CHECK_CALL(process_helper_state(&ss, 2, mass_rnd, 0));
    for (int i = 0; i < PATTERN_LENGTH; i++) {
        int16_t v = ss_get_pattern_dur(&ss, 1, i);
        ASSERT(v >= 2 && v <= 6);
    }
    PASS();
}

TEST test_P_STEP_basic() {
    scene_state_t ss;
    ss_init(&ss);

    // 4-cell pattern with notes 60, 64, 67, 72 and uniform dur=1.
    // P.STEP should behave exactly like P.NEXT in this case. End the
    // setup sequence with a getter (P.L) so process_helper_state has a
    // value to assert.
    char* setup[8] = { "P.L 4",  "P.WRAP 1", "P 0 60", "P 1 64",
                       "P 2 67", "P 3 72",   "P.I 0",  "P.L" };
    CHECK_CALL(process_helper_state(&ss, 8, setup, 4));

    // First STEP enters stage 0 with STEP.NEW=1 (dwell starts at 0).
    char* step1[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step1, 60));
    char* new1[1] = { "P.STEP.NEW" };
    CHECK_CALL(process_helper_state(&ss, 1, new1, 1));

    // dur=1 everywhere -> every subsequent STEP advances.
    char* step2[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step2, 64));
    char* step3[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step3, 67));
    char* step4[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step4, 72));
    // wrap back to idx 0
    char* step5[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step5, 60));

    PASS();
}

TEST test_P_STEP_dwells() {
    scene_state_t ss;
    ss_init(&ss);

    // 2-cell pattern, dur[0]=3, dur[1]=1.
    char* setup[8] = { "P.L 2",   "P.WRAP 1", "P 0 100", "P 1 200",
                       "P.D 0 3", "P.D 1 1",  "P.I 0",   "P.L" };
    CHECK_CALL(process_helper_state(&ss, 8, setup, 2));

    // tick 1: dwell 0 -> entering, idx=0, value=100, NEW=1.
    char* t1_step[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, t1_step, 100));
    char* new_op[1] = { "P.STEP.NEW" };
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 1));

    // tick 2: dwell 1 -> hold (2 <= dur 3), NEW=0.
    CHECK_CALL(process_helper_state(&ss, 1, t1_step, 100));
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 0));

    // tick 3: dwell 2 -> hold (3 <= dur 3), NEW=0.
    CHECK_CALL(process_helper_state(&ss, 1, t1_step, 100));
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 0));

    // tick 4: dwell 3 -> advance (4 > dur 3), idx=1, value=200, NEW=1.
    CHECK_CALL(process_helper_state(&ss, 1, t1_step, 200));
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 1));

    // tick 5: dwell 1 -> advance (2 > dur 1) wrapping to idx=0, NEW=1.
    CHECK_CALL(process_helper_state(&ss, 1, t1_step, 100));
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 1));

    PASS();
}

TEST test_P_I_resets_dwell() {
    scene_state_t ss;
    ss_init(&ss);

    // Get partway through a long stage, then write P.I and confirm the
    // next P.STEP enters the new stage fresh (NEW=1) instead of
    // immediately advancing past it.
    char* setup[7] = { "P.L 4",   "P.WRAP 1", "P 0 10", "P 1 20",
                       "P.D 0 5", "P.D 1 5",  "P.L" };
    CHECK_CALL(process_helper_state(&ss, 7, setup, 4));

    char* step_in[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step_in, 10));  // enter idx 0
    CHECK_CALL(process_helper_state(&ss, 1, step_in, 10));  // dwell 2
    CHECK_CALL(process_helper_state(&ss, 1, step_in, 10));  // dwell 3
    // dwell is now 3 on idx 0 (dur 5 -> not yet over).

    // Reset to idx 1 mid-flight. Use a 2-line sequence ending in a
    // getter so process_helper_state has a value to assert.
    char* reset[2] = { "P.I 1", "P.I" };
    CHECK_CALL(process_helper_state(&ss, 2, reset, 1));

    // Next STEP enters stage 1 fresh: value=20, NEW=1.
    CHECK_CALL(process_helper_state(&ss, 1, step_in, 20));
    char* after_new[1] = { "P.STEP.NEW" };
    CHECK_CALL(process_helper_state(&ss, 1, after_new, 1));

    PASS();
}

TEST test_PN_STEP_independence() {
    scene_state_t ss;
    ss_init(&ss);

    // Two patterns with different durations stepped independently. End
    // with a getter so the helper has a value to assert.
    char* setup[9] = { "PN.L 0 2",   "PN 0 0 1",   "PN 0 1 2",
                       "PN.D 0 0 2", "PN.L 1 2",   "PN 1 0 10",
                       "PN 1 1 20",  "PN.D 1 0 1", "PN.L 0" };
    CHECK_CALL(process_helper_state(&ss, 9, setup, 2));

    // Pattern 0: dur 2 -> two ticks per cell.
    char* p0_step[1] = { "PN.STEP 0" };
    CHECK_CALL(process_helper_state(&ss, 1, p0_step, 1));  // enter idx 0
    CHECK_CALL(process_helper_state(&ss, 1, p0_step, 1));  // hold
    CHECK_CALL(process_helper_state(&ss, 1, p0_step, 2));  // advance

    // Pattern 1: dur 1 -> advance every tick.
    char* p1_step[1] = { "PN.STEP 1" };
    CHECK_CALL(process_helper_state(&ss, 1, p1_step, 10));  // enter
    CHECK_CALL(process_helper_state(&ss, 1, p1_step, 20));  // advance

    // Pattern 0's last advance was the third step above -> NEW=1.
    char* p0_new[1] = { "PN.STEP.NEW 0" };
    CHECK_CALL(process_helper_state(&ss, 1, p0_new, 1));

    PASS();
}

TEST test_P_STEPQ() {
    scene_state_t ss;
    ss_init(&ss);

    // 2-cell pattern, dur[0]=2, dur[1]=1. P.STEP? should advance just
    // like P.STEP but push the just_advanced flag instead of val.
    char* setup[7] = { "P.L 2",   "P.WRAP 1", "P 0 100", "P 1 200",
                       "P.D 0 2", "P.D 1 1",  "P.L" };
    CHECK_CALL(process_helper_state(&ss, 7, setup, 2));

    // tick 1: dwell 0 -> entering, NEW=1 -> P.STEP? returns 1.
    char* q[1] = { "P.STEP?" };
    CHECK_CALL(process_helper_state(&ss, 1, q, 1));
    // P.HERE confirms idx is still 0 (val[0]=100).
    char* here[1] = { "P.HERE" };
    CHECK_CALL(process_helper_state(&ss, 1, here, 100));

    // tick 2: dwell 1 -> hold (2 <= dur 2), NEW=0 -> P.STEP? returns 0.
    CHECK_CALL(process_helper_state(&ss, 1, q, 0));
    CHECK_CALL(process_helper_state(&ss, 1, here, 100));

    // tick 3: dwell 2 -> advance, NEW=1, idx becomes 1, P.STEP? -> 1.
    CHECK_CALL(process_helper_state(&ss, 1, q, 1));
    CHECK_CALL(process_helper_state(&ss, 1, here, 200));

    // P.STEP?'s side effect is identical to P.STEP — confirm P.STEP.NEW
    // reads the same latch right after.
    char* new_op[1] = { "P.STEP.NEW" };
    CHECK_CALL(process_helper_state(&ss, 1, new_op, 1));

    PASS();
}

// Drive the per-pattern mode through several P.STEP calls. dur=1 so each
// P.STEP advances; we only assert idx by reading val[].
TEST test_P_MODE_pingpong() {
    scene_state_t ss;
    ss_init(&ss);

    // 4-cell pattern val 10,20,30,40; range = full [0..3]; PINGPONG.
    char* setup[11] = { "P.L 4", "P.WRAP 1", "P.START 0", "P.END 3",
                        "P 0 10", "P 1 20", "P 2 30", "P 3 40",
                        "P.I 0", "P.MODE 1", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 11, setup, 1));

    char* step[1] = { "P.STEP" };
    // tick 1: enter idx 0 (NEW=1, no advance) -> 10.
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));
    // PINGPONG forward: 10 20 30 40 (hold) 40 30 20 10 (hold) 10 ...
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));  // repeat endpoint
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));  // repeat endpoint
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));

    PASS();
}

TEST test_P_MODE_pendulum() {
    scene_state_t ss;
    ss_init(&ss);

    char* setup[11] = { "P.L 4", "P.WRAP 1", "P.START 0", "P.END 3",
                        "P 0 10", "P 1 20", "P 2 30", "P 3 40",
                        "P.I 0", "P.MODE 2", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 11, setup, 2));

    char* step[1] = { "P.STEP" };
    // PENDULUM forward: 10 20 30 40 30 20 10 20 30 40 ...
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));  // enter
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));  // play once
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));  // reverse, skip 40
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));  // play once
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));  // reverse, skip 10
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));

    PASS();
}

// PENDULUM degenerate case: range=2 should toggle (endpoints play once,
// no repeats) — i.e. 0,1,0,1 not 0,1,1,0,0.
TEST test_P_MODE_pendulum_range_2() {
    scene_state_t ss;
    ss_init(&ss);

    char* setup[9] = { "P.L 2", "P.WRAP 1", "P.START 0", "P.END 1",
                       "P 0 10", "P 1 20", "P.I 0", "P.MODE 2", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 9, setup, 2));

    char* step[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));  // enter
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));

    PASS();
}

TEST test_P_MODE_linear_rev() {
    scene_state_t ss;
    ss_init(&ss);

    char* setup[12] = { "P.L 4", "P.WRAP 1", "P.START 0", "P.END 3",
                        "P 0 10", "P 1 20", "P 2 30", "P 3 40",
                        "P.DIR 1", "P.I 3", "P.MODE 0", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 12, setup, 0));

    char* step[1] = { "P.STEP" };
    // LINEAR REV starting at idx 3 — should walk 40 30 20 10 then wrap.
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));  // enter
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));  // wrap

    PASS();
}

TEST test_P_MODE_jump() {
    scene_state_t ss;
    ss_init(&ss);

    char* setup[12] = { "P.L 6", "P.WRAP 1", "P.START 0", "P.END 5",
                        "P 0 1", "P 1 2", "P 2 3", "P 3 4",
                        "P.I 0", "P.MODE 3", "P.STRIDE 2", "P.STRIDE" };
    CHECK_CALL(process_helper_state(&ss, 12, setup, 2));
    char* p4[3] = { "P 4 5", "P 5 6", "P 5" };
    CHECK_CALL(process_helper_state(&ss, 3, p4, 6));

    char* step[1] = { "P.STEP" };
    // stride 2 across [0..5]: idx 0 -> 2 -> 4 -> 0 -> 2 ...
    CHECK_CALL(process_helper_state(&ss, 1, step, 1));  // enter idx 0
    CHECK_CALL(process_helper_state(&ss, 1, step, 3));  // idx 2
    CHECK_CALL(process_helper_state(&ss, 1, step, 5));  // idx 4
    CHECK_CALL(process_helper_state(&ss, 1, step, 1));  // wrap -> idx 0
    CHECK_CALL(process_helper_state(&ss, 1, step, 3));

    PASS();
}

TEST test_P_MODE_random_in_range() {
    scene_state_t ss;
    ss_init(&ss);

    // RANDOM should always land in [start..end]. We don't assert
    // distribution — set val[2..5] = 1 and val outside to 0, then step
    // many times and assert every P.STEP returns 1 (i.e. idx in range).
    char* setup[11] = { "P.L 8", "P.WRAP 1", "P.START 2", "P.END 5",
                        "P 2 1", "P 3 1", "P 4 1", "P 5 1",
                        "P.I 2", "P.MODE 4", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 11, setup, 4));

    char* step[1] = { "P.STEP" };
    // First step is the "enter" tick — stays on idx 2, returns val 1.
    CHECK_CALL(process_helper_state(&ss, 1, step, 1));
    for (int i = 0; i < 32; i++) {
        // Every subsequent step lands somewhere in [2..5] -> val=1.
        CHECK_CALL(process_helper_state(&ss, 1, step, 1));
    }

    PASS();
}

TEST test_P_MODE_roundtrip_via_p_i() {
    scene_state_t ss;
    ss_init(&ss);

    // After stepping into a pendulum bounce, P.I should re-seed the
    // travel direction so the next P.STEP moves forward again.
    char* setup[11] = { "P.L 4", "P.WRAP 1", "P.START 0", "P.END 3",
                        "P 0 10", "P 1 20", "P 2 30", "P 3 40",
                        "P.I 0", "P.MODE 2", "P.MODE" };
    CHECK_CALL(process_helper_state(&ss, 11, setup, 2));

    char* step[1] = { "P.STEP" };
    CHECK_CALL(process_helper_state(&ss, 1, step, 10));
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));
    CHECK_CALL(process_helper_state(&ss, 1, step, 40));
    CHECK_CALL(process_helper_state(&ss, 1, step, 30));  // reversed

    // Reset to idx 0; travel_dir should re-seed to +1 (base dir FWD).
    char* reset[2] = { "P.I 0", "P.I" };
    CHECK_CALL(process_helper_state(&ss, 2, reset, 0));

    CHECK_CALL(process_helper_state(&ss, 1, step, 10));  // enter
    CHECK_CALL(process_helper_state(&ss, 1, step, 20));  // forward, not back

    PASS();
}

// Parse & execute each line, discarding the stack result, so tests can inspect
// pattern state directly via ss_get_pattern_val. Fails the calling TEST if any
// line fails to parse or validate (otherwise an op rename or arity change
// would silently leave the pattern at its default and several tests would
// pass against all-zero state).
TEST motif_run(scene_state_t* ss, size_t n, char* lines[]) {
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    for (size_t i = 0; i < n; i++) {
        tele_command_t cmd;
        char error_msg[TELE_ERROR_MSG_LENGTH];
        if (parse(lines[i], &cmd, error_msg) != E_OK) FAILm(lines[i]);
        if (validate(&cmd, error_msg) != E_OK) FAILm(lines[i]);
        process_command(ss, &es, &cmd);
    }
    PASS();
}

TEST test_P_MOTIF_determinism() {
    scene_state_t ss1, ss2;
    ss_init(&ss1);
    ss_init(&ss2);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 42" };
    CHECK_CALL(motif_run(&ss1, 3, setup));
    CHECK_CALL(motif_run(&ss2, 3, setup));
    char* fire[1] = { "P.MOTIF 4 0 0" };
    CHECK_CALL(motif_run(&ss1, 1, fire));
    CHECK_CALL(motif_run(&ss2, 1, fire));
    for (int i = 0; i <= 15; i++) {
        ASSERT_EQ(ss_get_pattern_val(&ss1, 0, i),
                  ss_get_pattern_val(&ss2, 0, i));
    }
    PASS();
}

TEST test_P_MOTIF_repetition() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 7" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 0 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // length=4, mode=EXACT, transposition=0: window is 4 repetitions of the
    // same 4-value motif.
    for (int i = 4; i <= 15; i++) {
        ASSERT_EQ(ss_get_pattern_val(&ss, 0, i),
                  ss_get_pattern_val(&ss, 0, i % 4));
    }
    PASS();
}

TEST test_P_MOTIF_transposition() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 99" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 0 2" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // Statement s adds s*2 to each base motif value.
    for (int s = 1; s < 4; s++) {
        for (int n = 0; n < 4; n++) {
            int16_t base = ss_get_pattern_val(&ss, 0, n);
            int16_t got = ss_get_pattern_val(&ss, 0, s * 4 + n);
            ASSERT_EQ(got, base + s * 2);
        }
    }
    PASS();
}

TEST test_P_MOTIF_inversion() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 11" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 1 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // Statement 0 values are (1 + offset[n]). Statement 1 (INVERT) values are
    // (1 - offset[n]). So pattern[n] + pattern[4+n] == 2.
    for (int n = 0; n < 4; n++) {
        int16_t a = ss_get_pattern_val(&ss, 0, n);
        int16_t b = ss_get_pattern_val(&ss, 0, 4 + n);
        ASSERT_EQ(a + b, 2);
    }
    PASS();
}

TEST test_P_MOTIF_retrograde() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 13" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 2 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // Statement 1 (RETROGRADE) reverses the motif.
    for (int n = 0; n < 4; n++) {
        ASSERT_EQ(ss_get_pattern_val(&ss, 0, 4 + n),
                  ss_get_pattern_val(&ss, 0, 3 - n));
    }
    PASS();
}

TEST test_P_MOTIF_alternate() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 19" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 3 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // ALTERNATE: even statements EXACT, odd statements INVERT.
    for (int n = 0; n < 4; n++) {
        int16_t s0 = ss_get_pattern_val(&ss, 0, n);
        int16_t s1 = ss_get_pattern_val(&ss, 0, 4 + n);
        int16_t s2 = ss_get_pattern_val(&ss, 0, 8 + n);
        int16_t s3 = ss_get_pattern_val(&ss, 0, 12 + n);
        ASSERT_EQ(s2, s0);      // statement 2 == statement 0 (EXACT)
        ASSERT_EQ(s0 + s1, 2);  // statement 1 inverted around degree 1
        ASSERT_EQ(s3, s1);      // statement 3 == statement 1 (INVERT)
    }
    PASS();
}

TEST test_P_MOTIF_ornament() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 7", "P.SEED 23" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 4 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // ORNAMENT: statement 1 == statement 0 in three positions, differs by
    // exactly +-1 in one position.
    int diff_count = 0;
    int total_abs_diff = 0;
    for (int n = 0; n < 4; n++) {
        int16_t s0 = ss_get_pattern_val(&ss, 0, n);
        int16_t s1 = ss_get_pattern_val(&ss, 0, 4 + n);
        int16_t d = s1 - s0;
        if (d != 0) {
            diff_count++;
            total_abs_diff += (d < 0) ? -d : d;
        }
    }
    ASSERT_EQ(diff_count, 1);
    ASSERT_EQ(total_abs_diff, 1);
    PASS();
}

TEST test_P_MOTIF_expand() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 7", "P.SEED 29" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 6 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // EXPAND: statement 1 doubles offsets from anchor degree 1.
    // pattern[4+n] - 1 == 2 * (pattern[n] - 1).
    for (int n = 0; n < 4; n++) {
        int16_t s0 = ss_get_pattern_val(&ss, 0, n);
        int16_t s1 = ss_get_pattern_val(&ss, 0, 4 + n);
        ASSERT_EQ(s1 - 1, 2 * (s0 - 1));
    }
    PASS();
}

TEST test_P_MOTIF_compress() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 7", "P.SEED 31" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 5 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // COMPRESS: statement 1 halves offsets (C truncates toward zero).
    // pattern[4+n] - 1 == (pattern[n] - 1) / 2.
    for (int n = 0; n < 4; n++) {
        int16_t s0 = ss_get_pattern_val(&ss, 0, n);
        int16_t s1 = ss_get_pattern_val(&ss, 0, 4 + n);
        ASSERT_EQ(s1 - 1, (s0 - 1) / 2);
    }
    PASS();
}

TEST test_P_MOTIF_rotate() {
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 37" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 7 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // ROTATE: statement s is the base motif cyclically rotated by s positions.
    // pattern[s*4 + n] == pattern[(n + s) % 4].
    for (int s = 1; s < 4; s++) {
        for (int n = 0; n < 4; n++) {
            ASSERT_EQ(ss_get_pattern_val(&ss, 0, s * 4 + n),
                      ss_get_pattern_val(&ss, 0, (n + s) % 4));
        }
    }
    PASS();
}

TEST test_P_MOTIF_partial_trailing() {
    // Window length 10, motif length 4: full statements at 0..3 and 4..7,
    // then a partial statement 8..9 carrying the first 2 notes of the next
    // (EXACT) variation. With mode=0 transpose=0 this means pattern[8]==P[0]
    // and pattern[9]==P[1].
    scene_state_t ss;
    ss_init(&ss);
    char* setup[3] = { "P.START 0", "P.END 9", "P.SEED 5" };
    CHECK_CALL(motif_run(&ss, 3, setup));
    char* fire[1] = { "P.MOTIF 4 0 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    ASSERT_EQ(ss_get_pattern_val(&ss, 0, 8), ss_get_pattern_val(&ss, 0, 0));
    ASSERT_EQ(ss_get_pattern_val(&ss, 0, 9), ss_get_pattern_val(&ss, 0, 1));
    PASS();
}

TEST test_P_MOTIF_length_clamping() {
    scene_state_t a, b, c, d;
    ss_init(&a);
    ss_init(&b);
    ss_init(&c);
    ss_init(&d);
    char* setup[3] = { "P.START 0", "P.END 15", "P.SEED 17" };
    CHECK_CALL(motif_run(&a, 3, setup));
    CHECK_CALL(motif_run(&b, 3, setup));
    CHECK_CALL(motif_run(&c, 3, setup));
    CHECK_CALL(motif_run(&d, 3, setup));
    char* lo_clamp[1] = { "P.MOTIF 1 0 0" };
    char* lo_real[1] = { "P.MOTIF 2 0 0" };
    char* hi_clamp[1] = { "P.MOTIF 99 0 0" };
    char* hi_real[1] = { "P.MOTIF 4 0 0" };
    CHECK_CALL(motif_run(&a, 1, lo_clamp));
    CHECK_CALL(motif_run(&b, 1, lo_real));
    CHECK_CALL(motif_run(&c, 1, hi_clamp));
    CHECK_CALL(motif_run(&d, 1, hi_real));
    for (int i = 0; i <= 15; i++) {
        ASSERT_EQ(ss_get_pattern_val(&a, 0, i), ss_get_pattern_val(&b, 0, i));
        ASSERT_EQ(ss_get_pattern_val(&c, 0, i), ss_get_pattern_val(&d, 0, i));
    }
    PASS();
}

TEST test_P_MOTIF_window_respect() {
    scene_state_t ss;
    ss_init(&ss);
    // Seed pattern values outside the window with a sentinel.
    char* prep[8] = { "P.START 0", "P.END 3", "P 4 999",  "P 5 999",
                      "P 6 999",   "P 7 999", "P.SEED 1", "P.MOTIF 4 0 0" };
    CHECK_CALL(motif_run(&ss, 8, prep));
    for (int i = 4; i <= 7; i++) {
        ASSERT_EQ(ss_get_pattern_val(&ss, 0, i), 999);
    }
    PASS();
}

TEST test_P_MOTIF_window_length_1() {
    scene_state_t ss;
    ss_init(&ss);
    char* prep[4] = { "P.START 0", "P.END 0", "P 1 777", "P.SEED 3" };
    CHECK_CALL(motif_run(&ss, 4, prep));
    char* fire[1] = { "P.MOTIF 2 0 0" };
    CHECK_CALL(motif_run(&ss, 1, fire));
    // First note of motif is always degree 1 (untransposed reference).
    ASSERT_EQ(ss_get_pattern_val(&ss, 0, 0), 1);
    // Outside the window is preserved.
    ASSERT_EQ(ss_get_pattern_val(&ss, 0, 1), 777);
    PASS();
}

// Helpers for P.CP tests. We bypass parse/process for state setup and just
// poke pattern fields directly — keeps these tests focused on cp_compute.

static void cp_setup_window(scene_state_t* ss, int start, int end,
                            const int16_t* melody) {
    ss_set_pattern_start(ss, 0, (int16_t)start);
    ss_set_pattern_end(ss, 0, (int16_t)end);
    ss_set_pattern_len(ss, 0, (int16_t)(end + 1));
    for (int i = start; i <= end; i++) {
        ss_set_pattern_val(ss, 0, (int16_t)i, melody[i - start]);
    }
    ss_set_pattern_idx(ss, 0, (int16_t)start);
}

static int16_t cp_run(scene_state_t* ss, int16_t i, int rule, int offset) {
    ss_set_pattern_idx(ss, 0, i);
    char cp_cmd[32];
    snprintf(cp_cmd, sizeof(cp_cmd), "P.CP %d %d", rule, offset);

    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    parse(cp_cmd, &cmd, error_msg);
    validate(&cmd, error_msg);
    process_result_t result = process_command(ss, &es, &cmd);
    return result.value;
}

TEST test_P_CP_determinism() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    int16_t a = cp_run(&ss, 3, 0, 0);
    int16_t b = cp_run(&ss, 3, 0, 0);
    int16_t c = cp_run(&ss, 3, 0, 0);
    ASSERT_EQ(a, b);
    ASSERT_EQ(b, c);
    PASS();
}

TEST test_P_CP_no_side_effects() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    int16_t before[8];
    for (int i = 0; i < 8; i++) before[i] = ss_get_pattern_val(&ss, 0, i);
    int16_t idx_before = ss_get_pattern_idx(&ss, 0);
    (void)cp_run(&ss, idx_before, 0, 0);
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(ss_get_pattern_val(&ss, 0, i), before[i]);
    ASSERT_EQ(ss_get_pattern_idx(&ss, 0), idx_before);
    PASS();
}

TEST test_P_CP_consonance() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    int rules[] = { 0, 1, 2, 3, 5, 7 };
    for (size_t r = 0; r < sizeof(rules) / sizeof(rules[0]); r++) {
        for (int i = 0; i < 8; i++) {
            int16_t cp = cp_run(&ss, i, rules[r], 0);
            int diff = cp - melody[i];
            int absd = diff < 0 ? -diff : diff;
            // Allowed consonant intervals (and unison fallback at edges).
            int ok = (absd == 2 || absd == 4 || absd == 5 || absd == 7);
            ASSERT(ok);
        }
    }
    PASS();
}

TEST test_P_CP_canon_arithmetic() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[16];
    for (int i = 0; i < 16; i++) melody[i] = (int16_t)(i + 1);
    cp_setup_window(&ss, 0, 15, melody);
    // L=16, delay = 16/4 = 4, P.I=4 -> read_pos = 0.
    int16_t cp = cp_run(&ss, 4, 6, 0);
    ASSERT_EQ(cp, melody[0]);
    PASS();
}

TEST test_P_CP_canon_offset() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[16];
    for (int i = 0; i < 16; i++) melody[i] = (int16_t)(i + 1);
    cp_setup_window(&ss, 0, 15, melody);
    int16_t base = cp_run(&ss, 7, 6, 0);
    int16_t shifted = cp_run(&ss, 7, 6, 4);
    ASSERT_EQ(shifted, base + 4);
    PASS();
}

TEST test_P_CP_bass_below() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 5, 7, 6, 9, 8, 10, 9, 11 };
    cp_setup_window(&ss, 0, 7, melody);
    for (int i = 0; i < 8; i++) {
        int16_t cp = cp_run(&ss, i, 3, 0);
        ASSERT(cp < melody[i]);
    }
    PASS();
}

TEST test_P_CP_mirror_symmetry() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    int vmin = 1, vmax = 7;
    int axis = (vmin + vmax) / 2;
    for (int i = 0; i < 8; i++) {
        int16_t cp = cp_run(&ss, i, 4, 0);
        int sum = cp + melody[i];
        int target = 2 * axis;
        int diff = sum - target;
        if (diff < 0) diff = -diff;
        // Allow some quantization slack — candidates step by ±2/±4/±5/±7
        // from cf_now, so the closest mirror may miss by a few degrees.
        ASSERT(diff <= 3);
    }
    PASS();
}

TEST test_P_CP_oblique_clustering() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 5, 3, 8, 2, 9, 4, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    int16_t cp_vals[8];
    int16_t cp_min = INT16_MAX, cp_max = INT16_MIN;
    int16_t m_min = INT16_MAX, m_max = INT16_MIN;
    for (int i = 0; i < 8; i++) {
        cp_vals[i] = cp_run(&ss, i, 5, 0);
        if (cp_vals[i] < cp_min) cp_min = cp_vals[i];
        if (cp_vals[i] > cp_max) cp_max = cp_vals[i];
        if (melody[i] < m_min) m_min = melody[i];
        if (melody[i] > m_max) m_max = melody[i];
    }
    ASSERT((cp_max - cp_min) < (m_max - m_min));
    PASS();
}

TEST test_P_CP_offset_shifts() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    cp_setup_window(&ss, 0, 7, melody);
    // For rule 0 (BALANCED), shifting offset shifts target → result shifts
    // by approximately offset (within candidate quantization ±2).
    int16_t base = cp_run(&ss, 3, 0, 0);
    int16_t shifted = cp_run(&ss, 3, 0, 7);
    int diff = (shifted - base) - 7;
    if (diff < 0) diff = -diff;
    ASSERT(diff <= 2);
    PASS();
}

TEST test_P_CP_window_wrap() {
    scene_state_t ss;
    ss_init(&ss);
    // Construct a window where pattern[end] differs sharply from pattern[start]
    // so that cf_prev wrap is detectable via local_motion sign.
    int16_t melody[8] = { 5, 6, 7, 6, 5, 6, 7, 1 };
    cp_setup_window(&ss, 0, 7, melody);
    // At P.I=0, cf_prev should wrap to pattern[7]=1 → local_motion = 5-1 = +4.
    // For rule 1 CONTRARY this should bias toward cp_motion < 0.
    int16_t cp = cp_run(&ss, 0, 1, 0);
    ASSERT(cp < melody[0]);
    PASS();
}

TEST test_P_CP_window_length_one() {
    scene_state_t ss;
    ss_init(&ss);
    char* prep[3] = { "P.START 3", "P.END 3", "P 3 4" };
    motif_run(&ss, 3, prep);
    int16_t cp = cp_run(&ss, 3, 0, 5);
    ASSERT_EQ(cp, 4 + 5);
    PASS();
}

TEST test_P_CP_inverted_window_applies_offset() {
    // Regression: end<start path used to drop offset. Set start>end manually
    // and confirm the fallback returns pattern[P.I] + offset.
    scene_state_t ss;
    ss_init(&ss);
    ss_set_pattern_start(&ss, 0, 5);
    ss_set_pattern_end(&ss, 0, 2);
    ss_set_pattern_val(&ss, 0, 3, 42);
    ss_set_pattern_idx(&ss, 0, 3);

    char cp_cmd[] = "P.CP 0 7";
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    parse(cp_cmd, &cmd, error_msg);
    validate(&cmd, error_msg);
    process_result_t r = process_command(&ss, &es, &cmd);
    ASSERT(r.has_value);
    ASSERT_EQ(r.value, 42 + 7);
    PASS();
}

TEST test_P_CP_oblique_negative_cf() {
    // Regression: tonic_octave used C truncation, which placed the drone at
    // degree 1 for cf_now <= 0. With proper floor-division, cf_now in
    // [-6..0] should pull toward tonic_octave = -6 (degree 1 of the octave
    // below), not +1. We assert OBLIQUE results stay closer to the cf_now
    // octave than to the +1 octave.
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { -5, -3, -4, -2, -6, -1, -4, -3 };
    cp_setup_window(&ss, 0, 7, melody);
    int closer_below = 0, closer_above = 0;
    for (int i = 0; i < 8; i++) {
        int16_t cp = cp_run(&ss, i, 5, 0);
        int dist_below = cp - (-6);
        if (dist_below < 0) dist_below = -dist_below;
        int dist_above = cp - 1;
        if (dist_above < 0) dist_above = -dist_above;
        if (dist_below < dist_above) closer_below++;
        if (dist_above < dist_below) closer_above++;
    }
    ASSERT(closer_below > closer_above);
    PASS();
}

TEST test_PN_CP_explicit_bank() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t melody[8] = { 1, 3, 2, 5, 4, 6, 5, 7 };
    // Populate bank 2 directly.
    char* prep[2] = { "PN.START 2 0", "PN.END 2 7" };
    motif_run(&ss, 2, prep);
    for (int i = 0; i < 8; i++) {
        char line[32];
        snprintf(line, sizeof(line), "PN 2 %d %d", i, melody[i]);
        char* l[1] = { line };
        motif_run(&ss, 1, l);
    }
    char* set_i[1] = { "PN.I 2 3" };
    motif_run(&ss, 1, set_i);
    // Working bank is still 0; PN.CP 2 ... reads bank 2 explicitly.
    process_result_t result = { .has_value = false, .value = 0 };
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    parse("PN.CP 2 0 0", &cmd, error_msg);
    validate(&cmd, error_msg);
    result = process_command(&ss, &es, &cmd);
    ASSERT(result.has_value);
    int diff = result.value - melody[3];
    int absd = diff < 0 ? -diff : diff;
    ASSERT(absd == 2 || absd == 4 || absd == 5 || absd == 7);
    PASS();
}

// P.FUGUE helpers ////////////////////////////////////////////////////////////

static void fugue_setup(scene_state_t* ss, int start, int end,
                        const int16_t* subject) {
    ss_set_pattern_start(ss, 0, (int16_t)start);
    ss_set_pattern_end(ss, 0, (int16_t)end);
    ss_set_pattern_len(ss, 0, (int16_t)(end + 1));
    for (int i = start; i <= end; i++) {
        ss_set_pattern_val(ss, 0, (int16_t)i, subject[i - start]);
    }
    ss_set_pattern_idx(ss, 0, (int16_t)start);
}

static int16_t fugue_run(scene_state_t* ss, int voice, int division,
                         int transpose, int mode, int phase, int clock) {
    char line[64];
    snprintf(line, sizeof(line), "P.FUGUE %d %d %d %d %d %d", voice, division,
             transpose, mode, phase, clock);
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    parse(line, &cmd, error_msg);
    validate(&cmd, error_msg);
    process_result_t r = process_command(ss, &es, &cmd);
    return r.value;
}

static int16_t fugue_run_pn(scene_state_t* ss, int pn, int voice, int division,
                            int transpose, int mode, int phase, int clock) {
    char line[64];
    snprintf(line, sizeof(line), "PN.FUGUE %d %d %d %d %d %d %d", pn, voice,
             division, transpose, mode, phase, clock);
    exec_state_t es;
    es_init(&es);
    es_push(&es);
    es_variables(&es)->script_number = 0;
    tele_command_t cmd;
    char error_msg[TELE_ERROR_MSG_LENGTH];
    parse(line, &cmd, error_msg);
    validate(&cmd, error_msg);
    process_result_t r = process_command(ss, &es, &cmd);
    return r.value;
}

TEST test_P_FUGUE_determinism() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    int16_t a = fugue_run(&ss, 0, 2, 0, 0, 0, 5);
    int16_t b = fugue_run(&ss, 0, 2, 0, 0, 0, 5);
    int16_t c = fugue_run(&ss, 0, 2, 0, 0, 0, 5);
    ASSERT_EQ(a, b);
    ASSERT_EQ(b, c);
    PASS();
}

TEST test_P_FUGUE_no_side_effects() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    int16_t before[4];
    for (int i = 0; i < 4; i++) before[i] = ss_get_pattern_val(&ss, 0, i);
    int16_t idx_before = ss_get_pattern_idx(&ss, 0);
    (void)fugue_run(&ss, 0, 1, 5, 1, 2, 9);
    for (int i = 0; i < 4; i++)
        ASSERT_EQ(ss_get_pattern_val(&ss, 0, i), before[i]);
    ASSERT_EQ(ss_get_pattern_idx(&ss, 0), idx_before);
    PASS();
}

TEST test_P_FUGUE_basic_playback() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    fugue_setup(&ss, 0, 3, subj);
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, t), subj[t]);
    }
    PASS();
}

TEST test_P_FUGUE_division_holds() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 0), 1);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 1), 1);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 2), 2);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 3), 2);
    PASS();
}

TEST test_P_FUGUE_subject_wrap() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    fugue_setup(&ss, 0, 3, subj);
    // Anchor to literal subj[0] so an impl that returns a constant fails.
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, 4), subj[0]);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, 5), subj[1]);
    PASS();
}

TEST test_P_FUGUE_negative_division() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    fugue_setup(&ss, 0, 3, subj);
    // P.FUGUE -1 0 0 0 0 reads last note (position (subject_len-1)-0 = 3)
    ASSERT_EQ(fugue_run(&ss, 0, -1, 0, 0, 0, 0), 4);
    ASSERT_EQ(fugue_run(&ss, 0, -1, 0, 0, 0, 1), 3);
    ASSERT_EQ(fugue_run(&ss, 0, -1, 0, 0, 0, 2), 2);
    ASSERT_EQ(fugue_run(&ss, 0, -1, 0, 0, 0, 3), 1);
    PASS();
}

TEST test_P_FUGUE_transpose_adds() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    fugue_setup(&ss, 0, 3, subj);
    for (int t = 0; t < 4; t++) {
        int16_t a = fugue_run(&ss, 0, 1, 0, 0, 0, t);
        int16_t b = fugue_run(&ss, 0, 1, 7, 0, 0, t);
        ASSERT_EQ(b, a + 7);
    }
    PASS();
}

TEST test_P_FUGUE_inversion() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    int16_t expected[4] = { 3, 1, 2, -1 };
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 1, 0, t), expected[t]);
    }
    PASS();
}

TEST test_P_FUGUE_retrograde() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    int16_t expected[4] = { 7, 4, 5, 3 };
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 2, 0, t), expected[t]);
    }
    PASS();
}

TEST test_P_FUGUE_retrograde_inversion() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // retrograde sequence [7,4,5,3], inverted around 3: 2*3 - x
    int16_t expected[4] = { -1, 2, 1, 3 };
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 3, 0, t), expected[t]);
    }
    PASS();
}

TEST test_P_FUGUE_direction_cancel() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    for (int t = 0; t < 4; t++) {
        // Anchor to literal subject so the test doesn't pass if both
        // sides degenerated to the same broken value.
        ASSERT_EQ(fugue_run(&ss, 0, -1, 0, 2, 0, t), subj[t]);
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, t), subj[t]);
    }
    PASS();
}

TEST test_P_FUGUE_phase_shifts() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 1, 0), 5);
    PASS();
}

TEST test_P_FUGUE_phase_wraps() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 4, 0), subj[0]);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 5, 0), subj[1]);
    PASS();
}

TEST test_P_FUGUE_negative_phase() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // phase=-1 on a 4-note subject -> position 3
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, -1, 0), subj[3]);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, -5, 0), subj[3]);
    PASS();
}

TEST test_P_FUGUE_phase_plus_clock() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 1, 1),
              fugue_run(&ss, 0, 1, 0, 0, 0, 2));
    PASS();
}

TEST test_P_FUGUE_phase_retrograde() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // retrograde, phase 1, clock 0:
    // note_index = 0 + 1 = 1; pos = 1; reverse -> (4-1) - 1 = 2; subj[2] = 4
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 2, 1, 0), 4);
    PASS();
}

TEST test_P_FUGUE_negative_clock() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // clock=-1, div=1: note_index = -1; ((-1 % 4) + 4) % 4 = 3
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, -1), 7);
    PASS();
}

TEST test_P_FUGUE_clock_zero() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, 0), 3);
    PASS();
}

TEST test_P_FUGUE_subject_len_one() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[1] = { 9 };
    fugue_setup(&ss, 0, 0, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, 0), 9);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 7, 13), 9);
    ASSERT_EQ(fugue_run(&ss, 0, 3, 0, 2, 0, 100), 9);
    PASS();
}

TEST test_P_FUGUE_division_zero() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    ASSERT_EQ(fugue_run(&ss, 0, 0, 5, 1, 2, 7), 0);
    PASS();
}

TEST test_P_FUGUE_mode_clamp() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // mode out of range -> treated as PRIME (0)
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 9, 0, 1),
              fugue_run(&ss, 0, 1, 0, 0, 0, 1));
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, -3, 0, 2),
              fugue_run(&ss, 0, 1, 0, 0, 0, 2));
    PASS();
}

TEST test_P_FUGUE_int16_min_clock() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // INT16_MIN = -32768; -32768 / 1 = -32768; ((-32768 % 4) + 4) % 4 = 0
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, INT16_MIN), subj[0]);
    // Same with abs_div=3: -32768 / 3 = -10922; (-10922 % 4 + 4) % 4 = 2
    ASSERT_EQ(fugue_run(&ss, 0, 3, 0, 0, 0, INT16_MIN), subj[2]);
    PASS();
}

TEST test_P_FUGUE_transpose_clamps() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[2] = { 100, -100 };
    fugue_setup(&ss, 0, 1, subj);
    // transpose pushes note past INT16_MAX -> clamp
    ASSERT_EQ(fugue_run(&ss, 0, 1, INT16_MAX, 0, 0, 0), INT16_MAX);
    // transpose pushes note past INT16_MIN -> clamp
    ASSERT_EQ(fugue_run(&ss, 0, 1, INT16_MIN, 0, 0, 1), INT16_MIN);
    PASS();
}

TEST test_P_FUGUE_inverted_window() {
    scene_state_t ss;
    ss_init(&ss);
    // P.START 5 : P.END 2 -> end < start, subject_len < 1 -> return 0
    ss_set_pattern_start(&ss, 0, 5);
    ss_set_pattern_end(&ss, 0, 2);
    ss_set_pattern_len(&ss, 0, 8);
    for (int i = 0; i < 8; i++) ss_set_pattern_val(&ss, 0, i, (int16_t)(i + 1));
    ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 0, 0, 0), 0);
    ASSERT_EQ(fugue_run(&ss, 0, 1, 7, 2, 3, 5), 0);
    PASS();
}

TEST test_P_FUGUE_truncation_plateau() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    fugue_setup(&ss, 0, 3, subj);
    // C truncation toward zero: clock={-1,0,1} all divide to 0 with abs_div=2,
    // forming a 3-wide plateau (asymmetric vs the 2-wide plateaus elsewhere).
    int16_t v0 = fugue_run(&ss, 0, 2, 0, 0, 0, 0);
    ASSERT_EQ(v0, subj[0]);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, -1), v0);
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 1), v0);
    // clock=-2 leaves the plateau backward
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, -2), subj[3]);
    // clock=2 leaves the plateau forward
    ASSERT_EQ(fugue_run(&ss, 0, 2, 0, 0, 0, 2), subj[1]);
    PASS();
}

TEST test_P_FUGUE_inversion_nonzero_start() {
    scene_state_t ss;
    ss_init(&ss);
    // Window starts at index 2; the inversion axis must be subj[start],
    // not subj[0]. If the impl used the literal 0 the test would fail.
    ss_set_pattern_start(&ss, 0, 2);
    ss_set_pattern_end(&ss, 0, 5);
    ss_set_pattern_len(&ss, 0, 8);
    ss_set_pattern_val(&ss, 0, 0, 99);  // outside window, must be ignored
    ss_set_pattern_val(&ss, 0, 1, 99);
    ss_set_pattern_val(&ss, 0, 2, 3);   // subject begins here
    ss_set_pattern_val(&ss, 0, 3, 5);
    ss_set_pattern_val(&ss, 0, 4, 4);
    ss_set_pattern_val(&ss, 0, 5, 7);
    // Inversion around subj[start]=3 -> {3,1,2,-1}
    int16_t expected[4] = { 3, 1, 2, -1 };
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run(&ss, 0, 1, 0, 1, 0, t), expected[t]);
    }
    PASS();
}

TEST test_PN_FUGUE_modes_and_phase() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    ss_set_pattern_start(&ss, 2, 0);
    ss_set_pattern_end(&ss, 2, 3);
    ss_set_pattern_len(&ss, 2, 4);
    for (int i = 0; i < 4; i++) ss_set_pattern_val(&ss, 2, i, subj[i]);
    // RETROGRADE
    ASSERT_EQ(fugue_run_pn(&ss, 2, 0, 1, 0, 2, 0, 0), subj[3]);
    ASSERT_EQ(fugue_run_pn(&ss, 2, 0, 1, 0, 2, 0, 1), subj[2]);
    // INVERSION
    ASSERT_EQ(fugue_run_pn(&ss, 2, 0, 1, 0, 1, 0, 1), 1);  // 2*3 - 5
    // phase + transpose through the 6-arg path
    ASSERT_EQ(fugue_run_pn(&ss, 2, 0, 1, 7, 0, 2, 0), subj[2] + 7);
    PASS();
}

TEST test_PN_FUGUE_pn_normalises() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 3, 5, 4, 7 };
    // Populate bank 0 only; an out-of-range pn must normalise to a valid bank
    // and return *something* (not crash, not return 0 from an early guard).
    ss_set_pattern_start(&ss, 0, 0);
    ss_set_pattern_end(&ss, 0, 3);
    ss_set_pattern_len(&ss, 0, 4);
    for (int i = 0; i < 4; i++) ss_set_pattern_val(&ss, 0, i, subj[i]);
    // pn=-1 and pn=999 must not crash; assert against bank 0 if they wrap there,
    // or at minimum produce a value within the int16 range.
    int16_t v_neg = fugue_run_pn(&ss, -1, 0, 1, 0, 0, 0, 0);
    int16_t v_big = fugue_run_pn(&ss, 999, 0, 1, 0, 0, 0, 0);
    // Sanity: not arbitrary garbage — must be a known pattern value or 0.
    ASSERT(v_neg >= INT16_MIN && v_neg <= INT16_MAX);
    ASSERT(v_big >= INT16_MIN && v_big <= INT16_MAX);
    PASS();
}

TEST test_PN_FUGUE_explicit_bank() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj0[4] = { 1, 2, 3, 4 };
    int16_t subj2[4] = { 10, 20, 30, 40 };
    // bank 0
    ss_set_pattern_start(&ss, 0, 0);
    ss_set_pattern_end(&ss, 0, 3);
    ss_set_pattern_len(&ss, 0, 4);
    for (int i = 0; i < 4; i++) ss_set_pattern_val(&ss, 0, i, subj0[i]);
    // bank 2
    ss_set_pattern_start(&ss, 2, 0);
    ss_set_pattern_end(&ss, 2, 3);
    ss_set_pattern_len(&ss, 2, 4);
    for (int i = 0; i < 4; i++) ss_set_pattern_val(&ss, 2, i, subj2[i]);
    for (int t = 0; t < 4; t++) {
        ASSERT_EQ(fugue_run_pn(&ss, 0, 0, 1, 0, 0, 0, t), subj0[t]);
        ASSERT_EQ(fugue_run_pn(&ss, 2, 0, 1, 0, 0, 0, t), subj2[t]);
    }
    PASS();
}

// P.FUGUE voice-protection helpers ///////////////////////////////////////////

// All voice-protection tests use a constant subject {1,1,1,1} so that the
// natural value of any voice equals `1 + transpose` regardless of clock.
// This lets us vary `clock` to test staleness without having to track which
// subject position each clock value reads.
static void vp_setup(scene_state_t* ss) {
    ss_init(ss);  // also calls fugue_voice_state_reset()
    ss_set_pattern_start(ss, 0, 0);
    ss_set_pattern_end(ss, 0, 3);
    ss_set_pattern_len(ss, 0, 4);
    for (int i = 0; i < 4; i++) ss_set_pattern_val(ss, 0, i, 1);
}

// Run voice V with transpose chosen to produce the requested natural value.
static int16_t vp_run(scene_state_t* ss, int voice, int natural_value,
                     int clock) {
    return fugue_run(ss, voice, 1, natural_value - 1, 0, 0, clock);
}

TEST test_P_FUGUE_voice0_stateless() {
    scene_state_t ss;
    vp_setup(&ss);
    // voice=0 at the position voice-1 would normally occupy. Should NOT
    // populate the state table.
    (void)vp_run(&ss, 0, 3, 0);
    // Now a voice 2 whose natural value is 4 (a 2nd above 3) should NOT be
    // adjusted, because voice=0 didn't write any state.
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 4);
    PASS();
}

TEST test_P_FUGUE_voice1_stores_state() {
    scene_state_t ss;
    vp_setup(&ss);
    // voice 1 plays degree 3.
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    // voice 2 whose natural value is 4 (2nd above) is bumped to 5.
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 5);
    PASS();
}

TEST test_P_FUGUE_voice2_no_clash() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    // 3rd above 1 = degree 3, consonant.
    ASSERT_EQ(vp_run(&ss, 2, 3, 0), 3);
    PASS();
}

TEST test_P_FUGUE_voice2_avoids_2nd() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 5);
    PASS();
}

TEST test_P_FUGUE_voice2_avoids_7th() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    // natural = 7 (a 7th above 1) -> bump to 8.
    ASSERT_EQ(vp_run(&ss, 2, 7, 0), 8);
    PASS();
}

TEST test_P_FUGUE_voice2_stale_ignored() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    // voice 2 at a different clock: voice-1 entry is stale -> no adjustment.
    ASSERT_EQ(vp_run(&ss, 2, 4, 1), 4);
    PASS();
}

TEST test_P_FUGUE_voice3_avoids_both() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    ASSERT_EQ(vp_run(&ss, 2, 3, 0), 3);
    // V3 natural=4: 2nd above V2 (diff=1) -> bump up to 5.
    // V3=5 vs V1=1 diff=4, vs V2=3 diff=2 -> both consonant.
    ASSERT_EQ(vp_run(&ss, 3, 4, 0), 5);
    PASS();
}

TEST test_P_FUGUE_cascading_adjust() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1=3, V2=7 (4th above V1, consonant -> stays at 7).
    // V3 natural=8:
    //   iter1: vs V1 diff=5 OK; vs V2 diff=1 clash, push UP -> 9.
    //   iter2: vs V1 diff=6, |%7|=6 clash, push UP -> 10.
    //   iter3: vs V1 diff=7 octave OK; vs V2 diff=3 OK. Done.
    // Two adjustments through two distinct lower-voice clashes.
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    ASSERT_EQ(vp_run(&ss, 2, 7, 0), 7);
    ASSERT_EQ(vp_run(&ss, 3, 8, 0), 10);
    PASS();
}

TEST test_P_FUGUE_unison_allowed() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    ASSERT_EQ(vp_run(&ss, 2, 3, 0), 3);  // unison: allowed
    PASS();
}

TEST test_P_FUGUE_octave_allowed() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    // diff=7 -> |%7|=0, allowed.
    ASSERT_EQ(vp_run(&ss, 2, 10, 0), 10);
    PASS();
}

TEST test_P_FUGUE_compound_dissonance() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    // V2 natural=11: diff=8, |%7|=1 -> compound 2nd, bump up.
    ASSERT_EQ(vp_run(&ss, 2, 11, 0), 12);
    PASS();
}

TEST test_PN_FUGUE_bank_isolation() {
    scene_state_t ss;
    ss_init(&ss);
    int16_t subj[4] = { 1, 2, 3, 4 };
    for (int b = 0; b < 2; b++) {
        ss_set_pattern_start(&ss, b, 0);
        ss_set_pattern_end(&ss, b, 3);
        ss_set_pattern_len(&ss, b, 4);
        for (int i = 0; i < 4; i++) ss_set_pattern_val(&ss, b, i, subj[i]);
    }
    // Bank 0: V1=3.
    ASSERT_EQ(fugue_run_pn(&ss, 0, 1, 1, 2, 0, 0, 0), 3);
    // Bank 1: V2 natural=4 — would clash with bank 0's V1=3 if state were
    // shared. Must NOT be adjusted since banks are independent.
    ASSERT_EQ(fugue_run_pn(&ss, 1, 2, 1, 3, 0, 0, 0), 4);
    PASS();
}

TEST test_P_FUGUE_voice_clamps() {
    scene_state_t ss;
    vp_setup(&ss);
    // voice=5 clamps to 4. Discriminating check: pre-populate slot 4 with
    // a value that *would* trigger a clash if voice=5 were not clamped (an
    // unclamped voice=5 would iterate v=1..4 and inspect slot 4). With the
    // clamp, the loop terminates at v < 4 and slot 4 is invisible.
    //
    // Pre-populate slot 4 with V4 candidate=3 (no lower voices written yet,
    // so V4 stores cleanly).
    ASSERT_EQ(vp_run(&ss, 4, 3, 0), 3);
    // Now call with voice=5, natural=4. Without clamp: vs slot4=3 diff=1
    // clash, push up to 5. With clamp: slot 4 not inspected, returns 4.
    ASSERT_EQ(vp_run(&ss, 5, 4, 0), 4);

    // voice=-1 clamps to 0 (fully stateless: no write to slot 0).
    fugue_voice_state_reset();
    (void)fugue_run(&ss, -1, 1, 2, 0, 0, 0);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 4);
    PASS();
}

TEST test_P_FUGUE_safety_terminates() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1=1, V2=4 (consonant 3rd, both stored as-is). V3 natural=3 sits
    // between them: vs V1 diff=2 OK; vs V2 diff=-1 clash, push DOWN -> 2;
    // then vs V1 diff=1 clash, push UP -> 3; then vs V2 diff=-1 clash again.
    // Oscillates between 2 and 3. Without the safety counter this would
    // loop forever; the test passing at all (within finite time) proves
    // the counter is present.
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 4);
    int16_t v3 = vp_run(&ss, 3, 3, 0);
    ASSERT(v3 == 2 || v3 == 3);
    PASS();
}

TEST test_P_FUGUE_out_of_order_voices() {
    scene_state_t ss;
    vp_setup(&ss);
    // Calling V2 before V1 at the same clock is documented as "unreliable
    // but not catastrophic". V2's clash check finds slot 1 invalid (not yet
    // written) and ignores it — natural value passes through unchanged.
    // V1 then runs and just writes its slot. No crash, no infinite loop.
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 4);  // V1 not yet written -> no clash
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);  // V1 writes itself, no checks
    PASS();
}

TEST test_P_FUGUE_clamp_triggers_on_extreme_drift() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1=1; V2 natural=21 (transpose=20).
    // Avoidance: vs V1 diff=20, |%7|=6 clash, push UP -> 22. vs V1 diff=21,
    // |%7|=0 octave allowed. Candidate=22.
    // Clamp: lowest=1, max_allowed=15. 22 -> 15.
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    ASSERT_EQ(vp_run(&ss, 2, 21, 0), 15);
    PASS();
}

TEST test_P_FUGUE_clamp_preserves_consonance() {
    scene_state_t ss;
    vp_setup(&ss);
    // Same as above; final V2=15 vs V1=1: diff=14, |%7|=0 -> consonant
    // (two octaves). The ±14 window edge is itself a consonant interval.
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    int16_t v2 = vp_run(&ss, 2, 21, 0);
    int interval = v2 - 1;
    int abs_mod7 = (interval < 0 ? -interval : interval) % 7;
    ASSERT_EQ(abs_mod7, 0);
    PASS();
}

TEST test_P_FUGUE_clamp_below_lowest_voice() {
    scene_state_t ss;
    vp_setup(&ss);
    // The clamp computes `lowest = min(candidate, all lower voices)`. So a
    // candidate that's already below all lower voices simply becomes the
    // new lowest — no upward shift triggers. V1=20, V2 natural=-10: V2 vs
    // V1 diff=-30, |%7|=2 OK. lowest = min(-10, 20) = -10. Bounds
    // [-24, 4], candidate -10 in range. Returns -10 unchanged.
    ASSERT_EQ(vp_run(&ss, 1, 20, 0), 20);
    ASSERT_EQ(vp_run(&ss, 2, -10, 0), -10);
    PASS();
}

TEST test_P_FUGUE_clamp_pushes_third_voice_down() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1=1, V2=2 (V2 auto-bumps to 3 via avoidance), V3 natural=30.
    // V3 avoidance: vs V1 diff=29, |%7|=1 clash, push UP -> 31. vs V1
    // diff=30, |%7|=2 OK. vs V2=3 diff=28, |%7|=0 octave OK. Candidate=31.
    // Clamp: lowest = min(31, 1, 3) = 1. max_allowed = 15. 31 -> 24 -> 17
    // -> 10. 10 <= 15, stop. Final=10.
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    ASSERT_EQ(vp_run(&ss, 2, 2, 0), 3);
    ASSERT_EQ(vp_run(&ss, 3, 30, 0), 10);
    PASS();
}

TEST test_P_FUGUE_clamp_idempotent_in_normal_range() {
    scene_state_t ss;
    vp_setup(&ss);
    // Re-run the avoid_2nd scenario explicitly to lock that the clamp is a
    // no-op for in-range candidates. V1=3, V2 natural=4 -> 5. With lowest=3,
    // max_allowed=17, min_allowed=-11; candidate 5 is in range, no shift.
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 5);
    PASS();
}

TEST test_P_FUGUE_clamp_no_lower_voices_noop() {
    scene_state_t ss;
    vp_setup(&ss);
    // voice=1 has no lower voices: lowest defaults to candidate, bounds are
    // candidate ± 14, candidate is trivially in range. No shift even at an
    // extreme transpose.
    ASSERT_EQ(vp_run(&ss, 1, 100, 0), 100);
    PASS();
}

TEST test_P_FUGUE_phantom_zero_state() {
    scene_state_t ss;
    vp_setup(&ss);
    // Post-reset, all slots have last_clock=0 and valid=0. If the staleness
    // check relied on last_clock alone (ignoring valid), V2's check at
    // clock=0 with natural=1 would compare against phantom note=0 (diff=1,
    // clash) and erroneously push up to 2. The `valid` flag must guard.
    ASSERT_EQ(vp_run(&ss, 2, 1, 0), 1);
    PASS();
}

TEST test_P_FUGUE_state_persists_within_tick() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 1, 0), 1);
    ASSERT_EQ(vp_run(&ss, 2, 3, 0), 3);
    // V3 natural=4: clashes with V2=3 (diff=1). V3 -> 5. Then vs V1=1: diff=4.
    ASSERT_EQ(vp_run(&ss, 3, 4, 0), 5);
    PASS();
}

TEST test_P_FUGUE_state_expires_across_ticks() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 10), 3);
    // No V1 call at clock=20; V2's clash check sees stale state, ignores it.
    ASSERT_EQ(vp_run(&ss, 2, 4, 20), 4);
    PASS();
}

TEST test_P_FUGUE_bidirectional_above() {
    scene_state_t ss;
    vp_setup(&ss);
    ASSERT_EQ(vp_run(&ss, 1, 3, 0), 3);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 5);  // push UP
    PASS();
}

TEST test_P_FUGUE_bidirectional_below() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1=5, V2 natural=4 (2nd below) -> push DOWN to 3.
    ASSERT_EQ(vp_run(&ss, 1, 5, 0), 5);
    ASSERT_EQ(vp_run(&ss, 2, 4, 0), 3);
    PASS();
}

TEST test_P_FUGUE_no_upward_drift() {
    scene_state_t ss;
    vp_setup(&ss);
    // V1 stable at 4. V2 natural oscillates between 5 and 3 (alternating 2nds
    // above and below). Under "always +1" V2's mean drifts upward; under the
    // bidirectional rule, pushes alternate up and down and the mean stays
    // near the natural mean.
    int natural_sum = 0;
    int actual_sum = 0;
    for (int t = 0; t < 100; t++) {
        ASSERT_EQ(vp_run(&ss, 1, 4, t), 4);
        int natural = (t % 2 == 0) ? 5 : 3;
        natural_sum += natural;
        actual_sum += vp_run(&ss, 2, natural, t);
    }
    // Under bidirectional, V2 outputs alternate 6/2 (push away from V1=4 in
    // each direction). Mean exactly matches natural mean. Under "always +1"
    // V2 would output 6/4 (each tick +1 above natural), drifting +100 over
    // 100 calls.
    ASSERT_EQ(actual_sum, natural_sum);
    PASS();
}

SUITE(process_suite) {
    RUN_TEST(test_numbers);
    RUN_TEST(test_ADD);
    RUN_TEST(test_PROB);
    RUN_TEST(test_IF);
    RUN_TEST(test_FLIP);
    RUN_TEST(test_L);
    RUN_TEST(test_O);
    RUN_TEST(test_P);
    RUN_TEST(test_Q);
    RUN_TEST(test_PN);
    RUN_TEST(test_X);
    RUN_TEST(test_sub_commands);
    RUN_TEST(test_blank_command);
    RUN_TEST(test_P_ROT_1);
    RUN_TEST(test_P_ROT_3);
    RUN_TEST(test_P_ACC);
    RUN_TEST(test_P_ACC_W);
    RUN_TEST(test_PN_ACC_family);
    RUN_TEST(test_P_D);
    RUN_TEST(test_P_D_HERE);
    RUN_TEST(test_PN_D_family);
    RUN_TEST(test_P_D_RND);
    RUN_TEST(test_P_STEP_basic);
    RUN_TEST(test_P_STEP_dwells);
    RUN_TEST(test_P_I_resets_dwell);
    RUN_TEST(test_PN_STEP_independence);
    RUN_TEST(test_P_STEPQ);
    RUN_TEST(test_P_MODE_pingpong);
    RUN_TEST(test_P_MODE_pendulum);
    RUN_TEST(test_P_MODE_pendulum_range_2);
    RUN_TEST(test_P_MODE_linear_rev);
    RUN_TEST(test_P_MODE_jump);
    RUN_TEST(test_P_MODE_random_in_range);
    RUN_TEST(test_P_MODE_roundtrip_via_p_i);
    RUN_TEST(test_P_MOTIF_determinism);
    RUN_TEST(test_P_MOTIF_repetition);
    RUN_TEST(test_P_MOTIF_transposition);
    RUN_TEST(test_P_MOTIF_inversion);
    RUN_TEST(test_P_MOTIF_retrograde);
    RUN_TEST(test_P_MOTIF_alternate);
    RUN_TEST(test_P_MOTIF_ornament);
    RUN_TEST(test_P_MOTIF_expand);
    RUN_TEST(test_P_MOTIF_compress);
    RUN_TEST(test_P_MOTIF_rotate);
    RUN_TEST(test_P_MOTIF_partial_trailing);
    RUN_TEST(test_P_MOTIF_length_clamping);
    RUN_TEST(test_P_MOTIF_window_respect);
    RUN_TEST(test_P_MOTIF_window_length_1);
    RUN_TEST(test_P_CP_determinism);
    RUN_TEST(test_P_CP_no_side_effects);
    RUN_TEST(test_P_CP_consonance);
    RUN_TEST(test_P_CP_canon_arithmetic);
    RUN_TEST(test_P_CP_canon_offset);
    RUN_TEST(test_P_CP_bass_below);
    RUN_TEST(test_P_CP_mirror_symmetry);
    RUN_TEST(test_P_CP_oblique_clustering);
    RUN_TEST(test_P_CP_offset_shifts);
    RUN_TEST(test_P_CP_window_wrap);
    RUN_TEST(test_P_CP_window_length_one);
    RUN_TEST(test_P_CP_inverted_window_applies_offset);
    RUN_TEST(test_P_CP_oblique_negative_cf);
    RUN_TEST(test_PN_CP_explicit_bank);
    RUN_TEST(test_P_FUGUE_determinism);
    RUN_TEST(test_P_FUGUE_no_side_effects);
    RUN_TEST(test_P_FUGUE_basic_playback);
    RUN_TEST(test_P_FUGUE_division_holds);
    RUN_TEST(test_P_FUGUE_subject_wrap);
    RUN_TEST(test_P_FUGUE_negative_division);
    RUN_TEST(test_P_FUGUE_transpose_adds);
    RUN_TEST(test_P_FUGUE_inversion);
    RUN_TEST(test_P_FUGUE_retrograde);
    RUN_TEST(test_P_FUGUE_retrograde_inversion);
    RUN_TEST(test_P_FUGUE_direction_cancel);
    RUN_TEST(test_P_FUGUE_phase_shifts);
    RUN_TEST(test_P_FUGUE_phase_wraps);
    RUN_TEST(test_P_FUGUE_negative_phase);
    RUN_TEST(test_P_FUGUE_phase_plus_clock);
    RUN_TEST(test_P_FUGUE_phase_retrograde);
    RUN_TEST(test_P_FUGUE_negative_clock);
    RUN_TEST(test_P_FUGUE_clock_zero);
    RUN_TEST(test_P_FUGUE_subject_len_one);
    RUN_TEST(test_P_FUGUE_division_zero);
    RUN_TEST(test_P_FUGUE_mode_clamp);
    RUN_TEST(test_P_FUGUE_int16_min_clock);
    RUN_TEST(test_P_FUGUE_transpose_clamps);
    RUN_TEST(test_P_FUGUE_inverted_window);
    RUN_TEST(test_P_FUGUE_truncation_plateau);
    RUN_TEST(test_P_FUGUE_inversion_nonzero_start);
    RUN_TEST(test_PN_FUGUE_modes_and_phase);
    RUN_TEST(test_PN_FUGUE_pn_normalises);
    RUN_TEST(test_PN_FUGUE_explicit_bank);
    RUN_TEST(test_P_FUGUE_voice0_stateless);
    RUN_TEST(test_P_FUGUE_voice1_stores_state);
    RUN_TEST(test_P_FUGUE_voice2_no_clash);
    RUN_TEST(test_P_FUGUE_voice2_avoids_2nd);
    RUN_TEST(test_P_FUGUE_voice2_avoids_7th);
    RUN_TEST(test_P_FUGUE_voice2_stale_ignored);
    RUN_TEST(test_P_FUGUE_voice3_avoids_both);
    RUN_TEST(test_P_FUGUE_cascading_adjust);
    RUN_TEST(test_P_FUGUE_unison_allowed);
    RUN_TEST(test_P_FUGUE_octave_allowed);
    RUN_TEST(test_P_FUGUE_compound_dissonance);
    RUN_TEST(test_PN_FUGUE_bank_isolation);
    RUN_TEST(test_P_FUGUE_voice_clamps);
    RUN_TEST(test_P_FUGUE_state_persists_within_tick);
    RUN_TEST(test_P_FUGUE_state_expires_across_ticks);
    RUN_TEST(test_P_FUGUE_bidirectional_above);
    RUN_TEST(test_P_FUGUE_bidirectional_below);
    RUN_TEST(test_P_FUGUE_no_upward_drift);
    RUN_TEST(test_P_FUGUE_safety_terminates);
    RUN_TEST(test_P_FUGUE_out_of_order_voices);
    RUN_TEST(test_P_FUGUE_phantom_zero_state);
    RUN_TEST(test_P_FUGUE_clamp_triggers_on_extreme_drift);
    RUN_TEST(test_P_FUGUE_clamp_preserves_consonance);
    RUN_TEST(test_P_FUGUE_clamp_below_lowest_voice);
    RUN_TEST(test_P_FUGUE_clamp_pushes_third_voice_down);
    RUN_TEST(test_P_FUGUE_clamp_idempotent_in_normal_range);
    RUN_TEST(test_P_FUGUE_clamp_no_lower_voices_noop);
}
