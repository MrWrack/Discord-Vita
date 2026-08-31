#pragma once
#include "api.h"

void dv_ui_init(void);
void dv_ui_term(void);

void dv_ui_draw_login(const char *status);

void dv_ui_draw_device_login(
    const char *verification_uri,
    const char *user_code,
    const char *status
);

void dv_ui_draw_home(
    const DvGuildList *guilds,
    int selected,
    const char *username,
    const DvCapabilities *capabilities,
    const char *status
);

void dv_ui_draw_server_details(
    const DvGuild *guild,
    const char *username,
    const DvCapabilities *capabilities
);

void dv_ui_draw_account_menu(
    const char *username,
    int logout_selected
);

int dv_ui_set_selected_guild_icon(const char *guild_id, const char *path);
void dv_ui_clear_selected_guild_icon(void);

void dv_ui_draw_channels(
    const DvGuild *guild,
    const DvChannelList *channels,
    int selected,
    const char *username,
    const char *status
);

void dv_ui_draw_chat(
    const DvGuild *guild,
    const DvChannel *channel,
    const DvMessageList *messages,
    const char *username,
    const char *status
);
