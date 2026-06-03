#include "scene_serialization.h"

#include "teletype.h"
#include "util.h"

#define STATE_DESC 0
#define STATE_POUND 1
#define STATE_POUND_IGNORE 2
#define STATE_SCRIPT 3
#define STATE_PATTERNS 4
#define STATE_GRID 5
#define STATE_PATTERN_DURATIONS 6
#define STATE_CUSTOM_PATTERN 7

uint8_t grid_state = 0;
uint16_t grid_count = 0;
uint8_t grid_num = 0;

// internal test functions to make sure serializer struct is filled out
bool check_serializer(tt_serializer_t* stream);
bool check_deserializer(tt_deserializer_t* stream);

// internal helper functions for grid data serialization
void serialize_grid(tt_serializer_t* stream, scene_state_t* scene);
void deserialize_grid(tt_deserializer_t* stream, scene_state_t* scene, char c);


void serialize_scene(tt_serializer_t* stream, scene_state_t* scene,
                     char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]) {
    if (!check_serializer(stream)) { return; }

    char blank = 0;
    for (int l = 0; l < SCENE_TEXT_LINES; l++) {
        size_t line_length = strlen((*text)[l]);
        if (line_length > 0) {
            stream->write_buffer(stream->data, (uint8_t*)((*text)[l]),
                                 line_length);
            stream->write_char(stream->data, '\n');
            blank = 0;
        }
        else if (!blank) {
            stream->write_char(stream->data, '\n');
            blank = 1;
        }
    }

    char input[36];
    for (int s = 0; s < EDITABLE_SCRIPT_COUNT; s++) {
        stream->write_char(stream->data, '\n');
        stream->write_char(stream->data, '\n');
        stream->write_char(stream->data, '#');
        if (s == METRO_SCRIPT)
            stream->write_char(stream->data, 'M');
        else if (s == INIT_SCRIPT)
            stream->write_char(stream->data, 'I');
        else if (s == 8)
            stream->write_char(stream->data, 'N');
        else if (s == 9)
            stream->write_char(stream->data, 'O');
        else if (s == 10)
            stream->write_char(stream->data, 'Q');
        else if (s == 11)
            stream->write_char(stream->data, 'R');
        else if (s == 12)
            stream->write_char(stream->data, 'S');
        else if (s == 13)
            stream->write_char(stream->data, 'T');
        else if (s == 14)
            stream->write_char(stream->data, 'U');
        else if (s == 15)
            stream->write_char(stream->data, 'V');
        else if (s == 16)
            stream->write_char(stream->data, 'W');
        else if (s == 17)
            stream->write_char(stream->data, 'X');
        else if (s == 18)
            stream->write_char(stream->data, 'Y');
        else if (s == 19)
            stream->write_char(stream->data, 'Z');
        else
            stream->write_char(stream->data, s + 49);

        for (int l = 0; l < ss_get_script_len(scene, s); l++) {
            stream->write_char(stream->data, '\n');
            print_command(ss_get_script_command(scene, s, l), input);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
        }
    }

    stream->write_char(stream->data, '\n');
    stream->write_char(stream->data, '\n');
    stream->write_char(stream->data, '#');
    stream->write_char(stream->data, 'P');
    stream->write_char(stream->data, '\n');

    for (int b = 0; b < PATTERN_COUNT; b++) {
        itoa(ss_get_pattern_len(scene, b), input, 10);
        stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
        if (b == PATTERN_COUNT - 1)
            stream->write_char(stream->data, '\n');
        else
            stream->write_char(stream->data, '\t');
    }

    for (int b = 0; b < PATTERN_COUNT; b++) {
        itoa(ss_get_pattern_wrap(scene, b), input, 10);
        stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
        if (b == PATTERN_COUNT - 1)
            stream->write_char(stream->data, '\n');
        else
            stream->write_char(stream->data, '\t');
    }

    for (int b = 0; b < PATTERN_COUNT; b++) {
        itoa(ss_get_pattern_start(scene, b), input, 10);
        stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
        if (b == PATTERN_COUNT - 1)
            stream->write_char(stream->data, '\n');
        else
            stream->write_char(stream->data, '\t');
    }

    for (int b = 0; b < PATTERN_COUNT; b++) {
        itoa(ss_get_pattern_end(scene, b), input, 10);
        stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
        if (b == PATTERN_COUNT - 1)
            stream->write_char(stream->data, '\n');
        else
            stream->write_char(stream->data, '\t');
    }

    stream->write_char(stream->data, '\n');

    for (int l = 0; l < 64; l++) {
        for (int b = 0; b < PATTERN_COUNT; b++) {
            itoa(ss_get_pattern_val(scene, b, l), input, 10);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
            if (b == PATTERN_COUNT - 1)
                stream->write_char(stream->data, '\n');
            else
                stream->write_char(stream->data, '\t');
        }
    }

    // pattern durations (#D). Only emitted when any cell holds a
    // non-default duration — keeps existing presets byte-identical
    // through round-trip and makes the section forward-compatible (older
    // firmware will treat #D as an unknown section letter and skip it).
    bool any_dur_set = false;
    for (int b = 0; b < PATTERN_COUNT && !any_dur_set; b++) {
        for (int l = 0; l < PATTERN_LENGTH; l++) {
            if (ss_get_pattern_dur(scene, b, l) != 1) {
                any_dur_set = true;
                break;
            }
        }
    }
    if (any_dur_set) {
        stream->write_char(stream->data, '\n');
        stream->write_char(stream->data, '#');
        stream->write_char(stream->data, 'D');
        stream->write_char(stream->data, '\n');

        for (int l = 0; l < 64; l++) {
            for (int b = 0; b < PATTERN_COUNT; b++) {
                itoa(ss_get_pattern_dur(scene, b, l), input, 10);
                stream->write_buffer(stream->data, (uint8_t*)input,
                                     strlen(input));
                if (b == PATTERN_COUNT - 1)
                    stream->write_char(stream->data, '\n');
                else
                    stream->write_char(stream->data, '\t');
            }
        }
    }

    // custom pattern (#C). Per-column metadata rows (len, wrap, start, end)
    // then CUSTOM_PATTERN_LENGTH value rows, 8 columns each — same table shape
    // as #P. Column playhead idx is runtime, not serialized (as with #P).
    // Only emitted when any cell holds non-default state — keeps existing
    // presets byte-identical through round-trip and is forward-compatible
    // (older firmware treats #C as an unknown section letter and skips it).
    bool any_cp_set = false;
    for (int b = 0; b < CUSTOM_PATTERN_WIDTH && !any_cp_set; b++) {
        if (ss_get_cp_len(scene, b) != 0 || ss_get_cp_wrap(scene, b) != 1 ||
            ss_get_cp_start(scene, b) != 0 ||
            ss_get_cp_end(scene, b) != CUSTOM_PATTERN_LENGTH - 1) {
            any_cp_set = true;
            break;
        }
        for (int l = 0; l < CUSTOM_PATTERN_LENGTH; l++) {
            if (ss_get_cp_val(scene, b, l) != 0) {
                any_cp_set = true;
                break;
            }
        }
    }
    if (any_cp_set) {
        stream->write_char(stream->data, '\n');
        stream->write_char(stream->data, '#');
        stream->write_char(stream->data, 'C');
        stream->write_char(stream->data, '\n');

        for (int b = 0; b < CUSTOM_PATTERN_WIDTH; b++) {
            itoa(ss_get_cp_len(scene, b), input, 10);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
            stream->write_char(stream->data,
                               b == CUSTOM_PATTERN_WIDTH - 1 ? '\n' : '\t');
        }
        for (int b = 0; b < CUSTOM_PATTERN_WIDTH; b++) {
            itoa(ss_get_cp_wrap(scene, b), input, 10);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
            stream->write_char(stream->data,
                               b == CUSTOM_PATTERN_WIDTH - 1 ? '\n' : '\t');
        }
        for (int b = 0; b < CUSTOM_PATTERN_WIDTH; b++) {
            itoa(ss_get_cp_start(scene, b), input, 10);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
            stream->write_char(stream->data,
                               b == CUSTOM_PATTERN_WIDTH - 1 ? '\n' : '\t');
        }
        for (int b = 0; b < CUSTOM_PATTERN_WIDTH; b++) {
            itoa(ss_get_cp_end(scene, b), input, 10);
            stream->write_buffer(stream->data, (uint8_t*)input, strlen(input));
            stream->write_char(stream->data,
                               b == CUSTOM_PATTERN_WIDTH - 1 ? '\n' : '\t');
        }
        for (int l = 0; l < CUSTOM_PATTERN_LENGTH; l++) {
            for (int b = 0; b < CUSTOM_PATTERN_WIDTH; b++) {
                itoa(ss_get_cp_val(scene, b, l), input, 10);
                stream->write_buffer(stream->data, (uint8_t*)input,
                                     strlen(input));
                stream->write_char(stream->data,
                                   b == CUSTOM_PATTERN_WIDTH - 1 ? '\n' : '\t');
            }
        }
    }

    // serialize grid

    char fvalue[36];

    stream->write_char(stream->data, '\n');
    stream->write_char(stream->data, '#');
    stream->write_char(stream->data, 'G');
    stream->write_char(stream->data, '\n');
    for (uint16_t i = 0; i < GRID_BUTTON_COUNT; i++) {
        stream->write_char(stream->data, '0' + scene->grid.button[i].state);
        if ((i & 15) == 15) stream->write_char(stream->data, '\n');
    }
    stream->write_char(stream->data, '\n');
    for (uint16_t i = 0; i < GRID_FADER_COUNT; i++) {
        itoa(scene->grid.fader[i].value, fvalue, 10);
        stream->write_buffer(stream->data, (uint8_t*)fvalue, strlen(fvalue));
        stream->write_char(stream->data, (i & 15) == 15 ? '\n' : '\t');
    }
}

void deserialize_scene(tt_deserializer_t* stream, scene_state_t* scene,
                       char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]) {
    if (!check_deserializer(stream)) { return; }

    char c = '\n';
    uint8_t prev_cr = 0;
    uint8_t new_line = 0;
    uint8_t l = 0;
    uint8_t p = 0;
    uint8_t s = STATE_DESC, s2 = STATE_DESC;
    uint8_t script = NO_SCRIPT;
    uint8_t b = 0;
    int16_t num = 0;
    int16_t neg = 1;

    char input[32];
    memset(input, 0, sizeof(input));

    while (!stream->eof(stream->data)) {
        new_line = c == '\n';
        c = toupper(stream->read_char(stream->data));
        // stream->print_dbg_char(c);

        // deal with line endings
        // DOS: \r\n, *nix: \n, Mac: \r
        if (c == '\r') {
            c = '\n';
            prev_cr = 1;
        }
        else if (c == '\n' && prev_cr) {
            prev_cr = 0;
            continue;
        }
        else { prev_cr = 0; }

        if (c == '#' && new_line) {
            s = STATE_POUND;
            continue;
        }

        // SCENE TEXT
        if (s == STATE_DESC) {
            if (c == '\n') {
                l++;
                p = 0;
            }
            else {
                if (l < SCENE_TEXT_LINES && p < SCENE_TEXT_CHARS) {
                    (*text)[l][p] = c;
                    p++;
                }
            }
            continue;
        }

        if (s == STATE_POUND) {
            if (c == 'M') {
                script = METRO_SCRIPT;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'I') {
                script = INIT_SCRIPT;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'P') { s2 = STATE_PATTERNS; }
            else if (c == 'D') { s2 = STATE_PATTERN_DURATIONS; }
            else if (c == 'C') { s2 = STATE_CUSTOM_PATTERN; }
            else if (c == 'G') {
                grid_state = grid_count = 0;
                s2 = STATE_GRID;
            }
            else if (c == 'N') {
                script = 8;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'O') {
                script = 9;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'Q') {
                script = 10;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'R') {
                script = 11;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'S') {
                script = 12;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'T') {
                script = 13;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'U') {
                script = 14;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'V') {
                script = 15;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'W') {
                script = 16;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'X') {
                script = 17;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'Y') {
                script = 18;
                s2 = STATE_SCRIPT;
            }
            else if (c == 'Z') {
                script = 19;
                s2 = STATE_SCRIPT;
            }
            else {
                script = c - 49;
                if (script < 0 || script >= EDITABLE_SCRIPT_COUNT) {
                    script = NO_SCRIPT;
                }
                s2 = STATE_SCRIPT;
            }

            l = 0;
            p = 0;
            s = STATE_POUND_IGNORE;
            continue;
        }

        if (s == STATE_POUND_IGNORE) {
            if (c == '\n') { s = s2; }
            continue;
        }

        if (s == STATE_SCRIPT) {
            if (script == NO_SCRIPT || script < 0 ||
                script >= EDITABLE_SCRIPT_COUNT)
                continue;

            if (c != '\n') {
                if (p < 32) {
                    input[p] = c;
                    p++;
                }
            }
            else {
                if (p && l < SCRIPT_MAX_COMMANDS) {
                    tele_command_t temp;
                    temp.comment = false;
                    error_t status;
                    char error_msg[TELE_ERROR_MSG_LENGTH];
                    status = parse(input, &temp, error_msg);

                    if (status == E_OK) {
                        status = validate(&temp, error_msg);

                        if (status == E_OK) {
                            ss_overwrite_script_command(scene, script, l,
                                                        &temp);
                            l++;
                        }
                        else {
                            stream->print_dbg("\r\nvalidate: ");
                            stream->print_dbg(tele_error(status));
                            stream->print_dbg(" >> ");
                            stream->print_dbg("\r\nINPUT: ");
                            stream->print_dbg(input);
                        }
                    }
                    else {
                        stream->print_dbg("\r\nERROR: ");
                        stream->print_dbg(tele_error(status));
                        stream->print_dbg(" >> ");
                        stream->print_dbg("\r\nINPUT: ");
                        stream->print_dbg(input);
                    }

                    memset(input, 0, sizeof(input));
                    p = 0;
                }
            }
            continue;
        }

        // PATTERNS
        if (s == STATE_PATTERNS) {
            // tele_patterns[]. l wrap start end v[64]

            if (c == '\n' || c == '\t') {
                if (b < PATTERN_COUNT) {
                    if (l > 3 && (l - 4) < PATTERN_LENGTH) {
                        // bound check matters now that dur[] sits
                        // immediately after val[] in scene_pattern_t —
                        // without it, the blank line that follows the
                        // 64th value row would land at val[64] and
                        // corrupt dur[0] of pattern 0.
                        ss_set_pattern_val(scene, b, l - 4, neg * num);
                    }
                    else if (l == 0) { ss_set_pattern_len(scene, b, num); }
                    else if (l == 1) { ss_set_pattern_wrap(scene, b, num); }
                    else if (l == 2) { ss_set_pattern_start(scene, b, num); }
                    else if (l == 3) { ss_set_pattern_end(scene, b, num); }
                }

                b++;
                num = 0;
                neg = 1;

                if (c == '\n') {
                    if (p) l++;
                    if (l > 68) s = -1;
                    b = 0;
                    p = 0;
                }
            }
            else {
                if (c == '-')
                    neg = -1;
                else if (c >= '0' && c <= '9') {
                    num = num * 10 + (c - 48);
                    // stream->print_dbg("\r\nnum: ");
                    // stream->print_dbg_ulong(num);
                }
                p++;
            }
            continue;
        }

        // PATTERN DURATIONS
        if (s == STATE_PATTERN_DURATIONS) {
            // 64 rows of PATTERN_COUNT tab-separated dur values, no header.

            if (c == '\n' || c == '\t') {
                if (b < PATTERN_COUNT && l < PATTERN_LENGTH) {
                    ss_set_pattern_dur(scene, b, l, neg * num);
                }

                b++;
                num = 0;
                neg = 1;

                if (c == '\n') {
                    if (p) l++;
                    if (l >= PATTERN_LENGTH) s = -1;
                    b = 0;
                    p = 0;
                }
            }
            else {
                if (c == '-')
                    neg = -1;
                else if (c >= '0' && c <= '9') { num = num * 10 + (c - 48); }
                p++;
            }
            continue;
        }

        // CUSTOM PATTERN (#C)
        if (s == STATE_CUSTOM_PATTERN) {
            // l wrap start end then CUSTOM_PATTERN_LENGTH value rows, each
            // CUSTOM_PATTERN_WIDTH tab-separated columns. Mirrors #P.

            if (c == '\n' || c == '\t') {
                if (b < CUSTOM_PATTERN_WIDTH) {
                    if (l > 3 && (l - 4) < CUSTOM_PATTERN_LENGTH) {
                        ss_set_cp_val(scene, b, l - 4, neg * num);
                    }
                    else if (l == 0) { ss_set_cp_len(scene, b, num); }
                    else if (l == 1) { ss_set_cp_wrap(scene, b, num); }
                    else if (l == 2) { ss_set_cp_start(scene, b, num); }
                    else if (l == 3) { ss_set_cp_end(scene, b, num); }
                }

                b++;
                num = 0;
                neg = 1;

                if (c == '\n') {
                    if (p) l++;
                    if (l > 4 + CUSTOM_PATTERN_LENGTH) s = -1;
                    b = 0;
                    p = 0;
                }
            }
            else {
                if (c == '-')
                    neg = -1;
                else if (c >= '0' && c <= '9') { num = num * 10 + (c - 48); }
                p++;
            }
            continue;
        }


        if (s == STATE_GRID) {
            if (grid_state == 0) {
                if (c >= '0' && c <= '9') {
                    scene->grid.button[grid_count].state = c != '0';
                    if (++grid_count >= GRID_BUTTON_COUNT) {
                        grid_count = 0;
                        grid_state = 1;
                    }
                }
            }
            else if (grid_state == 1) {
                if (c >= '0' && c <= '9') {
                    grid_num = c - '0';
                    grid_state = 2;
                }
            }
            else if (grid_state == 2) {
                if (c >= '0' && c <= '9') {
                    grid_num = grid_num * 10 + c - '0';
                }
                else if (c == '\t' || c == '\n') {
                    if (grid_count < GRID_FADER_COUNT) {
                        scene->grid.fader[grid_count].value = grid_num;
                        grid_num = 0;
                        grid_count++;
                    }
                }
            }
            continue;
        }
    }
}

bool check_serializer(tt_serializer_t* stream) {
    return (stream && stream->write_char && stream->write_buffer &&
            stream->print_dbg);
}

bool check_deserializer(tt_deserializer_t* stream) {
    return (stream && stream->read_char && stream->eof && stream->print_dbg);
}
