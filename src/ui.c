#include "95read.h"
#include <conio.h>   /* for getch() */

/*
  SPACE   = next page
  ESC     = quit
  ignores all other keys (including extended arrows)
*/
char ui_get_cmd(void) {
    int c;  /* must be int to catch all getch() return values */

    while (1) {
        /* block here until a key is pressed */
        c = getch();

        /* handle extended‐key prefix (0 or 0xE0), discard the scan code */
        if (c == 0 || c == 0xE0) {
            getch();
            continue;
        }

        /* normalize lowercase letters to uppercase */
        if (c >= 'a' && c <= 'z') {
            c -= ('a' - 'A');
        }

        /* space → next page */
        if (c == ' ') {
            return cfg.key_next_page;
        }

        /* ESC → quit */
        if (c == 27) {
            return cfg.key_quit;
        }

        /* user‐configured keys */
        if ((unsigned char)c == (unsigned char)cfg.key_next_page ||
            (unsigned char)c == (unsigned char)cfg.key_prev_page ||
            (unsigned char)c == (unsigned char)cfg.key_quit)
        {
            return (char)c;
        }

        /* otherwise ignore and loop */
    }
}
