#include "95read.h"
#include <conio.h>    /* for clrscr(), gotoxy(), putch() */
#include <stddef.h>   /* for size_t */

int display_page(const char *buf) {
    const unsigned char *p = (const unsigned char *)buf;
    int row = 1, col = 1;
    unsigned char ch;
    int bytes = 0;

    clrscr();
    gotoxy(1, 1);

    /* Render characters until we hit the bottom of the screen or a NUL */
    while (row <= cfg.screen_lines && (ch = *p) != '\0') {
        p++;
        bytes++;

        switch (ch) {
        case '\b':  /* backspace: erase one character */
            if (col > 1) {
                col--;
                gotoxy(col, row);
                putch(' ');
                gotoxy(col, row);
            }
            break;

        case '\f':  /* form feed: clear and reset */
            clrscr();
            row = 1;
            col = 1;
            gotoxy(1, 1);
            break;

        case '\r':  /* ignore carriage return */
            break;

        case '\t': {  /* expand tab */
            int spaces = cfg.tab_width - ((col - 1) % cfg.tab_width);
            while (spaces-- > 0 && row <= cfg.screen_lines) {
                if (col > cfg.screen_cols) {
                    row++;
                    col = 1;
                    if (row > cfg.screen_lines) break;
                    gotoxy(col, row);
                }
                putch(' ');
                col++;
            }
            break;
        }

        case '\n':  /* newline: move to start of next line */
            row++;
            col = 1;
            if (row > cfg.screen_lines) break;
            gotoxy(1, row);
            break;

        default:    /* printable character */
            if (col > cfg.screen_cols) {
                row++;
                col = 1;
                if (row > cfg.screen_lines) break;
                gotoxy(1, row);
            }
            putch(ch);
            col++;
            break;
        }
    }

    /* Clear any leftover lines below the last rendered row */
    while (++row <= cfg.screen_lines) {
        int c2;
        gotoxy(1, row);
        for (c2 = 0; c2 < cfg.screen_cols; c2++) {
            putch(' ');
        }
    }

    return bytes;
}
