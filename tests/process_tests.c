#include "process_tests.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // ssize_t

#include "greatest/greatest.h"
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
}
