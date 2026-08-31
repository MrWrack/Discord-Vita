#pragma once

int dv_net_init(void);
void dv_net_term(void);

int dv_http_get(
    const char *url,
    const char *bearer,
    char *out,
    unsigned int out_size,
    int *status_code
);

int dv_http_post_json(
    const char *url,
    const char *bearer,
    const char *json,
    char *out,
    unsigned int out_size,
    int *status_code
);

int dv_http_download(
    const char *url,
    const char *bearer,
    const char *destination,
    int *status_code
);
