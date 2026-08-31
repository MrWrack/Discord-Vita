#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/net/https.h>
#include <psp2/io/fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "net.h"

static int g_ready = 0;
static void *g_net_mem = NULL;

static int request(
    int method,
    const char *url,
    const char *bearer,
    const char *body,
    char *out,
    unsigned int out_size,
    int *status_code
) {
    int tmpl = -1, conn = -1, req = -1;
    int result = -1;

    if (!g_ready || !url || !out || out_size < 2) return -1;
    out[0] = 0;
    if (status_code) *status_code = 0;

    unsigned int body_len = body ? (unsigned int)strlen(body) : 0;

    tmpl = sceHttpCreateTemplate("Discord Vita/1.7", SCE_HTTP_VERSION_1_1, 1);
    if (tmpl < 0) { result = tmpl; goto cleanup; }

    conn = sceHttpCreateConnectionWithURL(tmpl, url, 0);
    if (conn < 0) { result = conn; goto cleanup; }

    req = sceHttpCreateRequestWithURL(conn, method, url, body_len);
    if (req < 0) { result = req; goto cleanup; }

    if (bearer && bearer[0]) {
        char authorization[256];
        snprintf(authorization, sizeof(authorization), "Bearer %s", bearer);
        result = sceHttpAddRequestHeader(
            req, "Authorization", authorization, SCE_HTTP_HEADER_OVERWRITE
        );
        if (result < 0) goto cleanup;
    }

    if (body) {
        result = sceHttpAddRequestHeader(
            req, "Content-Type", "application/json", SCE_HTTP_HEADER_OVERWRITE
        );
        if (result < 0) goto cleanup;
    }

    result = sceHttpSendRequest(req, body, body_len);
    if (result < 0) goto cleanup;

    if (status_code) {
        result = sceHttpGetStatusCode(req, status_code);
        if (result < 0) goto cleanup;
    }

    unsigned int used = 0;
    while (used + 1 < out_size) {
        int n = sceHttpReadData(req, out + used, out_size - used - 1);
        if (n < 0) { result = n; goto cleanup; }
        if (n == 0) break;
        used += (unsigned int)n;
    }
    out[used] = 0;
    result = (int)used;

cleanup:
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
    return result;
}

int dv_net_init(void) {
    if (g_ready) return 0;

    int net_module = 0;
    int http_module = 0;
    int https_module = 0;
    int net_initialized = 0;
    int netctl_initialized = 0;
    int http_initialized = 0;
    int r = 0;

    r = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if (r < 0) goto fail;
    net_module = 1;

    g_net_mem = malloc(1024 * 1024);
    if (!g_net_mem) {
        r = -2;
        goto fail;
    }

    SceNetInitParam param;
    memset(&param, 0, sizeof(param));
    param.memory = g_net_mem;
    param.size = 1024 * 1024;

    r = sceNetInit(&param);
    if (r < 0) goto fail;
    net_initialized = 1;

    r = sceNetCtlInit();
    if (r < 0) goto fail;
    netctl_initialized = 1;

    r = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    if (r < 0) goto fail;
    http_module = 1;

    r = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
    if (r < 0) goto fail;
    https_module = 1;

    r = sceHttpInit(1024 * 1024);
    if (r < 0) goto fail;
    http_initialized = 1;

    g_ready = 1;
    return 0;

fail:
    if (http_initialized) sceHttpTerm();
    if (https_module) sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
    if (http_module) sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    if (netctl_initialized) sceNetCtlTerm();
    if (net_initialized) sceNetTerm();

    free(g_net_mem);
    g_net_mem = NULL;

    if (net_module) sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    return r;
}

void dv_net_term(void) {
    if (!g_ready) return;

    sceHttpTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);

    free(g_net_mem);
    g_net_mem = NULL;
    g_ready = 0;
}

int dv_http_get(
    const char *url,
    const char *bearer,
    char *out,
    unsigned int out_size,
    int *status_code
) {
    return request(
        SCE_HTTP_METHOD_GET, url, bearer, NULL, out, out_size, status_code
    );
}

int dv_http_post_json(
    const char *url,
    const char *bearer,
    const char *json,
    char *out,
    unsigned int out_size,
    int *status_code
) {
    return request(
        SCE_HTTP_METHOD_POST, url, bearer, json ? json : "",
        out, out_size, status_code
    );
}


int dv_http_download(
    const char *url,
    const char *bearer,
    const char *destination,
    int *status_code
) {
    int tmpl = -1, conn = -1, req = -1, fd = -1;
    int result = -1;

    if (!g_ready || !url || !destination) return -1;
    if (status_code) *status_code = 0;

    tmpl = sceHttpCreateTemplate(
        "Discord Vita/1.7", SCE_HTTP_VERSION_1_1, 1
    );
    if (tmpl < 0) { result = tmpl; goto cleanup; }

    conn = sceHttpCreateConnectionWithURL(tmpl, url, 0);
    if (conn < 0) { result = conn; goto cleanup; }

    req = sceHttpCreateRequestWithURL(
        conn, SCE_HTTP_METHOD_GET, url, 0
    );
    if (req < 0) { result = req; goto cleanup; }

    if (bearer && bearer[0]) {
        char authorization[256];
        snprintf(authorization, sizeof(authorization), "Bearer %s", bearer);
        result = sceHttpAddRequestHeader(
            req, "Authorization", authorization, SCE_HTTP_HEADER_OVERWRITE
        );
        if (result < 0) goto cleanup;
    }

    result = sceHttpSendRequest(req, NULL, 0);
    if (result < 0) goto cleanup;

    int http_status = 0;
    result = sceHttpGetStatusCode(req, &http_status);
    if (result < 0) goto cleanup;
    if (status_code) *status_code = http_status;

    if (http_status != 200) {
        result = 0;
        goto cleanup;
    }

    fd = sceIoOpen(
        destination,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0600
    );
    if (fd < 0) { result = fd; goto cleanup; }

    unsigned char buffer[16 * 1024];
    int total = 0;

    while (1) {
        int n = sceHttpReadData(req, buffer, sizeof(buffer));
        if (n < 0) { result = n; goto cleanup; }
        if (n == 0) break;

        int written = sceIoWrite(fd, buffer, n);
        if (written != n) {
            result = written < 0 ? written : -2;
            goto cleanup;
        }

        total += n;

        /* Server icons should stay tiny. Stop accidental huge downloads. */
        if (total > 512 * 1024) {
            result = -3;
            goto cleanup;
        }
    }

    result = total;

cleanup:
    if (fd >= 0) sceIoClose(fd);

    if (result < 0 && destination) {
        sceIoRemove(destination);
    }

    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
    return result;
}
