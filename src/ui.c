#include "95read.h"
#include <conio.h>   /* getch(), putch() */

/*
  Keys:
    SPACE  -> next page
    ESC    -> quit
    N/P/C  -> next/prev/quit (or runtime-configured values)
    B/E    -> begin/end
    G      -> goto percentage prompt
  Ignores all other keys (including extended arrows).
*/
char ui_get_cmd(void) {
    int c;  /* must be int to catch all getch() return values */

    for (;;) {
        /* block here until a key is pressed */
        c = getch();

        /* handle extended-key prefix (0 or 0xE0), discard the scan code */
        if (c == 0 || c == 0xE0) {
            (void)getch();
            continue;
        }

        /* normalize lowercase letters to uppercase */
        if (c >= 'a' && c <= 'z') {
            c -= ('a' - 'A');
        }

        /* space -> next page (map to configured "next" so main's checks pass) */
        if (c == ' ') {
            return cfg.key_next_page;
        }

        /* ESC -> quit (main also recognizes ESC explicitly) */
        if (c == 27) {
            return cfg.key_quit;
        }

        /* user-configured keys for next/prev/quit */
        if ((unsigned char)c == (unsigned char)cfg.key_next_page ||
            (unsigned char)c == (unsigned char)cfg.key_prev_page ||
            (unsigned char)c == (unsigned char)cfg.key_quit)
        {
            return (char)c;
        }

        /* compile-time extra commands: Begin/End/Goto */
        if (c == KEY_BEGIN || c == (KEY_BEGIN + 32)) {
            return (char)KEY_BEGIN;
        }
        if (c == KEY_END || c == (KEY_END + 32)) {
            return (char)KEY_END;
        }
        if (c == KEY_GOTO || c == (KEY_GOTO + 32)) {
            return (char)KEY_GOTO;
        }

        /* otherwise ignore and loop */
    }
}
