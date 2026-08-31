#include <psp2/apputil.h>
#include <psp2/sysmodule.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>
#include <stdint.h>
#include <string.h>
#include "ime.h"

static int initialized = 0;
static int ime_module_loaded = 0;

static unsigned int utf16_to_utf8(
    const uint16_t *input,
    char *out,
    unsigned int out_size
) {
    unsigned int used = 0;
    if (!input || !out || out_size == 0) return 0;

    for (unsigned int i = 0; input[i]; ++i) {
        uint32_t cp = input[i];

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            uint16_t low = input[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 +
                    (((uint32_t)input[i] - 0xD800) << 10) +
                    ((uint32_t)low - 0xDC00);
                i++;
            } else {
                cp = '?';
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = '?';
        }

        unsigned char bytes[4];
        unsigned int count = 0;

        if (cp <= 0x7F) {
            bytes[0] = (unsigned char)cp;
            count = 1;
        } else if (cp <= 0x7FF) {
            bytes[0] = 0xC0 | (cp >> 6);
            bytes[1] = 0x80 | (cp & 0x3F);
            count = 2;
        } else if (cp <= 0xFFFF) {
            bytes[0] = 0xE0 | (cp >> 12);
            bytes[1] = 0x80 | ((cp >> 6) & 0x3F);
            bytes[2] = 0x80 | (cp & 0x3F);
            count = 3;
        } else {
            bytes[0] = 0xF0 | (cp >> 18);
            bytes[1] = 0x80 | ((cp >> 12) & 0x3F);
            bytes[2] = 0x80 | ((cp >> 6) & 0x3F);
            bytes[3] = 0x80 | (cp & 0x3F);
            count = 4;
        }

        if (used + count + 1 > out_size) break;
        memcpy(out + used, bytes, count);
        used += count;
    }

    out[used] = 0;
    return used;
}

int dv_ime_init(void) {
    if (initialized) return 0;

    int r = sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
    if (r < 0) return r;
    ime_module_loaded = 1;

    SceAppUtilInitParam init_param;
    SceAppUtilBootParam boot_param;
    memset(&init_param, 0, sizeof(init_param));
    memset(&boot_param, 0, sizeof(boot_param));

    r = sceAppUtilInit(&init_param, &boot_param);
    if (r < 0) {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
        ime_module_loaded = 0;
        return r;
    }

    SceCommonDialogConfigParam config;
    memset(&config, 0, sizeof(config));
    r = sceCommonDialogSetConfigParam(&config);
    if (r < 0) {
        sceAppUtilShutdown();
        sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
        ime_module_loaded = 0;
        return r;
    }

    initialized = 1;
    return 0;
}

void dv_ime_term(void) {
    if (initialized) {
        sceAppUtilShutdown();
        initialized = 0;
    }

    if (ime_module_loaded) {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
        ime_module_loaded = 0;
    }
}

int dv_ime_prompt_message(char *out, unsigned int out_size) {
    if (!initialized || !out || out_size < 2) return -1;

    static const uint16_t title[] = {
        'S','k','r','i','v',' ',
        'm','e','d','d','e','l','a','n','d','e',0
    };

    static uint16_t input[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
    memset(input, 0, sizeof(input));

    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    param.supportedLanguages = SCE_IME_LANGUAGE_SWEDISH | SCE_IME_LANGUAGE_ENGLISH;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.option = SCE_IME_OPTION_MULTILINE;
    param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    param.title = title;
    param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
    param.initialText = input;
    param.inputTextBuffer = input;
    param.enterLabel = SCE_IME_ENTER_LABEL_SEND;

    int r = sceImeDialogInit(&param);
    if (r < 0) return r;

    while (1) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();

        vita2d_common_dialog_update();
        vita2d_swap_buffers();

        SceCommonDialogStatus status = sceImeDialogGetStatus();
        if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
            SceImeDialogResult result;
            memset(&result, 0, sizeof(result));

            r = sceImeDialogGetResult(&result);
            sceImeDialogTerm();

            if (r < 0) return r;
            if (result.button != SCE_IME_DIALOG_BUTTON_ENTER) {
                out[0] = 0;
                return 0;
            }

            utf16_to_utf8(input, out, out_size);
            return out[0] ? 1 : 0;
        }

        sceKernelDelayThread(16667);
    }
}
