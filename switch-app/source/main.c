#include <switch.h>
#include <stdio.h>

#define UNTIS_NEXT_URL "https://hannonuppi.github.io/JDNEXT_UNTIS/"
#define ADMIN_CENTER_URL "https://hannonuppi.github.io/Tobiservices-Account/admin.html"

static const char *menu_items[] = {
    "Untis Next",
    "TobiServices Admin Center"
};

static void draw_menu(unsigned selected, const char *status) {
    consoleClear();
    printf("\x1b[1;1H");
    printf("========================================\n");
    printf("        UNTIS NEXT - SWITCH PORTAL      \n");
    printf("========================================\n\n");
    printf("  Steuerung: UP/DOWN waehlen, A oeffnen\n");
    printf("             PLUS beendet die App\n\n");

    for (unsigned i = 0; i < 2; i++) {
        printf("  %s %s\n", i == selected ? ">" : " ", menu_items[i]);
    }

    printf("\n----------------------------------------\n");
    printf("  Internetverbindung erforderlich.\n");
    printf("  Anmeldungen erfolgen auf der jeweiligen\n");
    printf("  Website in der Switch-Webansicht.\n");

    if (status && status[0]) {
        printf("\n%s\n", status);
    }

    printf("\n");
    consoleUpdate(NULL);
}

static Result open_website(const char *url) {
    WebCommonConfig config;

    // webNewsCreate permits the initial URL's host for the Web applet.
    Result rc = webNewsCreate(&config, url);
    if (R_FAILED(rc)) {
        return rc;
    }

    // Cursor navigation is easier with a controller than a free pointer.
    (void)webConfigSetLeftStickMode(&config, WebLeftStickMode_Cursor);
    return webConfigShow(&config, NULL);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    unsigned selected = 0;
    char status[160] = "Bereit.";
    draw_menu(selected, status);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        bool redraw = false;

        if (down & HidNpadButton_Plus) {
            break;
        }

        if (down & HidNpadButton_Up) {
            selected = (selected + 1) % 2;
            status[0] = '\0';
            redraw = true;
        }

        if (down & HidNpadButton_Down) {
            selected = (selected + 1) % 2;
            status[0] = '\0';
            redraw = true;
        }

        if (down & HidNpadButton_A) {
            const char *url = selected == 0 ? UNTIS_NEXT_URL : ADMIN_CENTER_URL;
            snprintf(status, sizeof(status), "Oeffne %s ...", menu_items[selected]);
            draw_menu(selected, status);

            const Result rc = open_website(url);
            if (R_FAILED(rc)) {
                snprintf(status, sizeof(status),
                    "Webansicht konnte nicht geoeffnet werden (0x%08lX).",
                    (unsigned long)rc);
            } else {
                snprintf(status, sizeof(status),
                    "Zurueck aus %s. A oeffnet die Seite erneut.",
                    menu_items[selected]);
            }
            redraw = true;
        }

        if (redraw) {
            draw_menu(selected, status);
        }

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
