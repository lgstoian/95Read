#include "95read.h"
#include <dos.h>

char ui_get_cmd(void) {
    union REGS in, out;
    char       c;

    while (1) {
        /* Poll keyboard (AH=1) */
        in.h.ah = 0x01;
        int86(0x16, &in, &out);
        if (out.x.flags & 0x0040) {
            continue;  /* no key waiting */
        }

        /* Read key (AH=0) */
        in.h.ah = 0x00;
        int86(0x16, &in, &out);
        c = out.h.al;

        /* Map lowercase to uppercase */
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }

        /* Accept only N, P or C */
        if (c == KEY_NEXT_PAGE ||
            c == KEY_PREV_PAGE ||
            c == KEY_QUIT)
        {
            return c;
        }
    }
}
