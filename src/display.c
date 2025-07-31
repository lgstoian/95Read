#include "95read.h"
#include <conio.h>

/*
  Display exactly up to 16 lines × 40 cols.
  Returns the number of bytes from buf that were actually drawn.
*/
int display_page(const char *buf) {
    const char *p = buf;
    int row = 1, col = 1;
    char ch;

    clrscr();
    gotoxy(1, 1);

    while (row <= SCREEN_LINES && (ch = *p) != '\0') {
        /* Advance pointer now so continue/break logic works */
        p++;

        /* Skip stray carriage returns */
        if (ch == '\r') {
            continue;
        }

        /* Newline: move to next line */
        if (ch == '\n') {
            row++;
            col = 1;
            if (row > SCREEN_LINES) {
                break;
            }
            gotoxy(1, row);
            continue;
        }

        /* Column overflow: wrap before printing */
        if (col > SCREEN_COLS) {
            row++;
            col = 1;
            if (row > SCREEN_LINES) {
                break;
            }
            gotoxy(1, row);
        }

        /* Draw character */
        putch(ch);
        col++;
    }

    /* Return how many bytes we advanced in the buffer */
    return (int)(p - buf);
}
