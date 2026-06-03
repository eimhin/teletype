#ifndef _CUSTOM_TRACKER_MODE_H_
#define _CUSTOM_TRACKER_MODE_H_

#include <stdbool.h>
#include <stdint.h>

void set_custom_tracker_mode(void);
void process_custom_tracker_keys(uint8_t key, uint8_t mod_key, bool is_held_key);
void process_custom_tracker_knob(uint16_t knob, uint8_t mod_key);
uint8_t screen_refresh_custom_tracker(void);

// Called from tele_pattern_updated() so the view repaints when the XP (or any)
// pattern data changes underneath it while it is on screen.
void custom_tracker_mark_dirty(void);

#endif
