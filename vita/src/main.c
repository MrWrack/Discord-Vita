#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "net.h"
#include "session.h"
#include "api.h"
#include "input.h"
#include "ime.h"

typedef enum {
    APP_LOGIN,
    APP_DEVICE_LOGIN,
    APP_HOME,
    APP_SERVER_DETAILS,
    APP_CHANNELS,
    APP_CHAT,
    APP_ACCOUNT
} AppState;

static void load_selected_guild_icon(
    const char *session,
    const DvGuildList *guilds,
    int selected
) {
    dv_ui_clear_selected_guild_icon();

    if (!guilds || selected < 0 || selected >= guilds->count)
        return;

    const DvGuild *guild = &guilds->items[selected];
    if (!guild->icon[0])
        return;

    char path[256];
    if (dv_api_prepare_guild_icon(
            session, guild, path, sizeof(path)) == 0) {
        dv_ui_set_selected_guild_icon(guild->id, path);
    }
}

static int load_home_data(
    const char *session,
    DvGuildList *guilds,
    DvCapabilities *capabilities,
    char *status,
    unsigned int status_size
) {
    int cap_result = dv_api_load_capabilities(session, capabilities);
    int guild_result = dv_api_load_guilds(session, guilds);

    if (guild_result != 0) {
        snprintf(status, status_size,
                 "Kunde inte ladda servrar (%d)", guild_result);
        return guild_result;
    }

    if (cap_result == 0) {
        snprintf(status, status_size,
                 "%d servrar laddade", guilds->count);
    } else {
        snprintf(status, status_size,
                 "%d servrar laddade · statusfel %d",
                 guilds->count, cap_result);
    }

    return 0;
}

static void logout_local(
    char *session,
    char *username,
    DvGuildList *guilds,
    DvChannelList *channels,
    DvMessageList *messages,
    DvCapabilities *capabilities
) {
    dv_api_logout(session);
    dv_session_clear();

    session[0] = 0;
    username[0] = 0;

    dv_ui_clear_selected_guild_icon();
    memset(guilds, 0, sizeof(*guilds));
    memset(channels, 0, sizeof(*channels));
    memset(messages, 0, sizeof(*messages));
    memset(capabilities, 0, sizeof(*capabilities));
}

int main(void) {
    dv_ui_init();

    int input_result = dv_input_init();
    int ime_result = dv_ime_init();
    int net_result = dv_net_init();

    char session[256] = {0};
    char username[64] = {0};
    char status[160] = {0};

    DvDeviceLogin login;
    DvGuildList guilds;
    DvChannelList channels;
    DvMessageList messages;
    DvCapabilities capabilities;

    memset(&login, 0, sizeof(login));
    memset(&guilds, 0, sizeof(guilds));
    memset(&channels, 0, sizeof(channels));
    memset(&messages, 0, sizeof(messages));
    memset(&capabilities, 0, sizeof(capabilities));

    AppState state = APP_LOGIN;
    AppState account_return_state = APP_HOME;

    int selected_guild = 0;
    int selected_channel = 0;
    SceUInt64 next_poll_us = 0;

    if (net_result < 0) {
        snprintf(status, sizeof(status),
                 "Nätverksfel: 0x%08X", (unsigned int)net_result);
    } else if (input_result < 0) {
        snprintf(status, sizeof(status),
                 "Touch kunde inte starta: 0x%08X",
                 (unsigned int)input_result);
    }

    if (net_result >= 0 &&
        dv_session_load(session, sizeof(session)) == 0) {
        int r = dv_api_validate_session(
            session, username, sizeof(username));

        if (r == 0) {
            state = APP_HOME;
            load_home_data(
                session, &guilds, &capabilities,
                status, sizeof(status));
            load_selected_guild_icon(
                session, &guilds, selected_guild);
        } else {
            dv_session_clear();
            session[0] = 0;
            snprintf(status, sizeof(status),
                     "Sessionen har gått ut. Logga in igen.");
        }
    }

    while (1) {
        if (state == APP_LOGIN) {
            dv_ui_draw_login(
                status[0] ? status : "Tryck X för att börja.");
        } else if (state == APP_DEVICE_LOGIN) {
            dv_ui_draw_device_login(
                login.verification_uri,
                login.user_code,
                status[0] ? status : "Väntar på godkännande...");
        } else if (state == APP_HOME) {
            dv_ui_draw_home(
                &guilds, selected_guild, username,
                &capabilities, status);
        } else if (state == APP_SERVER_DETAILS) {
            const DvGuild *guild =
                (selected_guild >= 0 &&
                 selected_guild < guilds.count)
                    ? &guilds.items[selected_guild] : NULL;
            dv_ui_draw_server_details(
                guild, username, &capabilities);
        } else if (state == APP_CHANNELS) {
            const DvGuild *guild =
                (selected_guild >= 0 &&
                 selected_guild < guilds.count)
                    ? &guilds.items[selected_guild] : NULL;
            dv_ui_draw_channels(
                guild, &channels, selected_channel,
                username, status);
        } else if (state == APP_CHAT) {
            const DvGuild *guild =
                (selected_guild >= 0 &&
                 selected_guild < guilds.count)
                    ? &guilds.items[selected_guild] : NULL;
            const DvChannel *channel =
                (selected_channel >= 0 &&
                 selected_channel < channels.count)
                    ? &channels.items[selected_channel] : NULL;
            dv_ui_draw_chat(
                guild, channel, &messages,
                username, status);
        } else if (state == APP_ACCOUNT) {
            dv_ui_draw_account_menu(username, 1);
        }

        dv_input_update();
        const DvTouch *touch = dv_input_touch();

        if (state == APP_LOGIN) {
            int login_tap =
                touch->pressed &&
                touch->x >= 365 && touch->x <= 785 &&
                touch->y >= 219 && touch->y <= 281;

            if (dv_input_button_pressed(SCE_CTRL_CROSS) ||
                login_tap) {
                if (net_result < 0) {
                    snprintf(status, sizeof(status),
                             "Ingen nätverksanslutning.");
                } else {
                    int r = dv_api_device_start(&login);
                    if (r == 0) {
                        state = APP_DEVICE_LOGIN;
                        next_poll_us = sceKernelGetProcessTimeWide() +
                            ((SceUInt64)login.interval_seconds * 1000000ULL);
                        snprintf(status, sizeof(status),
                                 "Väntar på godkännande...");
                    } else {
                        snprintf(status, sizeof(status),
                                 "Login kunde inte starta (%d)", r);
                    }
                }
            }

            if (dv_input_button_pressed(SCE_CTRL_CIRCLE))
                break;

        } else if (state == APP_DEVICE_LOGIN) {
            int touch_poll =
                touch->pressed &&
                touch->x >= 518 && touch->x <= 898 &&
                touch->y >= 308 && touch->y <= 412;

            if (dv_input_button_pressed(SCE_CTRL_CIRCLE)) {
                state = APP_LOGIN;
                memset(&login, 0, sizeof(login));
                snprintf(status, sizeof(status), "Login avbruten.");
            } else {
                SceUInt64 now_us = sceKernelGetProcessTimeWide();

                if (now_us >= next_poll_us ||
                    dv_input_button_pressed(SCE_CTRL_CROSS) ||
                    touch_poll) {
                    int r = dv_api_device_poll(
                        &login,
                        session, sizeof(session),
                        username, sizeof(username));

                    if (r == 0) {
                        if (dv_session_save(session) == 0) {
                            selected_guild = 0;
                            state = APP_HOME;
                            load_home_data(
                                session, &guilds, &capabilities,
                                status, sizeof(status));
                            load_selected_guild_icon(
                                session, &guilds, selected_guild);
                        } else {
                            snprintf(status, sizeof(status),
                                     "Sessionen kunde inte sparas.");
                        }
                    } else if (r == 1) {
                        snprintf(status, sizeof(status),
                                 "Väntar på godkännande...");
                        next_poll_us = sceKernelGetProcessTimeWide() +
                            ((SceUInt64)login.interval_seconds * 1000000ULL);
                    } else if (r == -410) {
                        state = APP_LOGIN;
                        snprintf(status, sizeof(status),
                                 "Koden gick ut. Försök igen.");
                    } else {
                        snprintf(status, sizeof(status),
                                 "Login-fel (%d)", r);
                        next_poll_us = sceKernelGetProcessTimeWide() +
                            ((SceUInt64)login.interval_seconds * 1000000ULL);
                    }
                }
            }

        } else if (state == APP_HOME) {
            if (touch->pressed &&
                touch->x >= 72 && touch->x < 400 &&
                touch->y >= 70 && touch->y < 457) {
                int first = selected_guild - 4;
                if (first < 0) first = 0;
                if (first > guilds.count - 9)
                    first = guilds.count - 9;
                if (first < 0) first = 0;

                int row = (touch->y - 70) / 43;
                int tapped = first + row;

                if (tapped >= 0 && tapped < guilds.count) {
                    selected_guild = tapped;
                    status[0] = 0;
                    load_selected_guild_icon(
                        session, &guilds, selected_guild);
                }
            }

            if (dv_input_button_repeat(SCE_CTRL_UP) &&
                selected_guild > 0) {
                selected_guild--;
                status[0] = 0;
                load_selected_guild_icon(
                    session, &guilds, selected_guild);
            }

            if (dv_input_button_repeat(SCE_CTRL_DOWN) &&
                selected_guild + 1 < guilds.count) {
                selected_guild++;
                status[0] = 0;
                load_selected_guild_icon(
                    session, &guilds, selected_guild);
            }

            if (dv_input_button_pressed(SCE_CTRL_CROSS) &&
                guilds.count > 0) {
                if (!capabilities.channels) {
                    snprintf(status, sizeof(status),
                             "Backend-boten är inte konfigurerad.");
                } else {
                    memset(&channels, 0, sizeof(channels));
                    selected_channel = 0;

                    int r = dv_api_load_channels(
                        session,
                        guilds.items[selected_guild].id,
                        &channels);

                    if (r == 0) {
                        state = APP_CHANNELS;
                        snprintf(status, sizeof(status),
                                 "%d kanaler laddade", channels.count);
                    } else {
                        snprintf(status, sizeof(status),
                                 "Kanalfel %d", r);
                    }
                }
            }

            if (dv_input_button_pressed(SCE_CTRL_TRIANGLE) &&
                guilds.count > 0) {
                state = APP_SERVER_DETAILS;
            }

            if (dv_input_button_pressed(SCE_CTRL_START)) {
                account_return_state = APP_HOME;
                state = APP_ACCOUNT;
            }

            if (dv_input_button_pressed(SCE_CTRL_CIRCLE))
                break;

        } else if (state == APP_CHANNELS) {
            if (dv_input_button_repeat(SCE_CTRL_UP) &&
                selected_channel > 0) {
                selected_channel--;
                status[0] = 0;
            }

            if (dv_input_button_repeat(SCE_CTRL_DOWN) &&
                selected_channel + 1 < channels.count) {
                selected_channel++;
                status[0] = 0;
            }

            if (touch->pressed &&
                touch->x >= 72 && touch->x < 402 &&
                touch->y >= 72 && touch->y < 459) {
                int first = selected_channel - 4;
                if (first < 0) first = 0;
                if (first > channels.count - 9)
                    first = channels.count - 9;
                if (first < 0) first = 0;

                int row = (touch->y - 72) / 43;
                int tapped = first + row;
                if (tapped >= 0 && tapped < channels.count)
                    selected_channel = tapped;
            }

            if (dv_input_button_pressed(SCE_CTRL_CROSS) &&
                selected_channel >= 0 &&
                selected_channel < channels.count) {
                memset(&messages, 0, sizeof(messages));

                int r = dv_api_load_messages(
                    session,
                    channels.items[selected_channel].id,
                    &messages);

                if (r == 0) {
                    state = APP_CHAT;
                    snprintf(status, sizeof(status),
                             "%d meddelanden", messages.count);
                } else {
                    snprintf(status, sizeof(status),
                             "Meddelandefel %d", r);
                }
            }

            if (dv_input_button_pressed(SCE_CTRL_CIRCLE)) {
                state = APP_HOME;
                status[0] = 0;
            }

        } else if (state == APP_CHAT) {
            if (dv_input_button_pressed(SCE_CTRL_CIRCLE)) {
                state = APP_CHANNELS;
                status[0] = 0;
            }

            if (dv_input_button_pressed(SCE_CTRL_TRIANGLE) &&
                selected_channel >= 0 &&
                selected_channel < channels.count) {
                int r = dv_api_load_messages(
                    session,
                    channels.items[selected_channel].id,
                    &messages);

                if (r == 0)
                    snprintf(status, sizeof(status), "Chat uppdaterad");
                else
                    snprintf(status, sizeof(status),
                             "Uppdateringsfel %d", r);
            }

            if (dv_input_button_pressed(SCE_CTRL_SQUARE) &&
                selected_channel >= 0 &&
                selected_channel < channels.count) {
                if (ime_result < 0) {
                    snprintf(status, sizeof(status),
                             "Tangentbordet kunde inte startas.");
                } else {
                    char message[2048] = {0};
                    int ir = dv_ime_prompt_message(
                        message, sizeof(message));

                    if (ir > 0) {
                        int sr = dv_api_send_message(
                            session,
                            channels.items[selected_channel].id,
                            message);

                        if (sr == 0) {
                            dv_api_load_messages(
                                session,
                                channels.items[selected_channel].id,
                                &messages);
                            snprintf(status, sizeof(status),
                                     "Meddelande skickat");
                        } else {
                            snprintf(status, sizeof(status),
                                     "Skickfel %d", sr);
                        }
                    } else if (ir < 0) {
                        snprintf(status, sizeof(status),
                                 "Tangentbordsfel %d", ir);
                    }
                }
            }

        } else if (state == APP_SERVER_DETAILS) {
            if (dv_input_button_pressed(SCE_CTRL_CIRCLE)) {
                state = APP_HOME;
                status[0] = 0;
            }

            if (dv_input_button_pressed(SCE_CTRL_START)) {
                account_return_state = APP_SERVER_DETAILS;
                state = APP_ACCOUNT;
            }

        } else if (state == APP_ACCOUNT) {
            int logout_tap =
                touch->pressed &&
                touch->x >= 310 && touch->x <= 650 &&
                touch->y >= 292 && touch->y <= 344;

            if (dv_input_button_pressed(SCE_CTRL_CIRCLE)) {
                state = account_return_state;
            }

            if (dv_input_button_pressed(SCE_CTRL_CROSS) ||
                logout_tap) {
                logout_local(
                    session, username,
                    &guilds, &channels, &messages,
                    &capabilities);

                selected_guild = 0;
                selected_channel = 0;
                state = APP_LOGIN;
                snprintf(status, sizeof(status), "Utloggad.");
            }
        }

        sceKernelDelayThread(16667);
    }

    dv_net_term();
    dv_input_term();
    dv_ime_term();
    dv_ui_term();
    sceKernelExitProcess(0);
    return 0;
}
