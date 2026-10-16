"""Exercise actual display routing for stock, native Codex and hybrid builds."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'keyboards/ergohaven/macropad/display_modes.c').read_text()


def function(name):
    match = re.search(r'^(?:static )?(?:void|int8_t|bool) ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    pos, depth = match.end(), 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[match.start():pos]


enums = re.search(r'typedef enum \{.*?\} screen_t;', source, re.S)[0]
preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum { KC_AUDIO_VOL_UP = 1, KC_KB_VOLUME_UP, KC_AUDIO_VOL_DOWN, KC_KB_VOLUME_DOWN };
static unsigned active_screen = 42, loads, wakes, key_events, encoder_events, matrix_events;
static uint32_t now = 100;
static uint32_t layer_state=1, default_layer_state=1;
static uint8_t get_highest_layer(uint32_t state) { return state > 1 ? 1 : 0; }
static bool panel_on, enabled = true, volume_active;
typedef struct {void (*load)(void);} eh_screen_t;
static void splash(void) {active_screen=1; loads++;}
static void home(void) {active_screen=2; loads++;}
static void layout(void) {active_screen=3; loads++;}
static void codex(void) {active_screen=5; loads++;}
static void volume(void) {active_screen=4; loads++;}
static eh_screen_t eh_screen_splash={splash}, eh_screen_home={home}, eh_screen_layout={layout}, eh_screen_volume={volume};
static eh_screen_t eh_screen_codex={codex};
static eh_screen_t current_screen;
static bool is_display_enabled(void) {return enabled;}
static uint32_t timer_read32(void) {return now;}
static void display_turn_on(void) {panel_on=true; wakes++;}
static void display_turn_off(void) {panel_on=false;}
static void last_matrix_activity_trigger(void) {matrix_events++;}
static bool is_hid_volume_active(void) {return volume_active;}
static void screen_layout_process_keyevent(uint8_t row,uint8_t col,bool pressed) {
    (void)row;(void)col;(void)pressed;key_events++;
}
static void screen_layout_process_encoder_event(uint8_t index,bool cw) {(void)index;(void)cw;encoder_events++;}
'''
state = '''
static uint32_t screen_timer, last_user_activity_timer;
static screen_t screen_state=SCREEN_SPLASH, change_screen_state=SCREEN_SPLASH, volume_return_state=SCREEN_HOME;
'''
body = '\n'.join(function(name) for name in [
    'codex_layout_active', 'apply_screen_state', 'display_process_matrix_press', 'display_process_keyevent',
    'volume_direction_for_keycode', 'display_process_encoder_event'])
tests = r'''
int main(void) {
    display_process_matrix_press();
    assert(panel_on && wakes && matrix_events==1 && last_user_activity_timer==now);
#if defined(CODEX_MICRO_ENABLE) && !defined(CODEX_HYBRID_ENABLE)
    assert(active_screen==42 && loads==0);
#elif defined(CODEX_HYBRID_ENABLE)
    assert(active_screen==5 && loads==1);
#else
    assert(active_screen==3 && loads==1);
#endif
    display_process_keyevent(1,0,true);
    display_process_keyevent(1,0,false);
#ifdef CODEX_HYBRID_ENABLE
    assert(key_events==0);
#else
    assert(key_events==2);
#endif
    volume_active=true;
    now=200;
    display_process_encoder_event(0,true,KC_AUDIO_VOL_UP);
#ifdef CODEX_HYBRID_ENABLE
    assert(active_screen==5 && loads==1);
    // Switch renderers even though both screens are in LAYOUT mode.
    layer_state=2;
    display_process_matrix_press();
    assert(active_screen==3 && loads==2);
    display_process_encoder_event(0,true,KC_AUDIO_VOL_UP);
    assert(active_screen==4 && volume_return_state==SCREEN_LAYOUT);
    layer_state=1;
    display_process_matrix_press();
    assert(active_screen==5);
    change_screen_state=SCREEN_HOME;
    apply_screen_state();
    assert(active_screen==2 && panel_on);
    display_process_encoder_event(0,true,0);
    assert(active_screen==5 && panel_on);
    change_screen_state=SCREEN_OFF;
    apply_screen_state();
    assert(!panel_on);
    display_process_matrix_press();
    assert(active_screen==5 && panel_on);
    // Repeated presses cannot reload or replace the task screen.
    unsigned previous_loads=loads;
    display_process_matrix_press();
    assert(loads==previous_loads);
    layer_state=2;
    apply_screen_state();
    assert(active_screen==3);
#elif defined(CODEX_MICRO_ENABLE)
    assert(active_screen==42 && loads==0);
#else
    assert(active_screen==4 && loads==2 && volume_return_state==SCREEN_LAYOUT);
    display_process_matrix_press();
    assert(active_screen==3 && loads==3);
#endif
    puts("layer routing, key/encoder wake, standby and native screen ownership: PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='codex-screen-owner-') as tmp:
    tmp = Path(tmp)
    (tmp / 'routing.c').write_text(preamble + enums + state + body + tests)
    for name, flags in [('stock', []), ('codex', ['-DCODEX_MICRO_ENABLE']),
                        ('hybrid', ['-DCODEX_MICRO_ENABLE', '-DCODEX_HYBRID_ENABLE'])]:
        exe = tmp / name
        # Stock screen stubs intentionally go unused in the custom-screen build.
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', '-Wno-unused-variable',
                        '-fsanitize=address,undefined', *flags, str(tmp / 'routing.c'), '-o', str(exe)], check=True)
        print(name, flush=True)
        subprocess.run([str(exe)], check=True)
