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

// P.A: emit-then-advance, non-destructive, reset-every-n,
// offset wraps in [min, max].
TEST test_P_ACC() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep[3] = { "P.N 0", "PN 0 0 100", "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 100));

    // step=2, n=0, offset wraps in [0, 5].
    // offset sequence: 0,2,4, wrap(6,0,5)=0, 2, 4, ...
    // returned: 100, 102, 104, 100, 102, 104, ...
    char* t[1] = { "P.A 0 2 0 0 5" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t, 102));
    CHECK_CALL(process_helper_state(&ss, 1, t, 104));
    CHECK_CALL(process_helper_state(&ss, 1, t, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t, 102));

    // Pattern value itself must be untouched.
    char* read_base[1] = { "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 1, read_base, 100));

    // ACC.CLR rezeroes all offsets. Use a follow-up GET to assert.
    char* clr_then_read[2] = { "ACC.CLR", "P.A 0 2 0 0 5" };
    CHECK_CALL(process_helper_state(&ss, 2, clr_then_read, 100));

    // Reset-every-n: with n=3, step=1, sequence is 100,101,102,100,101,102,...
    char* clr_then_read2[2] = { "ACC.CLR", "P.A 0 1 3 0 100" };
    CHECK_CALL(process_helper_state(&ss, 2, clr_then_read2, 100));
    char* t2[1] = { "P.A 0 1 3 0 100" };
    CHECK_CALL(process_helper_state(&ss, 1, t2, 101));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 102));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 100));
    CHECK_CALL(process_helper_state(&ss, 1, t2, 101));

    PASS();
}

// P.AV: bounds the RETURNED VALUE, reproducing P.+W's destructive sequence.
TEST test_P_ACCV() {
    scene_state_t ss;
    ss_init(&ss);

    char* prep[3] = { "P.N 0", "PN 0 0 5", "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 5));

    // step=2, n=0, wrap in [0,10]. Note: shared wrap() helper treats `b` as
    // exclusive of the next wrap point, so wrap(11,0,10)=0 (modulus 11).
    // offset: 0,2,4,6,8,10,12,14,16,...
    // returned (= wrap(5+offset,0,10)):
    //   5, 7, 9, 0, 2, 4, 6, 8, 10, ...
    char* t[1] = { "P.AV 0 2 0 0 10" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 5));
    CHECK_CALL(process_helper_state(&ss, 1, t, 7));
    CHECK_CALL(process_helper_state(&ss, 1, t, 9));
    CHECK_CALL(process_helper_state(&ss, 1, t, 0));
    CHECK_CALL(process_helper_state(&ss, 1, t, 2));
    CHECK_CALL(process_helper_state(&ss, 1, t, 4));
    CHECK_CALL(process_helper_state(&ss, 1, t, 6));

    // Pattern itself unmodified.
    char* base[1] = { "PN 0 0" };
    CHECK_CALL(process_helper_state(&ss, 1, base, 5));

    PASS();
}

// P.AH: uses the current pattern position implicitly (no i arg).
TEST test_P_ACC_HERE() {
    scene_state_t ss;
    ss_init(&ss);

    // Set up pattern 0 with values at indices 0..2 and position the playhead
    // at 1.
    char* prep[7] = { "P.N 0",     "P.L 3", "PN 0 0 10", "PN 0 1 20",
                      "PN 0 2 30", "P.I 1", "P.I" };
    CHECK_CALL(process_helper_state(&ss, 7, prep, 1));

    // P.AH step=2 n=0 min=0 max=10 -> uses idx=1, base=20.
    // returned: 20, 22, 24, 26, 28, wrap(30,0,10)=30-11=8 -> 28? recompute.
    // offset 0,2,4,6,8,wrap(10,0,10)=10; results 20,22,24,26,28,30
    char* t[1] = { "P.AH 2 0 0 10" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 20));
    CHECK_CALL(process_helper_state(&ss, 1, t, 22));
    CHECK_CALL(process_helper_state(&ss, 1, t, 24));

    // Advance playhead to 0 -> next call uses base 10.
    char* advance[1] = { "P.I 0" };
    process_helper_state(&ss, 1, advance, 0);
    CHECK_CALL(process_helper_state(&ss, 1, t, 10));
    CHECK_CALL(process_helper_state(&ss, 1, t, 12));

    // Pattern values untouched.
    char* read[1] = { "PN 0 1" };
    CHECK_CALL(process_helper_state(&ss, 1, read, 20));

    PASS();
}

// PN.* variants: explicit pattern arg without touching p_n.
TEST test_PN_ACC_family() {
    scene_state_t ss;
    ss_init(&ss);

    // Set p_n to 3 to confirm PN.A does not consult it.
    char* prep[3] = { "P.N 3", "PN 1 0 200", "P.N" };
    CHECK_CALL(process_helper_state(&ss, 3, prep, 3));

    char* t[1] = { "PN.A 1 0 5 0 0 1000" };
    CHECK_CALL(process_helper_state(&ss, 1, t, 200));
    CHECK_CALL(process_helper_state(&ss, 1, t, 205));
    CHECK_CALL(process_helper_state(&ss, 1, t, 210));

    // p_n is still 3.
    char* check_pn[1] = { "P.N" };
    CHECK_CALL(process_helper_state(&ss, 1, check_pn, 3));

    // PN.AV: same as P.AV but with explicit pattern. Use pattern 2.
    char* prep2[2] = { "PN 2 0 5", "PN 2 0" };
    CHECK_CALL(process_helper_state(&ss, 2, prep2, 5));
    char* tv[1] = { "PN.AV 2 0 2 0 0 10" };
    CHECK_CALL(process_helper_state(&ss, 1, tv, 5));
    CHECK_CALL(process_helper_state(&ss, 1, tv, 7));
    CHECK_CALL(process_helper_state(&ss, 1, tv, 9));
    CHECK_CALL(process_helper_state(&ss, 1, tv, 0));

    // PN.AH: HERE variant on explicit pattern. Position pattern 4's playhead.
    char* prep3[5] = { "PN 4 0 100", "PN 4 1 200", "PN.L 4 2", "PN.I 4 1",
                       "PN.I 4" };
    CHECK_CALL(process_helper_state(&ss, 5, prep3, 1));
    char* th[1] = { "PN.AH 4 5 0 0 1000" };
    CHECK_CALL(process_helper_state(&ss, 1, th, 200));
    CHECK_CALL(process_helper_state(&ss, 1, th, 205));
    CHECK_CALL(process_helper_state(&ss, 1, th, 210));

    // PN.AVH: value-wrap HERE on explicit pattern. Pattern 5.
    char* prep4[5] = { "PN 5 0 5", "PN 5 1 50", "PN.L 5 2", "PN.I 5 0",
                       "PN.I 5" };
    CHECK_CALL(process_helper_state(&ss, 5, prep4, 0));
    char* tvh[1] = { "PN.AVH 5 2 0 0 10" };
    CHECK_CALL(process_helper_state(&ss, 1, tvh, 5));
    CHECK_CALL(process_helper_state(&ss, 1, tvh, 7));
    CHECK_CALL(process_helper_state(&ss, 1, tvh, 9));
    CHECK_CALL(process_helper_state(&ss, 1, tvh, 0));

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
    RUN_TEST(test_P_ACCV);
    RUN_TEST(test_P_ACC_HERE);
    RUN_TEST(test_PN_ACC_family);
}
