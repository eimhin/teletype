#include "custom_pattern_tests.h"

#include <stdio.h>

#include "greatest/greatest.h"
#include "teletype.h"

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

// Every XP op must parse and validate. A rename, missing token, or arity
// change fails here with a clear message.
TEST xp_ops_parse() {
    char* lines[] = { "XP 0 0",         "XP 0 0 1",       "XP.HERE 0",
                      "XP.HERE 0 1",    "XP.I 0",         "XP.I 0 1",
                      "XP.L 0",         "XP.L 0 1",       "XP.WRAP 0",
                      "XP.WRAP 0 1",    "XP.START 0",     "XP.START 0 1",
                      "XP.END 0",       "XP.END 0 1",     "XP.NEXT 0",
                      "XP.NEXT.ALL",    "XP.D 0 0",       "XP.D 0 0 1",
                      "XP.D.HERE 0",    "XP.D.HERE 0 1",  "XP.D.RND 0 1 4",
                      "XP.D.RND.M 0 8", "XP.D.RND.N 0 8", "XP.STEP 0",
                      "XP.STEP.NEW 0",  "XP.STEP? 0",     "XP.STEP.ALL" };
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        tele_command_t cmd;
        char error_msg[TELE_ERROR_MSG_LENGTH];
        if (parse(lines[i], &cmd, error_msg) != E_OK) FAILm(lines[i]);
        if (validate(&cmd, error_msg) != E_OK) FAILm(lines[i]);
    }
    PASS();
}

// Basic get/set at (column, index).
TEST xp_get_set() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP 0 0 60");
    ASSERT_EQ(60, run(&ss, "XP 0 0"));
    ASSERT_EQ(0, run(&ss, "XP 0 1"));  // untouched default
    PASS();
}

// Each column is an independent track: writing one leaves the others alone.
TEST xp_columns_independent() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP 0 0 11");
    run(&ss, "XP 1 0 22");
    run(&ss, "XP 6 0 77");
    ASSERT_EQ(11, run(&ss, "XP 0 0"));
    ASSERT_EQ(22, run(&ss, "XP 1 0"));
    ASSERT_EQ(77, run(&ss, "XP 6 0"));
    PASS();
}

// Column index clamps to 0..CUSTOM_PATTERN_WIDTH-1.
TEST xp_column_clamps() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP 99 0 5");  // -> column 6
    ASSERT_EQ(5, run(&ss, "XP 6 0"));
    run(&ss, "XP -1 0 9");  // -> column 0
    ASSERT_EQ(9, run(&ss, "XP 0 0"));
    PASS();
}

// Length get/set per column, with clamping to 0..CUSTOM_PATTERN_LENGTH.
TEST xp_length_clamps() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 4");
    ASSERT_EQ(4, run(&ss, "XP.L 0"));
    run(&ss, "XP.L 0 99");  // clamp high
    ASSERT_EQ(16, run(&ss, "XP.L 0"));
    run(&ss, "XP.L 0 -5");  // clamp low
    ASSERT_EQ(0, run(&ss, "XP.L 0"));
    PASS();
}

// HERE reads/writes the value at the column's current index.
TEST xp_here() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 4");
    run(&ss, "XP 0 2 55");
    run(&ss, "XP.I 0 2");
    ASSERT_EQ(2, run(&ss, "XP.I 0"));
    ASSERT_EQ(55, run(&ss, "XP.HERE 0"));
    run(&ss, "XP.HERE 0 77");
    ASSERT_EQ(77, run(&ss, "XP 0 2"));
    PASS();
}

// NEXT advances the column's index honouring its own length/start/end/wrap.
TEST xp_next_wraps_on_length() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 4");
    run(&ss, "XP 0 0 10");
    run(&ss, "XP 0 1 11");
    run(&ss, "XP 0 2 12");
    run(&ss, "XP 0 3 13");
    run(&ss, "XP.I 0 0");
    ASSERT_EQ(11, run(&ss, "XP.NEXT 0"));  // idx 0 -> 1
    ASSERT_EQ(12, run(&ss, "XP.NEXT 0"));  // -> 2
    ASSERT_EQ(13, run(&ss, "XP.NEXT 0"));  // -> 3 (== len-1)
    ASSERT_EQ(10, run(&ss, "XP.NEXT 0"));  // wraps to start 0
    PASS();
}

// NEXT honours an explicit END before the length boundary.
TEST xp_next_honours_end() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 8");
    run(&ss, "XP.START 0 1");
    run(&ss, "XP.END 0 2");
    run(&ss, "XP 0 1 101");
    run(&ss, "XP 0 2 102");
    run(&ss, "XP.I 0 1");
    ASSERT_EQ(102, run(&ss, "XP.NEXT 0"));  // 1 -> 2 (== end)
    ASSERT_EQ(101, run(&ss, "XP.NEXT 0"));  // end -> wrap to start 1
    PASS();
}

// NEXT.ALL advances every column once, each by its own range.
TEST xp_next_all_advances_every_column() {
    scene_state_t ss;
    ss_init(&ss);
    for (int c = 0; c < 8; c++) {
        char line[16];
        sprintf(line, "XP.L %d 4", c);
        run(&ss, line);
        sprintf(line, "XP.I %d 0", c);
        run(&ss, line);
    }
    run(&ss, "XP.NEXT.ALL");
    for (int c = 0; c < 8; c++) {
        char line[16];
        sprintf(line, "XP.I %d", c);
        ASSERT_EQ(1, run(&ss, line));
    }
    PASS();
}

// Durations: get/set per (column, index), clamped to a minimum of 1.
TEST xp_d_get_set() {
    scene_state_t ss;
    ss_init(&ss);
    // default duration is 1 for every cell.
    ASSERT_EQ(1, run(&ss, "XP.D 0 0"));
    ASSERT_EQ(1, run(&ss, "XP.D 3 5"));
    run(&ss, "XP.D 0 0 4");
    ASSERT_EQ(4, run(&ss, "XP.D 0 0"));
    // a duration < 1 would freeze the sequencer, so it clamps up to 1.
    run(&ss, "XP.D 1 0 0");
    ASSERT_EQ(1, run(&ss, "XP.D 1 0"));
    run(&ss, "XP.D 2 0 -5");
    ASSERT_EQ(1, run(&ss, "XP.D 2 0"));
    PASS();
}

// XP.D.HERE reads/writes the duration at the column's current index.
TEST xp_d_here() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 4");
    run(&ss, "XP.I 0 2");
    run(&ss, "XP.D.HERE 0 6");
    ASSERT_EQ(6, run(&ss, "XP.D 0 2"));
    ASSERT_EQ(6, run(&ss, "XP.D.HERE 0"));
    PASS();
}

// XP.STEP dwells on a cell for its duration before advancing, and
// XP.STEP.NEW reports the tick the playhead moved.
TEST xp_step_dwells() {
    scene_state_t ss;
    ss_init(&ss);
    // 2-cell column, dur[0]=3, dur[1]=1.
    run(&ss, "XP.L 0 2");
    run(&ss, "XP.WRAP 0 1");
    run(&ss, "XP 0 0 100");
    run(&ss, "XP 0 1 200");
    run(&ss, "XP.D 0 0 3");
    run(&ss, "XP.D 0 1 1");
    run(&ss, "XP.I 0 0");  // resets dwell

    // tick 1: entering stage fresh, idx 0, value 100, NEW=1.
    ASSERT_EQ(100, run(&ss, "XP.STEP 0"));
    ASSERT_EQ(1, run(&ss, "XP.STEP.NEW 0"));
    // tick 2: dwell 1 -> hold (2 <= 3), NEW=0.
    ASSERT_EQ(100, run(&ss, "XP.STEP 0"));
    ASSERT_EQ(0, run(&ss, "XP.STEP.NEW 0"));
    // tick 3: dwell 2 -> hold (3 <= 3), NEW=0.
    ASSERT_EQ(100, run(&ss, "XP.STEP 0"));
    ASSERT_EQ(0, run(&ss, "XP.STEP.NEW 0"));
    // tick 4: dwell 3 -> advance (4 > 3), idx 1, value 200, NEW=1.
    ASSERT_EQ(200, run(&ss, "XP.STEP 0"));
    ASSERT_EQ(1, run(&ss, "XP.STEP.NEW 0"));
    // tick 5: dur 1 -> advance, wraps to idx 0, value 100, NEW=1.
    ASSERT_EQ(100, run(&ss, "XP.STEP 0"));
    ASSERT_EQ(1, run(&ss, "XP.STEP.NEW 0"));
    PASS();
}

// XP.STEP? advances like XP.STEP but returns the just-advanced flag.
TEST xp_stepq() {
    scene_state_t ss;
    ss_init(&ss);
    run(&ss, "XP.L 0 2");
    run(&ss, "XP.D 0 0 2");
    run(&ss, "XP.I 0 0");
    ASSERT_EQ(1, run(&ss, "XP.STEP? 0"));  // entering, NEW
    ASSERT_EQ(0, run(&ss, "XP.STEP? 0"));  // dwelling (dur 2)
    ASSERT_EQ(1, run(&ss, "XP.STEP? 0"));  // advanced
    PASS();
}

// XP.STEP.ALL dwell-steps every column once, each by its own duration.
TEST xp_step_all() {
    scene_state_t ss;
    ss_init(&ss);
    for (int c = 0; c < CUSTOM_PATTERN_WIDTH; c++) {
        char line[16];
        sprintf(line, "XP.L %d 2", c);
        run(&ss, line);
        sprintf(line, "XP.I %d 0", c);
        run(&ss, line);
    }
    // dur defaults to 1: first step enters the stage (no advance), second
    // step advances every column to idx 1.
    run(&ss, "XP.STEP.ALL");
    for (int c = 0; c < CUSTOM_PATTERN_WIDTH; c++) {
        char line[16];
        sprintf(line, "XP.I %d", c);
        ASSERT_EQ(0, run(&ss, line));
    }
    run(&ss, "XP.STEP.ALL");
    for (int c = 0; c < CUSTOM_PATTERN_WIDTH; c++) {
        char line[16];
        sprintf(line, "XP.I %d", c);
        ASSERT_EQ(1, run(&ss, line));
    }
    PASS();
}

static int cp_dur_sum(scene_state_t* ss, int col, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        char line[16];
        sprintf(line, "XP.D %d %d", col, i);
        sum += run(ss, line);
    }
    return sum;
}

// XP.D.RND.N sums to exactly n; XP.D.RND.M to the smallest multiple of n >=
// len. Every cell stays >= 1.
TEST xp_d_rnd_sums() {
    scene_state_t ss;
    ss_init(&ss);

    run(&ss, "XP.L 0 4");
    run(&ss, "XP.D.RND.N 0 10");
    ASSERT_EQ(10, cp_dur_sum(&ss, 0, 4));
    for (int i = 0; i < 4; i++) {
        char line[16];
        sprintf(line, "XP.D 0 %d", i);
        ASSERT(run(&ss, line) >= 1);
    }

    // len 4, n 8 -> already a multiple of 8 and >= len, target 8.
    run(&ss, "XP.D.RND.M 0 8");
    ASSERT_EQ(8, cp_dur_sum(&ss, 0, 4));

    // len 5, n 4 -> rounds up to next multiple: 8.
    run(&ss, "XP.L 1 5");
    run(&ss, "XP.D.RND.M 1 4");
    ASSERT_EQ(8, cp_dur_sum(&ss, 1, 5));
    PASS();
}

SUITE(custom_pattern_suite) {
    RUN_TEST(xp_ops_parse);
    RUN_TEST(xp_get_set);
    RUN_TEST(xp_columns_independent);
    RUN_TEST(xp_column_clamps);
    RUN_TEST(xp_length_clamps);
    RUN_TEST(xp_here);
    RUN_TEST(xp_next_wraps_on_length);
    RUN_TEST(xp_next_honours_end);
    RUN_TEST(xp_next_all_advances_every_column);
    RUN_TEST(xp_d_get_set);
    RUN_TEST(xp_d_here);
    RUN_TEST(xp_step_dwells);
    RUN_TEST(xp_stepq);
    RUN_TEST(xp_step_all);
    RUN_TEST(xp_d_rnd_sums);
}
