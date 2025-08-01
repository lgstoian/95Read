#include "95read.h"
#include <conio.h>   /* for clrscr(), gotoxy(), putch() */

int display_page(const char *buf) {
    const char *p = buf;
    int row = 1, col = 1;
    char ch;
    const int TAB_WIDTH = 8;

    clrscr();
    gotoxy(1, 1);

    while (row <= SCREEN_LINES && (ch = *p) != '\0') {
        p++;

        /* Handle form-feed: clear & reset */
        if (ch == '\f') {
            clrscr();
            row = col = 1;
            gotoxy(1, 1);
            continue;
        }

        /* Skip carriage returns */
        if (ch == '\r') {
            continue;
        }

        /* Expand tabs */
        if (ch == '\t') {
            int spaces = TAB_WIDTH - (col - 1) % TAB_WIDTH;
            while (spaces-- > 0 && row <= SCREEN_LINES) {
                if (col > SCREEN_COLS) {
                    row++;
                    col = 1;
                    if (row > SCREEN_LINES) break;
                    gotoxy(1, row);
                }
                putch(' ');
                col++;
            }
            continue;
        }

        /* Newline: move to next line */
        if (ch == '\n') {
            row++;
            col = 1;
            if (row > SCREEN_LINES) break;
            gotoxy(1, row);
            continue;
        }

        /* Wrap on column overflow */
        if (col > SCREEN_COLS) {
            row++;
            col = 1;
            if (row > SCREEN_LINES) break;
            gotoxy(1, row);
        }

        /* Draw character */
        putch(ch);
        col++;
    }

    /* Clear any leftover lines for a clean page */
    while (++row <= SCREEN_LINES) {
        gotoxy(1, row);
        for (col = 1; col <= SCREEN_COLS; col++) {
            putch(' ');
        }
    }

    /* Return how many bytes we advanced in the buffer */
    return (int)(p - buf);
}
