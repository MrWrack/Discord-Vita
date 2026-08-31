#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include "api.h"
#include "net.h"
#include "config.h"

#define RESPONSE_MAX 32768

static int copy_json_string(const char *p, char *out, unsigned int out_size) {
    unsigned int used = 0;
    if (!p || *p != '"' || !out || out_size == 0) return -1;
    p++;

    while (*p && *p != '"') {
        char c = *p++;

        if (c == '\\') {
            char esc = *p++;
            if (!esc) return -1;
            switch (esc) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default:
                    /* Keep unsupported unicode escapes readable instead of overflowing. */
                    c = '?';
                    if (esc == 'u') {
                        for (int i = 0; i < 4 && *p; ++i) p++;
                    }
                    break;
            }
        }

        if (used + 1 < out_size) out[used++] = c;
    }

    out[used] = 0;
    return *p == '"' ? 0 : -1;
}

static const char *find_key(const char *json, const char *key) {
    static char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *p = json;
    while ((p = strstr(p, needle)) != NULL) {
        p += strlen(needle);
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != ':') continue;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        return p;
    }
    return NULL;
}

static int json_string(const char *json, const char *key, char *out, unsigned int size) {
    const char *p = find_key(json, key);
    if (!p || *p != '"') return -1;
    return copy_json_string(p, out, size);
}

static int json_int(const char *json, const char *key, int fallback) {
    const char *p = find_key(json, key);
    if (!p) return fallback;
    return atoi(p);
}

static int json_true(const char *json, const char *key) {
    const char *p = find_key(json, key);
    return p && strncmp(p, "true", 4) == 0;
}

static int json_bool(const char *json, const char *key, int fallback) {
    const char *p = find_key(json, key);
    if (!p) return fallback;
    if (strncmp(p, "true", 4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    return fallback;
}

static int backend_url(char *out, unsigned int size, const char *path) {
    int n = snprintf(out, size, "%s%s", DISCORD_VITA_BACKEND, path);
    return n > 0 && (unsigned int)n < size ? 0 : -1;
}

int dv_api_device_start(DvDeviceLogin *login) {
    if (!login) return -1;
    memset(login, 0, sizeof(*login));

    char url[384];
    char response[RESPONSE_MAX];
    int status = 0;

    if (backend_url(url, sizeof(url), "/oauth/device/start") < 0) return -2;
    if (dv_http_get(url, NULL, response, sizeof(response), &status) < 0) return -3;
    if (status != 200) return -1000 - status;

    if (json_string(response, "transaction_id", login->transaction_id, sizeof(login->transaction_id)) < 0)
        return -4;
    if (json_string(response, "poll_token", login->poll_token, sizeof(login->poll_token)) < 0)
        return -5;
    if (json_string(response, "user_code", login->user_code, sizeof(login->user_code)) < 0)
        return -6;
    if (json_string(response, "verification_uri", login->verification_uri, sizeof(login->verification_uri)) < 0)
        return -7;

    login->interval_seconds = json_int(response, "interval", 5);
    login->expires_in_seconds = json_int(response, "expires_in", 300);
    if (login->interval_seconds < 2) login->interval_seconds = 2;
    return 0;
}

int dv_api_device_poll(
    const DvDeviceLogin *login,
    char *session,
    unsigned int session_size,
    char *username,
    unsigned int username_size
) {
    if (!login || !session || session_size < 2) return -1;

    char url[512];
    char response[RESPONSE_MAX];
    int status = 0;

    int n = snprintf(
        url, sizeof(url), "%s/oauth/device/status?id=%s",
        DISCORD_VITA_BACKEND, login->transaction_id
    );
    if (n <= 0 || (unsigned int)n >= sizeof(url)) return -2;

    if (dv_http_get(
        url, login->poll_token, response, sizeof(response), &status
    ) < 0) return -3;

    if (status == 202) return 1;
    if (status == 410) return -410;
    if (status != 200) return -1000 - status;
    if (!json_true(response, "authenticated")) return 1;

    if (json_string(response, "session", session, session_size) < 0) return -4;

    if (username && username_size > 0) {
        if (json_string(response, "username", username, username_size) < 0)
            snprintf(username, username_size, "Discord user");
    }

    return 0;
}

int dv_api_validate_session(
    const char *session,
    char *username,
    unsigned int username_size
) {
    if (!session || !session[0]) return -1;

    char url[384];
    char response[RESPONSE_MAX];
    int status = 0;

    if (backend_url(url, sizeof(url), "/session") < 0) return -2;
    if (dv_http_get(url, session, response, sizeof(response), &status) < 0) return -3;
    if (status == 401) return 1;
    if (status != 200) return -1000 - status;

    if (username && username_size > 0) {
        if (json_string(response, "username", username, username_size) < 0)
            snprintf(username, username_size, "Discord user");
    }
    return 0;
}

int dv_api_logout(const char *session) {
    if (!session || !session[0]) return -1;

    char url[384];
    char response[1024];
    int status = 0;

    if (backend_url(url, sizeof(url), "/oauth/logout") < 0) return -2;
    if (dv_http_post_json(url, session, "{}", response, sizeof(response), &status) < 0)
        return -3;

    return status == 200 || status == 401 ? 0 : -1000 - status;
}

static const char *next_object(const char *p, const char **end) {
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *start = NULL;

    for (; *p; ++p) {
        char c = *p;

        if (in_string) {
            if (escaped) escaped = 0;
            else if (c == '\\') escaped = 1;
            else if (c == '"') in_string = 0;
            continue;
        }

        if (c == '"') {
            in_string = 1;
            continue;
        }

        if (c == '{') {
            if (depth == 0) start = p;
            depth++;
        } else if (c == '}' && depth > 0) {
            depth--;
            if (depth == 0 && start) {
                *end = p + 1;
                return start;
            }
        }
    }

    return NULL;
}

int dv_api_load_guilds(const char *session, DvGuildList *guilds) {
    if (!session || !guilds) return -1;
    memset(guilds, 0, sizeof(*guilds));

    char url[384];
    static char response[RESPONSE_MAX];
    int status = 0;

    if (backend_url(url, sizeof(url), "/guilds") < 0) return -2;
    if (dv_http_get(url, session, response, sizeof(response), &status) < 0) return -3;
    if (status == 401) return 1;
    if (status != 200) return -1000 - status;

    const char *p = response;
    while (*p && guilds->count < DV_MAX_GUILDS) {
        const char *end = NULL;
        const char *start = next_object(p, &end);
        if (!start || !end) break;

        size_t len = (size_t)(end - start);
        if (len < 2047) {
            char object[2048];
            memcpy(object, start, len);
            object[len] = 0;

            DvGuild *g = &guilds->items[guilds->count];
            if (
                json_string(object, "id", g->id, sizeof(g->id)) == 0 &&
                json_string(object, "name", g->name, sizeof(g->name)) == 0
            ) {
                json_string(object, "icon", g->icon, sizeof(g->icon));
                g->owner = json_bool(object, "owner", 0);
                guilds->count++;
            }
        }

        p = end;
    }

    return 0;
}


int dv_api_load_capabilities(const char *session, DvCapabilities *capabilities) {
    if (!capabilities) return -1;
    memset(capabilities, 0, sizeof(*capabilities));

    char url[384];
    char response[4096];
    int status = 0;

    if (backend_url(url, sizeof(url), "/capabilities") < 0) return -2;

    /*
     * /capabilities is public on the backend today. Supplying the session
     * anyway keeps the Vita call uniform if the backend later protects it.
     */
    if (dv_http_get(url, session, response, sizeof(response), &status) < 0)
        return -3;

    if (status != 200) return -1000 - status;

    capabilities->oauth_login = json_bool(response, "oauth_login", 0);
    capabilities->guilds = json_bool(response, "guilds", 0);
    capabilities->channels = json_bool(response, "channels", 0);
    capabilities->messages = json_bool(response, "messages", 0);
    capabilities->send_messages = json_bool(response, "send_messages", 0);

    return 0;
}


int dv_api_prepare_guild_icon(
    const char *session,
    const DvGuild *guild,
    char *path,
    unsigned int path_size
) {
    if (!session || !session[0] || !guild || !path || path_size < 2)
        return -1;

    path[0] = 0;

    if (!guild->id[0] || !guild->icon[0])
        return 1;

    sceIoMkdir("ux0:data/DiscordVita", 0777);
    sceIoMkdir("ux0:data/DiscordVita/cache", 0777);

    int n = snprintf(
        path,
        path_size,
        "ux0:data/DiscordVita/cache/guild_%s_%s.png",
        guild->id,
        guild->icon
    );
    if (n <= 0 || (unsigned int)n >= path_size)
        return -2;

    /*
     * Hash is part of the filename. If Discord changes the icon hash,
     * a new cache file is automatically used.
     */
    SceUID existing = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (existing >= 0) {
        sceIoClose(existing);
        return 0;
    }

    char url[640];
    n = snprintf(
        url,
        sizeof(url),
        "%s/guilds/%s/icon?hash=%s",
        DISCORD_VITA_BACKEND,
        guild->id,
        guild->icon
    );
    if (n <= 0 || (unsigned int)n >= sizeof(url))
        return -3;

    int status = 0;
    int result = dv_http_download(
        url,
        session,
        path,
        &status
    );

    if (result < 0) return -4;
    if (status == 404) {
        sceIoRemove(path);
        return 1;
    }
    if (status != 200) {
        sceIoRemove(path);
        return -1000 - status;
    }

    return result > 0 ? 0 : -5;
}


static int json_escape_string(
    const char *input,
    char *out,
    unsigned int out_size
) {
    unsigned int used = 0;
    if (!input || !out || out_size < 3) return -1;

    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        const char *escape = NULL;
        char temp[7];

        switch (*p) {
            case '"': escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            default:
                if (*p < 0x20) {
                    snprintf(temp, sizeof(temp), "\\u%04x", *p);
                    escape = temp;
                }
                break;
        }

        if (escape) {
            unsigned int n = (unsigned int)strlen(escape);
            if (used + n + 1 >= out_size) return -2;
            memcpy(out + used, escape, n);
            used += n;
        } else {
            if (used + 2 >= out_size) return -2;
            out[used++] = (char)*p;
        }
    }

    out[used] = 0;
    return 0;
}

int dv_api_load_channels(
    const char *session,
    const char *guild_id,
    DvChannelList *channels
) {
    if (!session || !session[0] || !guild_id || !guild_id[0] || !channels)
        return -1;

    memset(channels, 0, sizeof(*channels));

    char url[512];
    static char response[RESPONSE_MAX];
    int status = 0;

    int n = snprintf(
        url, sizeof(url),
        "%s/guilds/%s/channels",
        DISCORD_VITA_BACKEND,
        guild_id
    );
    if (n <= 0 || (unsigned int)n >= sizeof(url)) return -2;

    if (dv_http_get(url, session, response, sizeof(response), &status) < 0)
        return -3;
    if (status == 401) return 1;
    if (status != 200) return -1000 - status;

    const char *p = response;
    while (*p && channels->count < DV_MAX_CHANNELS) {
        const char *end = NULL;
        const char *start = next_object(p, &end);
        if (!start || !end) break;

        size_t len = (size_t)(end - start);
        if (len < 2047) {
            char object[2048];
            memcpy(object, start, len);
            object[len] = 0;

            DvChannel *channel = &channels->items[channels->count];
            if (
                json_string(object, "id", channel->id, sizeof(channel->id)) == 0 &&
                json_string(object, "name", channel->name, sizeof(channel->name)) == 0
            ) {
                channel->type = json_int(object, "type", 0);
                channels->count++;
            }
        }

        p = end;
    }

    return 0;
}

int dv_api_load_messages(
    const char *session,
    const char *channel_id,
    DvMessageList *messages
) {
    if (!session || !session[0] || !channel_id || !channel_id[0] || !messages)
        return -1;

    memset(messages, 0, sizeof(*messages));

    char url[512];
    static char response[RESPONSE_MAX];
    int status = 0;

    int n = snprintf(
        url, sizeof(url),
        "%s/channels/%s/messages",
        DISCORD_VITA_BACKEND,
        channel_id
    );
    if (n <= 0 || (unsigned int)n >= sizeof(url)) return -2;

    if (dv_http_get(url, session, response, sizeof(response), &status) < 0)
        return -3;
    if (status == 401) return 1;
    if (status != 200) return -1000 - status;

    const char *p = response;
    while (*p && messages->count < DV_MAX_MESSAGES) {
        const char *end = NULL;
        const char *start = next_object(p, &end);
        if (!start || !end) break;

        size_t len = (size_t)(end - start);
        if (len < 4095) {
            char object[4096];
            memcpy(object, start, len);
            object[len] = 0;

            DvMessage *message = &messages->items[messages->count];
            if (
                json_string(object, "id", message->id, sizeof(message->id)) == 0 &&
                json_string(object, "content", message->content, sizeof(message->content)) == 0
            ) {
                /*
                 * Backend's compact response contains author.username.
                 * find_key() walks nested JSON, so this resolves the author.
                 */
                if (json_string(
                    object, "username",
                    message->author, sizeof(message->author)
                ) < 0) {
                    snprintf(message->author, sizeof(message->author), "Discord user");
                }

                message->author_is_bot = json_bool(object, "bot", 0);
                messages->count++;
            }
        }

        p = end;
    }

    /*
     * Discord returns newest first. Vita chat renders oldest -> newest.
     */
    for (int i = 0; i < messages->count / 2; ++i) {
        DvMessage temp = messages->items[i];
        messages->items[i] = messages->items[messages->count - 1 - i];
        messages->items[messages->count - 1 - i] = temp;
    }

    return 0;
}

int dv_api_send_message(
    const char *session,
    const char *channel_id,
    const char *content
) {
    if (!session || !session[0] || !channel_id || !channel_id[0] ||
        !content || !content[0])
        return -1;

    char escaped[4096];
    if (json_escape_string(content, escaped, sizeof(escaped)) < 0)
        return -2;

    char body[4352];
    int body_len = snprintf(
        body, sizeof(body),
        "{\"content\":\"%s\"}",
        escaped
    );
    if (body_len <= 0 || (unsigned int)body_len >= sizeof(body))
        return -3;

    char url[512];
    int n = snprintf(
        url, sizeof(url),
        "%s/channels/%s/messages",
        DISCORD_VITA_BACKEND,
        channel_id
    );
    if (n <= 0 || (unsigned int)n >= sizeof(url))
        return -4;

    char response[4096];
    int status = 0;

    if (dv_http_post_json(
        url, session, body,
        response, sizeof(response), &status
    ) < 0) {
        return -5;
    }

    if (status == 401) return 1;
    if (status != 200 && status != 201)
        return -1000 - status;

    return 0;
}
