#include "95read.h"
#include <conio.h>   /* for kbhit(), getch() */

char ui_get_cmd(void) {
    char c;

    while (1) {
        /* Wait for any keypress */
        while (!kbhit())
            ;
        c = getch();

        /* Map lowercase to uppercase */
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }

        /* Accept only N, P, or C */
        if (c == KEY_NEXT_PAGE ||
            c == KEY_PREV_PAGE ||
            c == KEY_QUIT)
        {
            return c;
        }
    }
}
