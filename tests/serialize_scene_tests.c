#include "serialize_scene_tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "greatest/greatest.h"
#include "log.h"
#include "ops/op_enum.h"
#include "scene_serialization.h"
#include "serializer.h"
#include "state.h"
#include "teletype.h"

void test_file_write_buffer(void* self_data, uint8_t* buffer, uint16_t size) {
    fwrite(buffer, 1, size, (FILE*)self_data);
}
void test_file_write_char(void* self_data, uint8_t c) {
    fputc(c, (FILE*)self_data);
}
void test_print_dbg(const char* c) {
    printf("%s\n", c);
}

uint16_t test_file_read_char(void* self_data) {
    return (uint16_t)fgetc((FILE*)self_data);
}
bool test_file_eof(void* self_data) {
    return feof((FILE*)self_data) != 0;
}

typedef struct {
    char* buffer;
    unsigned int length;
    unsigned int position;
} stringsource;

void test_string_write_buffer(void* self_data, uint8_t* buffer, uint16_t size) {
    stringsource* ss = (stringsource*)self_data;
    strncpy(ss->buffer + ss->position, (char*)buffer, size);
    ss->position += size;
    ss->length += size;
}
void test_string_write_char(void* self_data, uint8_t c) {
    stringsource* ss = (stringsource*)self_data;
    ss->buffer[ss->position] = c;
    ss->position += 1;
    ss->length += 1;
}
uint16_t test_string_read_char(void* self_data) {
    stringsource* ss = (stringsource*)self_data;
    if (ss->position < ss->length) {
        char r = ss->buffer[ss->position];
        ss->position += 1;
        return r;
    }
    else { return -1; }
}
bool test_string_eof(void* self_data) {
    stringsource* ss = (stringsource*)self_data;
    return (ss->position >= ss->length);
}

tt_serializer_t test_file_writer, test_string_writer;
tt_deserializer_t test_file_reader, test_string_reader;

void init_serializers() {
    test_file_writer.write_buffer = &test_file_write_buffer;
    test_file_writer.write_char = &test_file_write_char;
    test_file_writer.print_dbg = &test_print_dbg;

    test_file_reader.read_char = &test_file_read_char;
    test_file_reader.eof = &test_file_eof;
    test_file_reader.print_dbg = &test_print_dbg;

    test_string_writer.write_buffer = &test_string_write_buffer;
    test_string_writer.write_char = &test_string_write_char;
    test_string_writer.print_dbg = &test_print_dbg;

    test_string_reader.read_char = &test_string_read_char;
    test_string_reader.eof = &test_string_eof;
    test_string_reader.print_dbg = &test_print_dbg;
}

void deserialize_fragment(char* fragment, scene_state_t* scene,
                          char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]) {
    stringsource ss;
    ss.buffer = fragment;
    ss.length = strlen(fragment);
    ss.position = 0;

    test_string_reader.data = (void*)&ss;

    deserialize_scene(&test_string_reader, scene, text);
}

int compare_files(char* filename, FILE* a, FILE* b) {
    fseek(a, 0, 0);
    fseek(b, 0, 0);

    int line = 1;

    while (!feof(a) && !feof(b)) {
        char ca = fgetc(a);
        char cb = fgetc(b);
        if (ca != cb) {
            lprintf("At %s line %d, expected '%c', got '%c'", filename, line,
                    ca, cb);
            return -1;
        }
        if (ca == '\n') { line++; }
    }

    if (feof(a) != feof(b)) { return -1; }

    return 0;
}

TEST test_round_trip_file(char* filename, char* tempfile) {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    for (int i = 0; i < 9; i++) {
        FILE* infile = fopen(filename, "rb");
        ASSERT(infile != 0);
        test_file_reader.data = (void*)infile;

        deserialize_scene(&test_file_reader, &scene, &text);

        FILE* outfile = fopen(tempfile, "w+b");
        ASSERT(outfile != 0);
        test_file_writer.data = (void*)outfile;

        serialize_scene(&test_file_writer, &scene, &text);

        CHECK_CALL(compare_files(filename, infile, outfile));
    }

    PASS();
}

// Round-trip patterns 0..PATTERN_COUNT-1 with distinct values, ensuring all
// pattern slots survive serialise/deserialise.
TEST test_round_trip_all_patterns() {
    scene_state_t scene_a, scene_b;
    ss_init(&scene_a);
    ss_init(&scene_b);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    for (int p = 0; p < PATTERN_COUNT; p++) {
        ss_set_pattern_len(&scene_a, p, 1 + p);
        ss_set_pattern_wrap(&scene_a, p, p & 1);
        ss_set_pattern_start(&scene_a, p, p);
        ss_set_pattern_end(&scene_a, p, p + 1);
        for (int i = 0; i < PATTERN_LENGTH; i++) {
            ss_set_pattern_val(&scene_a, p, i, (int16_t)(p * 100 + i));
        }
    }

    char buffer[32768];
    memset(buffer, 0, sizeof(buffer));
    stringsource out_ss = { .buffer = buffer,
                            .length = 0,
                            .position = 0 };
    test_string_writer.data = (void*)&out_ss;
    serialize_scene(&test_string_writer, &scene_a, &text);

    stringsource in_ss = { .buffer = buffer,
                           .length = out_ss.length,
                           .position = 0 };
    test_string_reader.data = (void*)&in_ss;
    deserialize_scene(&test_string_reader, &scene_b, &text);

    for (int p = 0; p < PATTERN_COUNT; p++) {
        ASSERT_EQ(1 + p, ss_get_pattern_len(&scene_b, p));
        ASSERT_EQ(p & 1, ss_get_pattern_wrap(&scene_b, p));
        ASSERT_EQ(p, ss_get_pattern_start(&scene_b, p));
        ASSERT_EQ(p + 1, ss_get_pattern_end(&scene_b, p));
        for (int i = 0; i < PATTERN_LENGTH; i++) {
            ASSERT_EQ((int16_t)(p * 100 + i),
                      ss_get_pattern_val(&scene_b, p, i));
        }
    }
    PASS();
}

// Loading an old-format scene (4 patterns) must zero-initialise extra slots.
TEST test_deserialize_legacy_4pattern_scene() {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    // Minimal #P block in the old 4-pattern-per-row format. Pattern 0 has
    // len=2, wrap=1, start=0, end=1 and values 5,6 in positions 0,1.
    char fragment[8192];
    int p = 0;
    p += snprintf(fragment + p, sizeof(fragment) - p,
                  "#P\n2\t0\t0\t0\n1\t0\t0\t0\n0\t0\t0\t0\n1\t0\t0\t0\n\n");
    p += snprintf(fragment + p, sizeof(fragment) - p, "5\t0\t0\t0\n");
    p += snprintf(fragment + p, sizeof(fragment) - p, "6\t0\t0\t0\n");
    for (int i = 2; i < 64; i++)
        p += snprintf(fragment + p, sizeof(fragment) - p, "0\t0\t0\t0\n");

    deserialize_fragment(fragment, &scene, &text);

    ASSERT_EQ(2, ss_get_pattern_len(&scene, 0));
    ASSERT_EQ(1, ss_get_pattern_wrap(&scene, 0));
    ASSERT_EQ(0, ss_get_pattern_start(&scene, 0));
    ASSERT_EQ(1, ss_get_pattern_end(&scene, 0));
    ASSERT_EQ(5, ss_get_pattern_val(&scene, 0, 0));
    ASSERT_EQ(6, ss_get_pattern_val(&scene, 0, 1));

    // patterns 4..PATTERN_COUNT-1 must remain at init defaults — the old
    // 4-pattern format does not touch them.
    for (int pat = 4; pat < PATTERN_COUNT; pat++) {
        ASSERT_EQ(0, ss_get_pattern_len(&scene, pat));
        ASSERT_EQ(1, ss_get_pattern_wrap(&scene, pat));  // ss_pattern_init
        ASSERT_EQ(0, ss_get_pattern_start(&scene, pat));
        ASSERT_EQ(63, ss_get_pattern_end(&scene, pat));  // ss_pattern_init
        for (int i = 0; i < PATTERN_LENGTH; i++)
            ASSERT_EQ(0, ss_get_pattern_val(&scene, pat, i));
    }
    PASS();
}

TEST test_deserialize_fragment_script_basic() {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    deserialize_fragment("#1\nTR.P 4\n\n", &scene, &text);
    // confirm we parsed a command with OP and NUMBER
    ASSERT(scene.scripts[0].c[0].length == 2);
    ASSERT(scene.scripts[0].c[0].data[0].tag == OP);
    ASSERT(scene.scripts[0].c[0].data[0].value == E_OP_TR_P);
    ASSERT(scene.scripts[0].c[0].data[1].tag == NUMBER);
    ASSERT(scene.scripts[0].c[0].data[1].value == 4);
    // confirm only one command in script
    ASSERT(scene.scripts[0].c[1].length == 0);
    // confirm rest of scripts are empty
    for (int i = 1; i < 11; i++) { ASSERT(scene.scripts[i].c[0].length == 0); }
    // confirm no text
    ASSERT(text[0][0] == 0);

    PASS();
}

SUITE(serialize_scene_suite) {
    log_init();
    init_serializers();
    RUN_TESTp(test_round_trip_file, "../presets/tt00.txt",
              "./test_output/tt00.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt01.txt",
              "./test_output/tt01.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt02.txt",
              "./test_output/tt02.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt03.txt",
              "./test_output/tt03.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt04.txt",
              "./test_output/tt04.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt05.txt",
              "./test_output/tt05.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt06.txt",
              "./test_output/tt06.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt07.txt",
              "./test_output/tt07.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt08.txt",
              "./test_output/tt08.txt");
    RUN_TESTp(test_round_trip_file, "../presets/tt09.txt",
              "./test_output/tt09.txt");
    RUN_TEST(test_round_trip_all_patterns);
    RUN_TEST(test_deserialize_legacy_4pattern_scene);
    RUN_TEST(test_deserialize_fragment_script_basic);
    log_print();
}
