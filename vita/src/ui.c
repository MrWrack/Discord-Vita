#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

static vita2d_pgf *font;
static vita2d_texture *selected_guild_icon = NULL;
static char selected_guild_icon_id[32] = {0};

#define BG         RGBA8(24,25,28,255)
#define RAIL       RGBA8(18,19,21,255)
#define PANEL      RGBA8(38,40,44,255)
#define PANEL_2    RGBA8(45,47,52,255)
#define CHAT       RGBA8(49,51,56,255)
#define ACCENT     RGBA8(88,101,242,255)
#define ACCENT_2   RGBA8(71,82,196,255)
#define TEXT       RGBA8(242,243,245,255)
#define MUTED      RGBA8(181,186,193,255)
#define DIM        RGBA8(120,124,132,255)
#define SELECT     RGBA8(62,65,71,255)
#define GREEN      RGBA8(35,165,89,255)
#define RED        RGBA8(218,55,60,255)
#define LINE       RGBA8(64,67,73,255)

static void text(int x, int y, float scale, unsigned int color, const char *value) {
    if (font && value) vita2d_pgf_draw_text(font, x, y, color, scale, value);
}

static void begin(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_draw_rectangle(0, 0, 960, 544, BG);
}

static void end(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void divider(int x, int y, int width) {
    vita2d_draw_rectangle(x, y, width, 1, LINE);
}

static char upper_ascii(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

static void guild_initials(const char *name, char out[3]) {
    out[0] = '?';
    out[1] = 0;
    out[2] = 0;

    if (!name || !name[0]) return;

    out[0] = upper_ascii(name[0]);

    const char *space = strchr(name, ' ');
    if (space && space[1]) {
        out[1] = upper_ascii(space[1]);
        out[2] = 0;
    }
}

static void pill(int x, int y, int w, const char *label, int enabled) {
    unsigned int fill = enabled ? ACCENT_2 : PANEL_2;
    unsigned int color = enabled ? TEXT : DIM;
    vita2d_draw_rectangle(x, y, w, 30, fill);
    text(x + 11, y + 21, 0.50f, color, label);
}

static void top_bar(const char *title, const char *subtitle) {
    vita2d_draw_rectangle(0, 0, 960, 58, PANEL);
    text(25, 37, 0.88f, TEXT, title);
    if (subtitle && subtitle[0]) text(192, 36, 0.52f, MUTED, subtitle);
    divider(0, 57, 960);
}

void dv_ui_init(void) {
    vita2d_init();
    vita2d_set_clear_color(BG);
    font = vita2d_load_default_pgf();
}

void dv_ui_term(void) {
    if (selected_guild_icon) {
        vita2d_free_texture(selected_guild_icon);
        selected_guild_icon = NULL;
    }
    if (font) vita2d_free_pgf(font);
    vita2d_fini();
}

void dv_ui_clear_selected_guild_icon(void) {
    if (selected_guild_icon) {
        vita2d_free_texture(selected_guild_icon);
        selected_guild_icon = NULL;
    }
    selected_guild_icon_id[0] = 0;
}

int dv_ui_set_selected_guild_icon(const char *guild_id, const char *path) {
    if (!guild_id || !guild_id[0] || !path || !path[0])
        return -1;

    if (
        selected_guild_icon &&
        strcmp(selected_guild_icon_id, guild_id) == 0
    ) {
        return 0;
    }

    dv_ui_clear_selected_guild_icon();

    selected_guild_icon = vita2d_load_PNG_file(path);
    if (!selected_guild_icon)
        return -2;

    snprintf(
        selected_guild_icon_id,
        sizeof(selected_guild_icon_id),
        "%s",
        guild_id
    );
    return 0;
}

static void draw_selected_icon(float x, float y, float size) {
    if (!selected_guild_icon) return;

    unsigned int width = vita2d_texture_get_width(selected_guild_icon);
    unsigned int height = vita2d_texture_get_height(selected_guild_icon);

    if (!width || !height) return;

    float scale_x = size / (float)width;
    float scale_y = size / (float)height;

    vita2d_draw_texture_scale(
        selected_guild_icon,
        x,
        y,
        scale_x,
        scale_y
    );
}

void dv_ui_draw_login(const char *status) {
    begin();

    vita2d_draw_rectangle(0, 0, 960, 544, BG);

    /* subtle left brand band */
    vita2d_draw_rectangle(0, 0, 290, 544, RAIL);
    vita2d_draw_fill_circle(145, 157, 54, ACCENT);
    text(105, 176, 1.55f, TEXT, "DV");
    text(73, 252, 1.08f, TEXT, "Discord Vita");
    text(66, 284, 0.58f, MUTED, "Byggd för PS Vita");

    text(365, 133, 1.02f, TEXT, "Välkommen tillbaka");
    text(365, 168, 0.60f, MUTED, "Logga in säkert via Discord på en annan enhet.");

    vita2d_draw_rectangle(365, 219, 420, 62, ACCENT);
    text(414, 258, 0.83f, TEXT, "X  Logga in med Discord");

    vita2d_draw_rectangle(365, 299, 420, 48, PANEL);
    text(413, 330, 0.57f, MUTED, "Din Discord-token stannar på backend");

    text(365, 395, 0.52f, DIM, "O  Avsluta");
    if (status && status[0]) text(365, 447, 0.54f, MUTED, status);

    end();
}

void dv_ui_draw_device_login(
    const char *verification_uri,
    const char *user_code,
    const char *status
) {
    begin();
    top_bar("Discord Vita", "Säker inloggning");

    text(62, 112, 0.95f, TEXT, "1. Öppna Discord-aktivering");
    text(62, 148, 0.58f, MUTED, "Använd mobil, dator eller annan webbläsare.");

    vita2d_draw_rectangle(62, 174, 836, 58, PANEL);
    text(82, 211, 0.67f, TEXT,
         verification_uri && verification_uri[0]
             ? verification_uri
             : "https://discord.com/activate");

    text(62, 284, 0.95f, TEXT, "2. Skriv in koden");
    vita2d_draw_rectangle(62, 308, 420, 104, ACCENT);
    text(96, 375, 1.70f, TEXT, user_code && user_code[0] ? user_code : "----");

    vita2d_draw_rectangle(518, 308, 380, 104, PANEL);
    vita2d_draw_fill_circle(550, 341, 8, GREEN);
    text(572, 348, 0.59f, TEXT, "Väntar på Discord");
    text(550, 381, 0.51f, MUTED, status && status[0] ? status : "Kontrollerar automatiskt...");

    text(62, 490, 0.53f, MUTED, "X  Kontrollera nu");
    text(247, 490, 0.53f, MUTED, "O  Avbryt");

    end();
}

void dv_ui_draw_home(
    const DvGuildList *guilds,
    int selected,
    const char *username,
    const DvCapabilities *capabilities,
    const char *status
) {
    begin();

    /* layout */
    vita2d_draw_rectangle(0, 0, 72, 544, RAIL);
    vita2d_draw_rectangle(72, 0, 328, 544, PANEL);
    vita2d_draw_rectangle(400, 0, 560, 544, CHAT);

    /* brand */
    vita2d_draw_fill_circle(36, 34, 21, ACCENT);
    text(23, 41, 0.56f, TEXT, "DV");

    text(96, 39, 0.86f, TEXT, "Servrar");
    text(285, 38, 0.48f, DIM, "LIVE");
    divider(90, 56, 292);

    const int count = guilds ? guilds->count : 0;
    int first = selected - 4;
    if (first < 0) first = 0;
    if (first > count - 9) first = count - 9;
    if (first < 0) first = 0;

    for (int row = 0; row < 9; ++row) {
        int i = first + row;
        if (i >= count) break;

        int y = 70 + row * 43;
        const DvGuild *guild = &guilds->items[i];

        if (i == selected) {
            vita2d_draw_rectangle(84, y, 300, 37, SELECT);
            vita2d_draw_rectangle(84, y, 4, 37, ACCENT);
        }

        char initials[3];
        guild_initials(guild->name, initials);

        vita2d_draw_fill_circle(
            36, y + 18, 18, i == selected ? ACCENT : PANEL_2
        );
        text(27, y + 24, 0.51f, TEXT, initials);
        text(104, y + 25, 0.65f, i == selected ? TEXT : MUTED, guild->name);

        if (guild->owner) {
            text(334, y + 24, 0.40f, MUTED, "OWN");
        }
    }

    if (count == 0) {
        text(105, 105, 0.63f, MUTED, "Inga servrar hittades.");
    }

    /* right panel */
    text(428, 41, 0.86f, TEXT, "Översikt");
    divider(420, 56, 512);

    if (count > 0 && selected >= 0 && selected < count) {
        const DvGuild *g = &guilds->items[selected];

        char initials[3];
        guild_initials(g->name, initials);

        if (selected_guild_icon &&
            strcmp(selected_guild_icon_id, g->id) == 0) {
            draw_selected_icon(436.0f, 81.0f, 76.0f);
        } else {
            vita2d_draw_fill_circle(474, 119, 38, ACCENT);
            text(452, 131, 0.82f, TEXT, initials);
        }

        text(530, 109, 0.56f, MUTED, "VALD SERVER");
        text(530, 142, 0.90f, TEXT, g->name);

        text(428, 205, 0.53f, MUTED, "DISCORD VITA-FUNKTIONER");
        pill(428, 226, 104, "OAuth", capabilities && capabilities->oauth_login);
        pill(542, 226, 104, "Servrar", capabilities && capabilities->guilds);
        pill(656, 226, 104, "Kanaler", capabilities && capabilities->channels);
        pill(770, 226, 104, "Chat", capabilities && capabilities->messages);

        vita2d_draw_rectangle(428, 286, 444, 104, PANEL);
        text(449, 318, 0.59f, TEXT, "Serverdata är live från Discord.");
        text(449, 347, 0.53f, MUTED, "Kanaler och chat körs via Discord Vita-boten.");
        text(449, 371, 0.53f, MUTED, "Privata kanaler filtreras efter användarens rättigheter.");

        text(428, 433, 0.52f, MUTED, "X  Kanaler");
        text(568, 433, 0.52f, MUTED, "START  Konto");
        text(731, 433, 0.52f, MUTED, "O  Avsluta");
        text(428, 462, 0.46f, DIM, "Touch: tryck på en server för att välja den");
    }

    /* status/account bar */
    vita2d_draw_rectangle(72, 482, 888, 62, RAIL);
    vita2d_draw_fill_circle(103, 513, 17, GREEN);
    text(128, 509, 0.64f, TEXT,
         username && username[0] ? username : "Discord user");
    text(128, 529, 0.47f, MUTED, "Inloggad");

    if (status && status[0]) text(420, 519, 0.51f, MUTED, status);

    end();
}

void dv_ui_draw_server_details(
    const DvGuild *guild,
    const char *username,
    const DvCapabilities *capabilities
) {
    begin();
    top_bar("Serverinfo", guild ? guild->name : "");

    char initials[3];
    guild_initials(guild ? guild->name : "", initials);

    if (guild && selected_guild_icon &&
        strcmp(selected_guild_icon_id, guild->id) == 0) {
        draw_selected_icon(60.0f, 95.0f, 84.0f);
    } else {
        vita2d_draw_fill_circle(102, 137, 42, ACCENT);
        text(77, 151, 0.92f, TEXT, initials);
    }

    text(166, 120, 0.55f, MUTED, "SERVER");
    text(166, 154, 1.02f, TEXT, guild ? guild->name : "Okänd server");

    if (guild && guild->owner) {
        vita2d_draw_rectangle(166, 174, 110, 28, ACCENT_2);
        text(181, 194, 0.47f, TEXT, "Du äger den");
    }

    text(62, 259, 0.54f, MUTED, "SERVER-ID");
    text(62, 291, 0.66f, TEXT, guild ? guild->id : "-");

    divider(62, 319, 836);

    text(62, 360, 0.54f, MUTED, "TILLGÄNGLIGT I DENNA BUILD");
    text(62, 395, 0.62f,
         capabilities && capabilities->guilds ? GREEN : MUTED,
         "[+] Serverlista");
    text(266, 395, 0.62f,
         capabilities && capabilities->channels ? GREEN : MUTED,
         capabilities && capabilities->channels ? "[+] Kanaler" : "[-] Kanaler");
    text(432, 395, 0.62f,
         capabilities && capabilities->messages ? GREEN : MUTED,
         capabilities && capabilities->messages ? "[+] Meddelanden" : "[-] Meddelanden");
    text(656, 395, 0.62f,
         capabilities && capabilities->send_messages ? GREEN : MUTED,
         capabilities && capabilities->send_messages ? "[+] Skicka" : "[-] Skicka");

    text(62, 468, 0.52f, MUTED, "O  Tillbaka");
    text(758, 468, 0.50f, DIM,
         username && username[0] ? username : "Discord user");

    end();
}

void dv_ui_draw_account_menu(
    const char *username,
    int logout_selected
) {
    begin();

    /* dimmed background look */
    vita2d_draw_rectangle(0, 0, 960, 544, RAIL);
    vita2d_draw_rectangle(280, 95, 400, 350, PANEL);

    text(316, 143, 0.90f, TEXT, "Konto");
    divider(310, 162, 340);

    vita2d_draw_fill_circle(342, 214, 25, GREEN);
    text(388, 211, 0.67f, TEXT,
         username && username[0] ? username : "Discord user");
    text(388, 234, 0.49f, MUTED, "Discord-session aktiv");

    vita2d_draw_rectangle(
        310, 292, 340, 52, logout_selected ? RED : PANEL_2
    );
    text(389, 325, 0.65f, TEXT, "Logga ut");

    text(310, 397, 0.50f, MUTED, "X  Bekräfta");
    text(512, 397, 0.50f, MUTED, "O  Avbryt");

    end();
}


static void ellipsis_copy(
    const char *src,
    char *dst,
    unsigned int dst_size,
    unsigned int max_chars
) {
    if (!dst || dst_size == 0) return;
    dst[0] = 0;
    if (!src) return;

    unsigned int len = (unsigned int)strlen(src);
    if (len <= max_chars) {
        snprintf(dst, dst_size, "%s", src);
        return;
    }

    if (max_chars < 4) {
        snprintf(dst, dst_size, "%.*s", (int)max_chars, src);
        return;
    }

    snprintf(dst, dst_size, "%.*s...", (int)(max_chars - 3), src);
}

void dv_ui_draw_channels(
    const DvGuild *guild,
    const DvChannelList *channels,
    int selected,
    const char *username,
    const char *status
) {
    begin();

    vita2d_draw_rectangle(0, 0, 72, 544, RAIL);
    vita2d_draw_rectangle(72, 0, 330, 544, PANEL);
    vita2d_draw_rectangle(402, 0, 558, 544, CHAT);

    vita2d_draw_fill_circle(36, 34, 21, ACCENT);
    text(23, 41, 0.56f, TEXT, "DV");

    text(96, 38, 0.80f, TEXT, guild ? guild->name : "Server");
    divider(90, 56, 294);

    int count = channels ? channels->count : 0;
    int first = selected - 4;
    if (first < 0) first = 0;
    if (first > count - 9) first = count - 9;
    if (first < 0) first = 0;

    for (int row = 0; row < 9; ++row) {
        int i = first + row;
        if (i >= count) break;

        int y = 72 + row * 43;
        const DvChannel *channel = &channels->items[i];

        if (i == selected) {
            vita2d_draw_rectangle(84, y, 302, 37, SELECT);
            vita2d_draw_rectangle(84, y, 4, 37, ACCENT);
        }

        text(102, y + 25, 0.66f,
             i == selected ? TEXT : MUTED, "#");

        char name[38];
        ellipsis_copy(channel->name, name, sizeof(name), 29);
        text(127, y + 25, 0.64f,
             i == selected ? TEXT : MUTED, name);
    }

    text(430, 42, 0.88f, TEXT, "Välj kanal");
    divider(420, 57, 510);

    if (count > 0 && selected >= 0 && selected < count) {
        text(430, 110, 0.53f, MUTED, "VALD KANAL");
        text(430, 149, 1.02f, TEXT, "#");
        text(460, 149, 1.02f, TEXT, channels->items[selected].name);

        vita2d_draw_rectangle(430, 199, 430, 94, PANEL);
        text(452, 232, 0.60f, TEXT,
             "X öppnar live-meddelanden.");
        text(452, 261, 0.52f, MUTED,
             "Åtkomst beror på botens kanalbehörigheter.");
    } else {
        text(430, 112, 0.62f, MUTED,
             "Inga textkanaler som boten kan lista.");
    }

    vita2d_draw_rectangle(72, 482, 888, 62, RAIL);
    vita2d_draw_fill_circle(103, 513, 17, GREEN);
    text(128, 509, 0.64f, TEXT,
         username && username[0] ? username : "Discord user");
    text(128, 529, 0.47f, MUTED, "OAuth-användare");

    if (status && status[0]) text(430, 519, 0.50f, MUTED, status);
    text(711, 454, 0.49f, MUTED, "↑↓ Välj   X Öppna   O Tillbaka");

    end();
}

void dv_ui_draw_chat(
    const DvGuild *guild,
    const DvChannel *channel,
    const DvMessageList *messages,
    const char *username,
    const char *status
) {
    begin();

    vita2d_draw_rectangle(0, 0, 230, 544, PANEL);
    vita2d_draw_rectangle(230, 0, 730, 544, CHAT);

    text(24, 39, 0.74f, TEXT, guild ? guild->name : "Server");
    divider(18, 56, 194);

    text(24, 93, 0.53f, MUTED, "KANAL");
    text(24, 126, 0.74f, TEXT, "#");
    text(48, 126, 0.74f, TEXT, channel ? channel->name : "channel");

    text(24, 189, 0.53f, MUTED, "SKICKA SOM");
    text(24, 221, 0.61f, TEXT, "Discord Vita-boten");
    text(24, 247, 0.48f, DIM,
         "Inte ditt privata användarkonto.");

    text(24, 466, 0.48f, MUTED, "O  Kanaler");
    text(24, 494, 0.48f, MUTED, "□  Skriv");
    text(24, 522, 0.48f, MUTED, "△  Uppdatera");

    text(256, 39, 0.88f, TEXT, "#");
    text(281, 39, 0.88f, TEXT, channel ? channel->name : "channel");
    divider(248, 56, 688);

    int count = messages ? messages->count : 0;
    int first = count > 8 ? count - 8 : 0;
    int y = 91;

    for (int i = first; i < count; ++i) {
        const DvMessage *message = &messages->items[i];

        char author[42];
        char content[78];
        ellipsis_copy(message->author, author, sizeof(author), 30);
        ellipsis_copy(message->content, content, sizeof(content), 65);

        text(260, y, 0.61f,
             message->author_is_bot ? ACCENT : TEXT,
             author);
        y += 23;
        text(260, y, 0.58f, MUTED,
             content[0] ? content : "(meddelande utan text)");
        y += 43;
        if (y > 438) break;
    }

    if (count == 0) {
        text(260, 108, 0.58f, MUTED,
             "Inga meddelanden, eller boten saknar läsrättighet.");
    }

    vita2d_draw_rectangle(248, 474, 688, 49, PANEL);
    text(270, 506, 0.59f, MUTED,
         "□  Skriv ett meddelande som Discord Vita-boten...");

    if (status && status[0]) {
        vita2d_draw_rectangle(500, 65, 426, 32, PANEL_2);
        text(516, 87, 0.47f, TEXT, status);
    }

    (void)username;
    end();
}
