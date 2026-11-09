#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 960
#define SCREEN_H 544

static void draw_text(vita2d_pgf *font, float x, float y, unsigned int colour, const char *text)
{
    vita2d_pgf_draw_text(font, x, y, colour, 1.0f, text);
}

int main(void)
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;
    int selected_tab = 0;
    const char *tabs[] = { "Discover", "Categories", "Downloads", "Library" };
    const int tab_count = 4;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    if (vita2d_init() < 0) {
        sceKernelExitProcess(1);
        return 1;
    }

    vita2d_pgf *font = vita2d_pgf_load_default();
    if (!font) {
        vita2d_fini();
        sceKernelExitProcess(1);
        return 1;
    }

    while (1) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);

        unsigned int pressed = pad.buttons & ~previous_buttons;
        previous_buttons = pad.buttons;

        if (pressed & SCE_CTRL_LEFT) {
            selected_tab = (selected_tab + tab_count - 1) % tab_count;
        }
        if (pressed & SCE_CTRL_RIGHT) {
            selected_tab = (selected_tab + 1) % tab_count;
        }
        if (pressed & SCE_CTRL_START) {
            break;
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_draw_rectangle(0, 0, SCREEN_W, 74, RGBA8(17, 22, 36, 255));
        vita2d_draw_rectangle(0, 74, SCREEN_W, 3, RGBA8(70, 150, 255, 255));

        draw_text(font, 38, 48, RGBA8(255, 255, 255, 255), "DSHOP");
        draw_text(font, 830, 45, RGBA8(145, 165, 195, 255), "PS VITA");

        for (int i = 0; i < tab_count; i++) {
            unsigned int colour = (i == selected_tab)
                ? RGBA8(90, 175, 255, 255)
                : RGBA8(175, 185, 205, 255);
            draw_text(font, 42 + (i * 220), 116, colour, tabs[i]);
        }

        vita2d_draw_rectangle(36, 137, SCREEN_W - 72, 2, RGBA8(55, 68, 90, 255));

        draw_text(font, 48, 205, RGBA8(255, 255, 255, 255), tabs[selected_tab]);

        if (selected_tab == 0) {
            draw_text(font, 48, 255, RGBA8(175, 190, 215, 255),
                "Welcome to DShop, your independent homebrew store.");
            draw_text(font, 48, 295, RGBA8(135, 155, 185, 255),
                "The store catalogue is being prepared.");
        } else if (selected_tab == 1) {
            draw_text(font, 48, 255, RGBA8(175, 190, 215, 255),
                "Games   Ports   Emulators   Utilities   Applications");
        } else if (selected_tab == 2) {
            draw_text(font, 48, 255, RGBA8(175, 190, 215, 255),
                "Your downloads will appear here.");
        } else {
            draw_text(font, 48, 255, RGBA8(175, 190, 215, 255),
                "Installed applications will appear here.");
        }

        draw_text(font, 42, 510, RGBA8(125, 140, 165, 255),
            "LEFT / RIGHT: Navigate tabs                         START: Exit");

        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
    }

    vita2d_pgf_free(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
