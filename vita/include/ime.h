#pragma once

int dv_ime_init(void);
void dv_ime_term(void);

/*
 * Returns:
 *  1 = user entered text
 *  0 = cancelled
 * <0 = error
 */
int dv_ime_prompt_message(char *out, unsigned int out_size);
