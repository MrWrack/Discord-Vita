#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <string.h>
#include "input.h"

static SceCtrlData pad;
static SceCtrlData old_pad;

static SceTouchData touch_data;
static SceTouchPanelInfo panel_info;
static DvTouch touch;
static int old_touch_down = 0;

static unsigned int hold_button = 0;
static int hold_frames = 0;

static int scale_axis(int value, int min_value, int max_value, int screen_size) {
    int span = max_value - min_value;
    if (span <= 0) return 0;

    int relative = value - min_value;
    if (relative < 0) relative = 0;
    if (relative > span) relative = span;

    return (relative * (screen_size - 1)) / span;
}

int dv_input_init(void) {
    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));
    memset(&touch_data, 0, sizeof(touch_data));
    memset(&panel_info, 0, sizeof(panel_info));
    memset(&touch, 0, sizeof(touch));

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    int r = sceTouchSetSamplingState(
        SCE_TOUCH_PORT_FRONT,
        SCE_TOUCH_SAMPLING_STATE_START
    );
    if (r < 0) return r;

    r = sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel_info);
    if (r < 0) return r;

    return 0;
}

void dv_input_term(void) {
    sceTouchSetSamplingState(
        SCE_TOUCH_PORT_FRONT,
        SCE_TOUCH_SAMPLING_STATE_STOP
    );
}

void dv_input_update(void) {
    old_pad = pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);

    memset(&touch_data, 0, sizeof(touch_data));
    int tr = sceTouchPeek(
        SCE_TOUCH_PORT_FRONT,
        &touch_data,
        1
    );

    int current_touch_down =
        tr >= 0 && touch_data.reportNum > 0;

    touch.pressed = current_touch_down && !old_touch_down;
    touch.released = !current_touch_down && old_touch_down;
    touch.down = current_touch_down;

    if (current_touch_down) {
        const SceTouchReport *report = &touch_data.report[0];

        touch.x = scale_axis(
            report->x,
            panel_info.minDispX,
            panel_info.maxDispX,
            960
        );
        touch.y = scale_axis(
            report->y,
            panel_info.minDispY,
            panel_info.maxDispY,
            544
        );
    }

    old_touch_down = current_touch_down;

    /*
     * One-button repeat. Navigation becomes:
     * immediate press -> short pause -> regular repeats.
     */
    unsigned int nav =
        pad.buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN |
                       SCE_CTRL_LEFT | SCE_CTRL_RIGHT);

    if (!nav) {
        hold_button = 0;
        hold_frames = 0;
    } else {
        unsigned int current = 0;
        if (nav & SCE_CTRL_UP) current = SCE_CTRL_UP;
        else if (nav & SCE_CTRL_DOWN) current = SCE_CTRL_DOWN;
        else if (nav & SCE_CTRL_LEFT) current = SCE_CTRL_LEFT;
        else if (nav & SCE_CTRL_RIGHT) current = SCE_CTRL_RIGHT;

        if (current != hold_button) {
            hold_button = current;
            hold_frames = 0;
        } else {
            hold_frames++;
        }
    }
}

const DvTouch *dv_input_touch(void) {
    return &touch;
}

int dv_input_button_pressed(unsigned int button) {
    return (pad.buttons & button) && !(old_pad.buttons & button);
}

int dv_input_button_repeat(unsigned int button) {
    if (dv_input_button_pressed(button)) return 1;
    if (hold_button != button) return 0;

    if (hold_frames < 20) return 0;
    return ((hold_frames - 20) % 5) == 0;
}
