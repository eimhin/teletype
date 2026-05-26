#include "pattern_mode.h"

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

// Invariant: PATTERN_COUNT must be a multiple of PATTERNS_PER_PAGE. The OLED
// tracker renders a full page of PATTERNS_PER_PAGE columns and the circular
// arrow wrap targets column PATTERNS_PER_PAGE - 1 on the last page; a partial
// last page would index ss->patterns[] out of bounds (state.c accessors do
// no clamping). If PATTERN_COUNT is bumped to a non-multiple, the per-page
// loops here and the wrap target need to clamp against PATTERN_COUNT.
#define PATTERNS_PER_PAGE 4
#define PATTERN_PAGE_COUNT \
    ((PATTERN_COUNT + PATTERNS_PER_PAGE - 1) / PATTERNS_PER_PAGE)

static int16_t value_copy_buffer;
static uint8_t pattern;  // column within current page (0..PATTERNS_PER_PAGE-1)
static uint8_t pattern_page;  // 0..PATTERN_PAGE_COUNT-1
static uint8_t base;     // base + offset determine what we are editting
static uint8_t offset;

static bool dirty;
static bool editing_number;
static int32_t edit_buffer;
static bool edit_negative;

// PATTERN mode has two views over each cell: val (default) and dur. The
// duration view is toggled by `~` (HID_TILDE) inside process_pattern_keys.
// pm_get_cell / pm_set_cell dispatch every cell access through the view
// so the existing renderer and key handlers all operate on the active
// array without per-call conditionals.
//   pattern_view 0 -> val[] (notes / arbitrary values)
//   pattern_view 1 -> dur[] (per-cell dwell ticks used by P.STEP)
static uint8_t pattern_view = 0;

static inline uint8_t abs_pattern(void) {
    return pattern_page * PATTERNS_PER_PAGE + pattern;
}

static inline int16_t pm_get_cell(scene_state_t *ss, size_t pn, size_t idx) {
    return pattern_view ? ss_get_pattern_dur(ss, pn, idx)
                        : ss_get_pattern_val(ss, pn, idx);
}

static inline void pm_set_cell(scene_state_t *ss, size_t pn, size_t idx,
                               int16_t v) {
    if (pattern_view)
        ss_set_pattern_dur(ss, pn, idx, v);  // clamps < 1 to 1
    else
        ss_set_pattern_val(ss, pn, idx, v);
}

// teletype_io.h
void tele_pattern_updated() {
    dirty = true;
}

void set_pattern_mode() {
    dirty = true;
    editing_number = false;
    edit_negative = false;
    edit_buffer = 0;
}

uint8_t get_pattern_offset() {
    return offset;
}

void set_pattern_offset(uint8_t o) {
    base = 0;
    offset = o;
    dirty = true;
}

uint8_t get_pattern_page() {
    return pattern_page;
}

void set_pattern_page(uint8_t page) {
    if (page >= PATTERN_PAGE_COUNT) page = PATTERN_PAGE_COUNT - 1;
    pattern_page = page;
    dirty = true;
}

void set_pattern_selected_value(uint8_t p, uint8_t offset) {
    if (p >= PATTERN_COUNT) p = PATTERN_COUNT - 1;
    pattern_page = p / PATTERNS_PER_PAGE;
    pattern = p % PATTERNS_PER_PAGE;
    base = offset;
    dirty = true;
}

void pattern_up() {
    editing_number = false;
    if (base)
        base--;
    else if (offset)
        offset--;
    dirty = true;
}

void pattern_down() {
    editing_number = false;
    base++;
    if (base == 8) {
        base = 7;
        if (offset < 56) { offset++; }
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

void note_nudge(int8_t semitones) {
    if (editing_number) {
        edit_buffer = transpose_n_value(edit_buffer, semitones);
    }
    else {
        int16_t pattern_val =
            pm_get_cell(&scene_state, abs_pattern(), base + offset);
        int16_t new_val = transpose_n_value(pattern_val, semitones);
        pm_set_cell(&scene_state, abs_pattern(), base + offset, new_val);
    }
    dirty = true;
}

void process_pattern_keys(uint8_t k, uint8_t m, bool is_held_key) {
    // <down>: move down
    if (match_no_mod(m, k, HID_DOWN)) { pattern_down(); }
    // alt-<down>: move a page down
    else if (match_alt(m, k, HID_DOWN)) {
        editing_number = false;
        if (offset < 48)
            offset += 8;
        else {
            offset = 56;
            base = 7;
        }
        dirty = true;
    }
    // <up>: move up
    else if (match_no_mod(m, k, HID_UP)) { pattern_up(); }
    // alt-<up>: move a page up
    else if (match_alt(m, k, HID_UP)) {
        editing_number = false;
        if (offset > 8) { offset -= 8; }
        else {
            offset = 0;
            base = 0;
        }
        dirty = true;
    }
    // <left>: move left (auto-wraps to previous page; wraps around end-to-end)
    else if (match_no_mod(m, k, HID_LEFT)) {
        editing_number = false;
        if (pattern > 0) { pattern--; }
        else if (pattern_page > 0) {
            pattern_page--;
            pattern = PATTERNS_PER_PAGE - 1;
        }
        else {
            pattern_page = PATTERN_PAGE_COUNT - 1;
            pattern = PATTERNS_PER_PAGE - 1;
        }
        dirty = true;
    }
    // alt-<left>: move to the very left
    else if (match_alt(m, k, HID_LEFT)) {
        editing_number = false;
        base = 0;
        offset = 0;
        dirty = true;
    }
    // shift-<left>: flip to previous pattern page (cursor column unchanged)
    else if (match_shift(m, k, HID_LEFT)) {
        editing_number = false;
        if (PATTERN_PAGE_COUNT > 1) {
            pattern_page = (pattern_page + PATTERN_PAGE_COUNT - 1)
                           % PATTERN_PAGE_COUNT;
            dirty = true;
        }
    }
    // <right>: move right (auto-wraps to next page; wraps around end-to-end)
    else if (match_no_mod(m, k, HID_RIGHT)) {
        editing_number = false;
        if (pattern < PATTERNS_PER_PAGE - 1) { pattern++; }
        else if (pattern_page < PATTERN_PAGE_COUNT - 1) {
            pattern_page++;
            pattern = 0;
        }
        else {
            pattern_page = 0;
            pattern = 0;
        }
        dirty = true;
    }
    // alt-<right>: move to the very right
    else if (match_alt(m, k, HID_RIGHT)) {
        editing_number = false;
        base = 7;
        offset = 56;
        dirty = true;
    }
    // shift-<right>: flip to next pattern page (cursor column unchanged)
    else if (match_shift(m, k, HID_RIGHT)) {
        editing_number = false;
        if (PATTERN_PAGE_COUNT > 1) {
            pattern_page = (pattern_page + 1) % PATTERN_PAGE_COUNT;
            dirty = true;
        }
    }
    // [: decrement by 1
    else if (match_no_mod(m, k, HID_OPEN_BRACKET)) {
        if (editing_number) {
            if (edit_buffer == INT16_MIN)
                edit_buffer = INT16_MAX;
            else
                edit_buffer -= 1;
            dirty = true;
        }
        else {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), base + offset);
            if (v == INT16_MIN)
                pm_set_cell(&scene_state, abs_pattern(), base + offset,
                            INT16_MAX);
            else
                pm_set_cell(&scene_state, abs_pattern(), base + offset, v - 1);
            dirty = true;
        }
    }
    // ]: increment by 1
    else if (match_no_mod(m, k, HID_CLOSE_BRACKET)) {
        if (editing_number) {
            if (edit_buffer == INT16_MAX)
                edit_buffer = INT16_MIN;
            else
                edit_buffer += 1;
            dirty = true;
        }
        else {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), base + offset);
            if (v == INT16_MAX)
                pm_set_cell(&scene_state, abs_pattern(), base + offset,
                            INT16_MIN);
            else
                pm_set_cell(&scene_state, abs_pattern(), base + offset, v + 1);
            dirty = true;
        }
    }
    // alt-[: decrement by 1 semitone
    else if (match_alt(m, k, HID_OPEN_BRACKET)) { note_nudge(-1); }
    // alt-]: increment by 1 semitone
    else if (match_alt(m, k, HID_CLOSE_BRACKET)) { note_nudge(1); }
    // ctrl-[: decrement by a fifth (7 semitones)
    else if (match_ctrl(m, k, HID_OPEN_BRACKET)) { note_nudge(-7); }
    // ctrl-]: increment by a fifth (7 semitones)
    else if (match_ctrl(m, k, HID_CLOSE_BRACKET)) { note_nudge(7); }
    // sh-[: decrement by 1 octave
    else if (match_shift(m, k, HID_OPEN_BRACKET)) { note_nudge(-12); }
    // sh-]: increment by 1 octave
    else if (match_shift(m, k, HID_CLOSE_BRACKET)) { note_nudge(12); }
    // alt-<0-9>: transpose up by numeric semitones
    else if (mod_only_alt(m) && k >= HID_1 && k <= HID_0) {
        uint8_t n = (k - HID_1 + 1);  // convert HID numbers to decimal,
                                      // leave 0 = 10 semitones
        if (n == 1) n = 11;  // 1 = 11 semitones since we already have alt-[ ]
        note_nudge(n);
    }
    // sh-alt-<0-9>: transpose down by numeric semitones
    else if (mod_only_shift_alt(m) && k >= HID_1 && k <= HID_0) {
        uint8_t n = (k - HID_1 + 1);  // convert HID numbers to decimal,
                                      // leave 0 = 10 semitones
        if (n == 1) n = 11;  // 1 = 11 semitones since we already have alt-[ ]
        note_nudge(-n);
    }
    // <backspace>: delete a digit
    else if (match_no_mod(m, k, HID_BACKSPACE)) {
        if (editing_number)
            edit_buffer /= 10;
        else {
            editing_number = true;
            edit_buffer =
                pm_get_cell(&scene_state, abs_pattern(), base + offset) / 10;
        }
        dirty = true;
    }
    // shift-<backspace>: delete an entry, shift numbers up
    else if (match_shift(m, k, HID_BACKSPACE)) {
        editing_number = false;
        for (size_t i = base + offset; i < 63; i++) {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), i + 1);
            pm_set_cell(&scene_state, abs_pattern(), i, v);
        }

        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (l > base + offset) ss_set_pattern_len(&scene_state, abs_pattern(), l - 1);
        dirty = true;
    }
    // <enter>: commit edit, extend pattern length
    else if (match_no_mod(m, k, HID_ENTER)) {
        // commit an edit if active
        if (editing_number) {
            pm_set_cell(&scene_state, abs_pattern(), base + offset,
                        edit_buffer);
            editing_number = false;
            edit_negative = false;
        }
        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (base + offset == l && l < 64)
            ss_set_pattern_len(&scene_state, abs_pattern(), l + 1);
        dirty = true;
    }
    // shift-<enter>: duplicate entry and shift downwards (increase length only
    // if on the entry immediately after the current length)
    else if (match_shift(m, k, HID_ENTER)) {
        // commit an edit before duplication
        if (editing_number) {
            pm_set_cell(&scene_state, abs_pattern(), base + offset,
                        edit_buffer);
            editing_number = false;
            edit_negative = false;
        }
        for (int i = 63; i > base + offset; i--) {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), i - 1);
            pm_set_cell(&scene_state, abs_pattern(), i, v);
        }
        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (base + offset == l && l < 64) {
            ss_set_pattern_len(&scene_state, abs_pattern(), l + 1);
        }
        dirty = true;
    }
    // alt-x: cut value (n.b. ctrl-x not supported)
    else if (match_alt(m, k, HID_X)) {
        editing_number = false;
        value_copy_buffer =
            pm_get_cell(&scene_state, abs_pattern(), base + offset);
        for (int i = base + offset; i < 63; i++) {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), i + 1);
            pm_set_cell(&scene_state, abs_pattern(), i, v);
        }

        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (l > base + offset) {
            ss_set_pattern_len(&scene_state, abs_pattern(), l - 1);
        }
        dirty = true;
    }
    // alt-c: copy value (n.b. ctrl-c not supported)
    else if (match_alt(m, k, HID_C)) {
        if (editing_number)
            value_copy_buffer = edit_buffer;
        else
            value_copy_buffer =
                pm_get_cell(&scene_state, abs_pattern(), base + offset);
    }
    // alt-v: paste value (n.b. ctrl-v not supported)
    else if (match_alt(m, k, HID_V)) {
        editing_number = false;
        pm_set_cell(&scene_state, abs_pattern(), base + offset,
                    value_copy_buffer);
        dirty = true;
    }
    // shift-alt-v: insert value
    else if (match_shift_alt(m, k, HID_V)) {
        editing_number = false;
        for (int i = 63; i > base + offset; i--) {
            int16_t v = pm_get_cell(&scene_state, abs_pattern(), i - 1);
            pm_set_cell(&scene_state, abs_pattern(), i, v);
        }
        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (l >= base + offset && l < 63) {
            ss_set_pattern_len(&scene_state, abs_pattern(), l + 1);
        }
        pm_set_cell(&scene_state, abs_pattern(), base + offset,
                    value_copy_buffer);
        dirty = true;
    }
    // shift-l: set length to current position
    else if (match_shift(m, k, HID_L)) {
        editing_number = false;
        ss_set_pattern_len(&scene_state, abs_pattern(), base + offset + 1);
        dirty = true;
    }
    // alt-l: go to current length entry
    else if (match_alt(m, k, HID_L)) {
        editing_number = false;
        uint16_t l = ss_get_pattern_len(&scene_state, abs_pattern());
        if (l) {
            offset = ((l - 1) >> 3) << 3;
            base = (l - 1) & 0x7;
            int8_t delta = base - 3;
            if ((offset + delta > 0) && (offset + delta < 56)) {
                offset += delta;
                base = 3;
            }
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
        ss_set_pattern_start(&scene_state, abs_pattern(), offset + base);
        dirty = true;
    }
    // alt-s: go to start entry
    else if (match_alt(m, k, HID_S)) {
        editing_number = false;
        int16_t start = ss_get_pattern_start(&scene_state, abs_pattern());
        if (start) {
            offset = (start >> 3) << 3;
            base = start & 0x7;
            int8_t delta = base - 3;
            if ((offset + delta > 0) && (offset + delta < 56)) {
                offset += delta;
                base = 3;
            }
        }
        else {
            offset = 0;
            base = 0;
        }
        dirty = true;
    }
    // shift-e: set end to current position
    else if (match_shift(m, k, HID_E)) {
        editing_number = false;
        ss_set_pattern_end(&scene_state, abs_pattern(), offset + base);
        dirty = true;
    }
    // alt-e: go to end entry
    else if (match_alt(m, k, HID_E)) {
        editing_number = false;
        int16_t end = ss_get_pattern_end(&scene_state, abs_pattern());
        if (end) {
            offset = (end >> 3) << 3;
            base = end & 0x7;
            int8_t delta = base - 3;
            if ((offset + delta > 0) && (offset + delta < 56)) {
                offset += delta;
                base = 3;
            }
        }
        else {
            offset = 0;
            base = 0;
        }
        dirty = true;
    }
    // -: negate value
    else if (match_no_mod(m, k, HID_UNDERSCORE)) {
        int16_t v = pm_get_cell(&scene_state, abs_pattern(), base + offset);
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
        else { pm_set_cell(&scene_state, abs_pattern(), base + offset, -v); }
        dirty = true;
    }
    // <space>: toggle non-zero to zero, and zero to 1
    else if (match_no_mod(m, k, HID_SPACEBAR)) {
        editing_number = false;
        if (pm_get_cell(&scene_state, abs_pattern(), base + offset))
            pm_set_cell(&scene_state, abs_pattern(), base + offset, 0);
        else
            pm_set_cell(&scene_state, abs_pattern(), base + offset, 1);
        dirty = true;
    }
    else if (match_shift(m, k, HID_2)) {
        turtle_set_shown(&scene_state.turtle,
                         !turtle_get_shown(&scene_state.turtle));
        dirty = true;
    }
    // ~: toggle between value view (val[]) and duration view (dur[]).
    // Mirrors LIVE mode's tilde idiom (live_mode.c) where ~ toggles a
    // sub-view. Cells visibly flip on toggle since fresh-scene defaults
    // are 0 (val) and 1 (dur), so no header indicator is needed.
    else if (match_no_mod(m, k, HID_TILDE)) {
        pattern_view = pattern_view ? 0 : 1;
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
        uint8_t n = (k - HID_1 + 1) % 10;  // convert HID numbers to decimal,
                                           // taking care of HID_0
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

void process_pattern_knob(uint16_t knob, uint8_t m) {
    if (mod_only_ctrl_alt(m)) {
        pm_set_cell(&scene_state, abs_pattern(), base + offset, knob >> 7);
        dirty = true;
    }
    else if (mod_only_shift_ctrl(m)) {
        pm_set_cell(&scene_state, abs_pattern(), base + offset, knob >> 2);
        dirty = true;
    }
}

uint8_t screen_refresh_pattern() {
    if (!dirty) { return 0; }

    char s[32];
    for (uint8_t y = 0; y < 8; y++) {
        region_fill(&line[y], 0);
        itoa(y + offset, s, 10);
        font_string_region_clip_right(&line[y], s, 4, 0, 0x1, 0);

        for (uint8_t x = 0; x < PATTERNS_PER_PAGE; x++) {
            uint8_t p = pattern_page * PATTERNS_PER_PAGE + x;
            uint8_t a = 1;
            if (ss_get_pattern_len(&scene_state, p) > y + offset) a = 6;

            itoa(pm_get_cell(&scene_state, p, y + offset), s, 10);
            font_string_region_clip_right(&line[y], s, (x + 1) * 30 + 4, 0, a,
                                          0);

            if (y + offset >= ss_get_pattern_start(&scene_state, p)) {
                if (y + offset <= ss_get_pattern_end(&scene_state, p)) {
                    for (uint8_t i = 0; i < 8; i += 2) {
                        line[y].data[i * 128 + (x + 1) * 30 + 6] = 1;
                    }
                }
            }

            if (y + offset == ss_get_pattern_idx(&scene_state, p)) {
                line[y].data[2 * 128 + (x + 1) * 30 + 6] = 11;
                line[y].data[3 * 128 + (x + 1) * 30 + 6] = 11;
                line[y].data[4 * 128 + (x + 1) * 30 + 6] = 11;
            }
        }
    }

    if (editing_number) {
        font_string_region_clip_right(&line[base], "      ",
                                      (pattern + 1) * 30 + 4, 0, 0xf, 0);
        if (edit_negative && edit_buffer == 0)
            font_string_region_clip_right(&line[base], "    -0",
                                          (pattern + 1) * 30 + 4, 0, 0xf, 0);
        else {
            itoa(edit_buffer, s, 10);
            font_string_region_clip_right(&line[base], s,
                                          (pattern + 1) * 30 + 4, 0, 0xf, 0);
        }
    }
    else {
        itoa(pm_get_cell(&scene_state, abs_pattern(), base + offset), s, 10);
        font_string_region_clip_right(&line[base], s, (pattern + 1) * 30 + 4, 0,
                                      0xf, 0);
    }

    if (scene_state.turtle.shown) {
        int16_t y = turtle_get_y(&scene_state.turtle);
        int16_t x = turtle_get_x(&scene_state.turtle);
        int16_t col = x - pattern_page * PATTERNS_PER_PAGE;
        if (y >= offset && y < offset + 8 && col >= 0
            && col < PATTERNS_PER_PAGE) {
            font_string_region_clip_right(&line[y - offset], "<",
                                          (col + 1) * 30 + 9, 0, 0xf, 0);
        }
    }

    for (uint8_t y = 0; y < 64; y += 2) {
        line[y >> 3].data[(y & 0x7) * 128 + 8] = 1;
    }

    for (uint8_t y = 0; y < 8; y++) {
        line[(offset + y) >> 3].data[((offset + y) & 0x7) * 128 + 8] = 6;
    }

    // Page indicator: partition the scrollbar highlight on pages beyond the
    // first. The highlight is 8 px tall (even), so a centered partition uses
    // 2 dim pixels (intensity 1, matching the scrollbar background) at
    // y_relative = 3 and 4 — splitting the highlight into 3+2+3. Page 1 stays
    // solid (current behavior).
    if (pattern_page > 0) {
        line[(offset + 3) >> 3].data[((offset + 3) & 0x7) * 128 + 8] = 1;
        line[(offset + 4) >> 3].data[((offset + 4) & 0x7) * 128 + 8] = 1;
    }

    dirty = false;

    return 0xFF;
}
