#include "custom_tracker_mode.h"

// this
#include "globals.h"
#include "keyboard_helper.h"

// tables
#include "table.h"

// teletype
#include "teletype.h"
#include "teletype_io.h"

// libavr32
#include "font.h"
#include "region.h"
#include "util.h"

// asf
#include "conf_usb_host.h"  // needed in order to include "usb_protocol_hid.h"
#include "usb_protocol_hid.h"

// The custom tracker edits the single XP pattern: CUSTOM_PATTERN_WIDTH columns
// (each an independent track) x CUSTOM_PATTERN_LENGTH indices. All columns are
// shown at once; the index axis scrolls vertically with CT_VISIBLE rows on
// screen, exactly like the PATTERN view.
#define CT_WIDTH CUSTOM_PATTERN_WIDTH
#define CT_LENGTH CUSTOM_PATTERN_LENGTH
#define CT_VISIBLE 8
#define CT_MAX_OFFSET (CT_LENGTH - CT_VISIBLE)
// Right-edge x for column c's right-aligned value text. 7 columns on a 17px
// pitch, right-anchored to match the regular pattern tracker: column 6's text
// ends at x=124 and its start/end + playhead marker (CT_COLX(6)+2) at x=126,
// leaving x=127 empty exactly as pattern_mode does. The 17px pitch (up from 15
// when there were 8 columns) gives each column room for a 3-digit value plus
// the marker. Markers sit 2px right of the text; any slack falls left of column
// 0, by the flush-left scrollbar.
#define CT_COLX(c) (22 + (c)*17)
// Scrollbar: the full 64px bar represents CT_LENGTH indices.
#define CT_PX_PER_IDX (64 / CT_LENGTH)

static int16_t value_copy_buffer;
static uint8_t column;  // 0..CT_WIDTH-1
static uint8_t base;    // 0..CT_VISIBLE-1
static uint8_t offset;  // 0..CT_MAX_OFFSET; base + offset is the index

static bool dirty;
static bool editing_number;
static int32_t edit_buffer;
static bool edit_negative;
static uint8_t ct_view;  // 0 = value grid, 1 = duration grid

static inline uint8_t cur_idx(void) {
    return base + offset;
}

// ct_get_cell / ct_set_cell dispatch every cell access through the view flag,
// so the tracker edits either the value or the duration grid. Mirrors
// pm_get_cell / pm_set_cell in pattern_mode.c. ss_set_cp_dur clamps dur < 1
// to 1.
static inline int16_t ct_get_cell(scene_state_t *ss, size_t col, size_t idx) {
    return ct_view ? ss_get_cp_dur(ss, col, idx) : ss_get_cp_val(ss, col, idx);
}

static inline void ct_set_cell(scene_state_t *ss, size_t col, size_t idx,
                               int16_t v) {
    if (ct_view)
        ss_set_cp_dur(ss, col, idx, v);
    else
        ss_set_cp_val(ss, col, idx, v);
}

// teletype_io.h's tele_pattern_updated() (in pattern_mode.c) calls this so the
// custom tracker repaints when XP / pattern data changes while it is shown.
void custom_tracker_mark_dirty() {
    dirty = true;
}

void set_custom_tracker_mode() {
    dirty = true;
    editing_number = false;
    edit_negative = false;
    edit_buffer = 0;
    ct_view = 0;
}

static void ct_up(void) {
    editing_number = false;
    if (base)
        base--;
    else if (offset)
        offset--;
    dirty = true;
}

static void ct_down(void) {
    editing_number = false;
    base++;
    if (base == CT_VISIBLE) {
        base = CT_VISIBLE - 1;
        if (offset < CT_MAX_OFFSET) { offset++; }
    }
    dirty = true;
}

static int16_t transpose_n_value(int16_t value, int8_t interval) {
    uint8_t last_note = 127;
    if (interval > last_note) { interval = last_note; }
    else if (interval < -last_note) { interval = -last_note; }
    if (value > table_n[last_note]) {
        uint8_t idx = last_note;
        if (interval < 0) idx++;
        return table_n[(idx + interval) % (last_note + 1)];
    }
    int16_t new_value = 0;
    for (int i = 0; i <= last_note; i++) {
        if (table_n[i] >= value) {
            int8_t j = i + interval;
            if (table_n[i] > value && interval > 0)
                j--;  // quantize to lower note
            if (j > last_note) { j = j % last_note + 1; }
            else if (j < 0) { j = j + last_note + 1; }
            new_value = table_n[j];
            break;
        }
    }
    return new_value;
}

static void note_nudge(int8_t semitones) {
    if (editing_number) { edit_buffer = transpose_n_value(edit_buffer, semitones); }
    else {
        int16_t v = ct_get_cell(&scene_state, column, cur_idx());
        ct_set_cell(&scene_state, column, cur_idx(),
                    transpose_n_value(v, semitones));
    }
    dirty = true;
}

void process_custom_tracker_keys(uint8_t k, uint8_t m, bool is_held_key) {
    (void)is_held_key;
    // <down>: move down
    if (match_no_mod(m, k, HID_DOWN)) { ct_down(); }
    // alt-<down>: jump to the bottom page
    else if (match_alt(m, k, HID_DOWN)) {
        editing_number = false;
        offset = CT_MAX_OFFSET;
        dirty = true;
    }
    // <up>: move up
    else if (match_no_mod(m, k, HID_UP)) { ct_up(); }
    // alt-<up>: jump to the top page
    else if (match_alt(m, k, HID_UP)) {
        editing_number = false;
        offset = 0;
        dirty = true;
    }
    // <left>: move to previous column (wraps around)
    else if (match_no_mod(m, k, HID_LEFT)) {
        editing_number = false;
        column = (column + CT_WIDTH - 1) % CT_WIDTH;
        dirty = true;
    }
    // alt-<left>: move to the very top
    else if (match_alt(m, k, HID_LEFT)) {
        editing_number = false;
        base = 0;
        offset = 0;
        dirty = true;
    }
    // <right>: move to next column (wraps around)
    else if (match_no_mod(m, k, HID_RIGHT)) {
        editing_number = false;
        column = (column + 1) % CT_WIDTH;
        dirty = true;
    }
    // alt-<right>: move to the very bottom
    else if (match_alt(m, k, HID_RIGHT)) {
        editing_number = false;
        base = CT_VISIBLE - 1;
        offset = CT_MAX_OFFSET;
        dirty = true;
    }
    // [: decrement by 1
    else if (match_no_mod(m, k, HID_OPEN_BRACKET)) {
        if (editing_number) {
            if (edit_buffer == INT16_MIN)
                edit_buffer = INT16_MAX;
            else
                edit_buffer -= 1;
        }
        else {
            int16_t v = ct_get_cell(&scene_state, column, cur_idx());
            ct_set_cell(&scene_state, column, cur_idx(),
                        v == INT16_MIN ? INT16_MAX : v - 1);
        }
        dirty = true;
    }
    // ]: increment by 1
    else if (match_no_mod(m, k, HID_CLOSE_BRACKET)) {
        if (editing_number) {
            if (edit_buffer == INT16_MAX)
                edit_buffer = INT16_MIN;
            else
                edit_buffer += 1;
        }
        else {
            int16_t v = ct_get_cell(&scene_state, column, cur_idx());
            ct_set_cell(&scene_state, column, cur_idx(),
                        v == INT16_MAX ? INT16_MIN : v + 1);
        }
        dirty = true;
    }
    // alt-[ / alt-]: nudge by a semitone
    else if (match_alt(m, k, HID_OPEN_BRACKET)) { note_nudge(-1); }
    else if (match_alt(m, k, HID_CLOSE_BRACKET)) { note_nudge(1); }
    // ctrl-[ / ctrl-]: nudge by a fifth
    else if (match_ctrl(m, k, HID_OPEN_BRACKET)) { note_nudge(-7); }
    else if (match_ctrl(m, k, HID_CLOSE_BRACKET)) { note_nudge(7); }
    // sh-[ / sh-]: nudge by an octave
    else if (match_shift(m, k, HID_OPEN_BRACKET)) { note_nudge(-12); }
    else if (match_shift(m, k, HID_CLOSE_BRACKET)) { note_nudge(12); }
    // alt-<0-9>: transpose up by numeric semitones
    else if (mod_only_alt(m) && k >= HID_1 && k <= HID_0) {
        uint8_t n = (k - HID_1 + 1);
        if (n == 1) n = 11;
        note_nudge(n);
    }
    // sh-alt-<0-9>: transpose down by numeric semitones
    else if (mod_only_shift_alt(m) && k >= HID_1 && k <= HID_0) {
        uint8_t n = (k - HID_1 + 1);
        if (n == 1) n = 11;
        note_nudge(-n);
    }
    // <backspace>: delete a digit
    else if (match_no_mod(m, k, HID_BACKSPACE)) {
        if (editing_number)
            edit_buffer /= 10;
        else {
            editing_number = true;
            edit_buffer = ct_get_cell(&scene_state, column, cur_idx()) / 10;
        }
        dirty = true;
    }
    // shift-<backspace>: delete an entry, shift numbers up
    else if (match_shift(m, k, HID_BACKSPACE)) {
        editing_number = false;
        for (size_t i = cur_idx(); i < CT_LENGTH - 1; i++) {
            ct_set_cell(&scene_state, column, i,
                        ct_get_cell(&scene_state, column, i + 1));
        }
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (l > cur_idx()) ss_set_cp_len(&scene_state, column, l - 1);
        dirty = true;
    }
    // <enter>: commit edit, extend length
    else if (match_no_mod(m, k, HID_ENTER)) {
        if (editing_number) {
            ct_set_cell(&scene_state, column, cur_idx(), edit_buffer);
            editing_number = false;
            edit_negative = false;
        }
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (cur_idx() == l && l < CT_LENGTH)
            ss_set_cp_len(&scene_state, column, l + 1);
        dirty = true;
    }
    // shift-<enter>: duplicate entry and shift downwards
    else if (match_shift(m, k, HID_ENTER)) {
        if (editing_number) {
            ct_set_cell(&scene_state, column, cur_idx(), edit_buffer);
            editing_number = false;
            edit_negative = false;
        }
        for (int i = CT_LENGTH - 1; i > cur_idx(); i--) {
            ct_set_cell(&scene_state, column, i,
                        ct_get_cell(&scene_state, column, i - 1));
        }
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (cur_idx() == l && l < CT_LENGTH)
            ss_set_cp_len(&scene_state, column, l + 1);
        dirty = true;
    }
    // alt-x: cut value
    else if (match_alt(m, k, HID_X)) {
        editing_number = false;
        value_copy_buffer = ct_get_cell(&scene_state, column, cur_idx());
        for (int i = cur_idx(); i < CT_LENGTH - 1; i++) {
            ct_set_cell(&scene_state, column, i,
                        ct_get_cell(&scene_state, column, i + 1));
        }
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (l > cur_idx()) ss_set_cp_len(&scene_state, column, l - 1);
        dirty = true;
    }
    // alt-c: copy value
    else if (match_alt(m, k, HID_C)) {
        if (editing_number)
            value_copy_buffer = edit_buffer;
        else
            value_copy_buffer = ct_get_cell(&scene_state, column, cur_idx());
    }
    // alt-v: paste value
    else if (match_alt(m, k, HID_V)) {
        editing_number = false;
        ct_set_cell(&scene_state, column, cur_idx(), value_copy_buffer);
        dirty = true;
    }
    // shift-alt-v: insert value
    else if (match_shift_alt(m, k, HID_V)) {
        editing_number = false;
        for (int i = CT_LENGTH - 1; i > cur_idx(); i--) {
            ct_set_cell(&scene_state, column, i,
                        ct_get_cell(&scene_state, column, i - 1));
        }
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (l >= cur_idx() && l < CT_LENGTH - 1)
            ss_set_cp_len(&scene_state, column, l + 1);
        ct_set_cell(&scene_state, column, cur_idx(), value_copy_buffer);
        dirty = true;
    }
    // shift-l: set length to current position
    else if (match_shift(m, k, HID_L)) {
        editing_number = false;
        ss_set_cp_len(&scene_state, column, cur_idx() + 1);
        dirty = true;
    }
    // alt-l: go to current length entry
    else if (match_alt(m, k, HID_L)) {
        editing_number = false;
        uint16_t l = ss_get_cp_len(&scene_state, column);
        if (l) {
            offset = ((l - 1) >> 3) << 3;
            base = (l - 1) & 0x7;
            if (offset > CT_MAX_OFFSET) offset = CT_MAX_OFFSET;
        }
        else {
            offset = 0;
            base = 0;
        }
        dirty = true;
    }
    // shift-s: set start to current position
    else if (match_shift(m, k, HID_S)) {
        editing_number = false;
        ss_set_cp_start(&scene_state, column, cur_idx());
        dirty = true;
    }
    // alt-s: go to start entry
    else if (match_alt(m, k, HID_S)) {
        editing_number = false;
        int16_t start = ss_get_cp_start(&scene_state, column);
        offset = ((start >> 3) << 3);
        if (offset > CT_MAX_OFFSET) offset = CT_MAX_OFFSET;
        base = start & 0x7;
        dirty = true;
    }
    // shift-e: set end to current position
    else if (match_shift(m, k, HID_E)) {
        editing_number = false;
        ss_set_cp_end(&scene_state, column, cur_idx());
        dirty = true;
    }
    // alt-e: go to end entry
    else if (match_alt(m, k, HID_E)) {
        editing_number = false;
        int16_t end = ss_get_cp_end(&scene_state, column);
        offset = ((end >> 3) << 3);
        if (offset > CT_MAX_OFFSET) offset = CT_MAX_OFFSET;
        base = end & 0x7;
        dirty = true;
    }
    // -: negate value
    else if (match_no_mod(m, k, HID_UNDERSCORE)) {
        int16_t v = ct_get_cell(&scene_state, column, cur_idx());
        if (v == 0 && !editing_number) {
            editing_number = true;
            edit_buffer = 0;
        }
        if (editing_number) {
            if (edit_buffer == 0)
                edit_negative = !edit_negative;
            else
                edit_buffer *= -1;
        }
        else { ct_set_cell(&scene_state, column, cur_idx(), -v); }
        dirty = true;
    }
    // <space>: toggle non-zero to zero, and zero to 1
    else if (match_no_mod(m, k, HID_SPACEBAR)) {
        editing_number = false;
        if (ct_get_cell(&scene_state, column, cur_idx()))
            ct_set_cell(&scene_state, column, cur_idx(), 0);
        else
            ct_set_cell(&scene_state, column, cur_idx(), 1);
        dirty = true;
    }
    // ~: toggle between value grid (val[]) and duration grid (dur[]). Mirrors
    // pattern_mode's tilde toggle. Cells visibly flip on toggle since fresh-
    // scene defaults are 0 (val) and 1 (dur), so no header indicator is needed.
    else if (match_no_mod(m, k, HID_TILDE)) {
        ct_view = ct_view ? 0 : 1;
        editing_number = false;
        edit_negative = false;
        edit_buffer = 0;
        dirty = true;
    }
    // 0-9: numeric entry
    else if (no_mod(m) && k >= HID_1 && k <= HID_0) {
        if (!editing_number) {
            editing_number = true;
            edit_buffer = 0;
        }
        uint8_t n = (k - HID_1 + 1) % 10;
        uint32_t old_buffer = edit_buffer;

        edit_buffer *= 10;
        if (edit_buffer == 0) { edit_buffer = n; }
        else if (edit_buffer < 0) {
            edit_buffer -= n;
            if (edit_buffer < INT16_MIN) edit_buffer = old_buffer;
        }
        else {
            edit_buffer += n;
            if (edit_buffer > INT16_MAX) edit_buffer = old_buffer;
        }
        if (edit_negative && edit_buffer != 0) {
            edit_negative = false;
            edit_buffer *= -1;
        }
        dirty = true;
    }
    if (!editing_number) edit_negative = false;
}

void process_custom_tracker_knob(uint16_t knob, uint8_t m) {
    if (mod_only_ctrl_alt(m)) {
        ct_set_cell(&scene_state, column, cur_idx(), knob >> 7);
        dirty = true;
    }
    else if (mod_only_shift_ctrl(m)) {
        ct_set_cell(&scene_state, column, cur_idx(), knob >> 2);
        dirty = true;
    }
}

uint8_t screen_refresh_custom_tracker() {
    if (!dirty) { return 0; }

    char s[32];
    for (uint8_t y = 0; y < CT_VISIBLE; y++) {
        region_fill(&line[y], 0);

        for (uint8_t c = 0; c < CT_WIDTH; c++) {
            uint8_t a = 1;
            if (ss_get_cp_len(&scene_state, c) > y + offset) a = 6;

            itoa(ct_get_cell(&scene_state, c, y + offset), s, 10);
            font_string_region_clip_right(&line[y], s, CT_COLX(c), 0, a, 0);

            if (y + offset >= ss_get_cp_start(&scene_state, c)
                && y + offset <= ss_get_cp_end(&scene_state, c)) {
                for (uint8_t i = 0; i < 8; i += 2) {
                    line[y].data[i * 128 + CT_COLX(c) + 2] = 1;
                }
            }

            if (y + offset == ss_get_cp_idx(&scene_state, c)) {
                line[y].data[2 * 128 + CT_COLX(c) + 2] = 11;
                line[y].data[3 * 128 + CT_COLX(c) + 2] = 11;
                line[y].data[4 * 128 + CT_COLX(c) + 2] = 11;
            }
        }
    }

    if (editing_number) {
        font_string_region_clip_right(&line[base], "   ", CT_COLX(column), 0,
                                      0xf, 0);
        if (edit_negative && edit_buffer == 0)
            font_string_region_clip_right(&line[base], "-0", CT_COLX(column), 0,
                                          0xf, 0);
        else {
            itoa(edit_buffer, s, 10);
            font_string_region_clip_right(&line[base], s, CT_COLX(column), 0,
                                          0xf, 0);
        }
    }
    else {
        itoa(ct_get_cell(&scene_state, column, cur_idx()), s, 10);
        font_string_region_clip_right(&line[base], s, CT_COLX(column), 0, 0xf, 0);
    }

    // scrollbar: dim dotted full-height bar, with the visible window of
    // CT_VISIBLE indices highlighted brighter.
    for (uint8_t py = 0; py < 64; py += 2)
        line[py >> 3].data[(py & 0x7) * 128] = 1;
    for (uint8_t i = offset; i < offset + CT_VISIBLE; i++) {
        for (uint8_t d = 0; d < CT_PX_PER_IDX; d++) {
            uint8_t py = i * CT_PX_PER_IDX + d;
            if (py < 64) line[py >> 3].data[(py & 0x7) * 128] = 6;
        }
    }

    dirty = false;

    return 0xFF;
}
