/* ui.c
   @mod: ui
   @breadcrumb: ui/input
   Keyboard input: translates key presses into reader commands.
*/

#include "95read.h"
#include <conio.h>   /* getch(), putch() */

/*
  Keys:
    SPACE  -> next page
    ENTER  -> next page
    BS     -> prev page
    ESC    -> quit
    N/P/C  -> next/prev/quit (or runtime-configured values, case-insensitive)
    B/E    -> begin/end
    G      -> goto percentage prompt
    I      -> invert colors (case-insensitive)              // @note: documented explicitly
    T      -> open inversion test screen (case-insensitive) // @note: added T support
    Arrows/Home/End/PgUp/PgDn -> intuitive navigation
  Ignores all other keys (including other extended keys).
*/

/* @region: input
   @breadcrumb: ui/my_toupper */
static unsigned char my_toupper(unsigned char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

/* @region: input
   @breadcrumb: ui/ui_get_cmd */
char ui_get_cmd(void) {
    int c;  /* must be int to catch all getch() return values */

    for (;;) {
        /* block here until a key is pressed */
        c = getch();

        /* handle extended-key prefix (0 or 0xE0) */
        if (c == 0 || c == 0xE0) {
            c = getch();  /* get scan code */
            switch (c) {
                case 71: return 'B';                /* Home -> begin */
                case 79: return 'E';                /* End  -> end   */
                case 73: return cfg.key_prev_page;  /* PgUp -> prev  */
                case 81: return cfg.key_next_page;  /* PgDn -> next  */
                case 72:                            /* Up    -> prev */
                case 75:                            /* Left  -> prev */
                    return cfg.key_prev_page;
                case 80:                            /* Down  -> next */
                case 77:                            /* Right -> next */
                    return cfg.key_next_page;
                default:
                    continue;  /* ignore other extended */
            }
        }

        /* ENTER -> next page */
        if (c == 13) {
            return cfg.key_next_page;
        }

        /* BACKSPACE -> prev page */
        if (c == 8) {
            return cfg.key_prev_page;
        }

        /* space -> next page */
        if (c == ' ') {
            return cfg.key_next_page;
        }

        /* ESC -> quit */
        if (c == 27) {
            return cfg.key_quit;
        }

        /* compile-time extra commands: Begin/End/Goto/Find/Help/Jump/Reload (case-insensitive) */
        if (my_toupper((unsigned char)c) == KEY_BEGIN) {
            return KEY_BEGIN;
        }
        if (my_toupper((unsigned char)c) == KEY_END) {
            return KEY_END;
        }
        if (my_toupper((unsigned char)c) == KEY_GOTO) {
            return KEY_GOTO;
        }
        if (my_toupper((unsigned char)c) == KEY_FIND) {
            return KEY_FIND;
        }
        if (my_toupper((unsigned char)c) == KEY_HELP) {
            return KEY_HELP;
        }
        if (my_toupper((unsigned char)c) == KEY_GOTO_LINE) {
            return KEY_GOTO_LINE;
        }
        if (my_toupper((unsigned char)c) == KEY_RELOAD) {
            return KEY_RELOAD;
        }

        /* user-configured keys for next/prev/quit (case-insensitive) */
        if (my_toupper((unsigned char)c) == my_toupper((unsigned char)cfg.key_next_page)) {
            return cfg.key_next_page;
        }
        if (my_toupper((unsigned char)c) == my_toupper((unsigned char)cfg.key_prev_page)) {
            return cfg.key_prev_page;
        }
        if (my_toupper((unsigned char)c) == my_toupper((unsigned char)cfg.key_quit)) {
            return cfg.key_quit;
        }
        if (my_toupper((unsigned char)c) == 'Q') {
            return cfg.key_quit;
        }

        /* invert colors (case-insensitive) */
        if (my_toupper((unsigned char)c) == 'I') {
            return 'I';
        }

        /* @note: inversion test command (case-insensitive) */
        if (my_toupper((unsigned char)c) == 'T') {
            return 'T';
        }

        /* otherwise ignore and loop */
    }
}
