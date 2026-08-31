#pragma once
int dv_session_load(char *out, unsigned int size);
int dv_session_save(const char *session);
void dv_session_clear(void);
