#pragma once
#include "config.h"

typedef struct {
    char transaction_id[96];
    char poll_token[96];
    char user_code[32];
    char verification_uri[160];
    int interval_seconds;
    int expires_in_seconds;
} DvDeviceLogin;

typedef struct {
    char id[32];
    char name[DV_GUILD_NAME_MAX];
    char icon[DV_GUILD_ICON_MAX];
    int owner;
} DvGuild;

typedef struct {
    DvGuild items[DV_MAX_GUILDS];
    int count;
} DvGuildList;


typedef struct {
    char id[32];
    char name[DV_CHANNEL_NAME_MAX];
    int type;
} DvChannel;

typedef struct {
    DvChannel items[DV_MAX_CHANNELS];
    int count;
} DvChannelList;

typedef struct {
    char id[32];
    char author[DV_MESSAGE_AUTHOR_MAX];
    char content[DV_MESSAGE_CONTENT_MAX];
    int author_is_bot;
} DvMessage;

typedef struct {
    DvMessage items[DV_MAX_MESSAGES];
    int count;
} DvMessageList;

typedef struct {
    int oauth_login;
    int guilds;
    int channels;
    int messages;
    int send_messages;
} DvCapabilities;

int dv_api_device_start(DvDeviceLogin *login);

int dv_api_device_poll(
    const DvDeviceLogin *login,
    char *session,
    unsigned int session_size,
    char *username,
    unsigned int username_size
);

int dv_api_validate_session(
    const char *session,
    char *username,
    unsigned int username_size
);

int dv_api_logout(const char *session);
int dv_api_load_guilds(const char *session, DvGuildList *guilds);
int dv_api_load_capabilities(const char *session, DvCapabilities *capabilities);

int dv_api_prepare_guild_icon(
    const char *session,
    const DvGuild *guild,
    char *path,
    unsigned int path_size
);

int dv_api_load_channels(
    const char *session,
    const char *guild_id,
    DvChannelList *channels
);

int dv_api_load_messages(
    const char *session,
    const char *channel_id,
    DvMessageList *messages
);

int dv_api_send_message(
    const char *session,
    const char *channel_id,
    const char *content
);
