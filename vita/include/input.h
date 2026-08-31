#pragma once

typedef struct {
    int x;
    int y;
    int down;
    int pressed;
    int released;
} DvTouch;

int dv_input_init(void);
void dv_input_term(void);
void dv_input_update(void);
const DvTouch *dv_input_touch(void);

int dv_input_button_pressed(unsigned int button);
int dv_input_button_repeat(unsigned int button);
