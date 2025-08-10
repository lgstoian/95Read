#include "95read.h"
#include <conio.h>    /* clrscr(), gotoxy(), putch() */
#include <stddef.h>   /* size_t */

/* Emit CRLF using putch to ensure column reset on HP 95LX BIOS */
static void emit_crlf(void) {
    putch('\r');
    putch('\n');
}

/*
  display_page
  ------------
  - Renders the given page buffer into the HP 95LX 40x16 text screen.
  - Uses effective dimensions (EFF_SCREEN_LINES/COLS) to honor runtime
    config while staying within compile-time buffer sizes.
  - Reserves the last effective screen line for the footer/status line
    (drawn by main), so it never overwrites the status area.
  - Avoids mid-line gotoxy/clreol (HP 95LX can shift output left when
    frequently repositioning). Uses sequential output and explicit CRLF.
  - Safely handles control characters:
      \b backspace   : erases one column (no wrap to prior line)
      \f form feed   : clears and resets
      \r carriage rtn: ignored (CRLF handled by explicit emit_crlf)
      \t tab         : expands using cfg.tab_width (clamped 1..16)
      \n newline     : moves to start of next line via CRLF
    All other bytes are printed as-is.
  - Returns the number of input bytes consumed from buf.
  - Stops when either:
      * NUL byte encountered, or
      * Reaches the bottom (view_rows) of the screen.
*/
int display_page(const char *buf) {
    const unsigned char *p = (const unsigned char *)buf;
    int row, col;
    unsigned char ch;
    int bytes = 0;
    int done = 0;

    /* Effective geometry; reserve last line for status/footer if possible */
    const int eff_lines = EFF_SCREEN_LINES;
    const int eff_cols  = EFF_SCREEN_COLS;
    const int view_rows = (eff_lines > 1) ? (eff_lines - 1) : eff_lines;

    /* Clamp tab width to a safe range (prevents div-by-zero and huge loops) */
    int tabw = cfg.tab_width;
    if (tabw < 1)  tabw = 4;
    if (tabw > 16) tabw = 16;

    /* Prepare screen: clear and position to the top-left */
    clrscr();
    gotoxy(1, 1);
    row = 1;
    col = 1;

    while (!done && (ch = *p) != '\0') {
        p++;
        bytes++;

        switch (ch) {
        case '\b':  /* backspace: erase one character (no line wrap) */
            if (col > 1) {
                /* Move left visually: backspace, overwrite with space, backspace again */
                putch('\b');
                putch(' ');
                putch('\b');
                col--;
            }
            break;

        case '\f':  /* form feed: clear and reset */
            clrscr();
            gotoxy(1, 1);
            row = 1;
            col = 1;
            break;

        case '\r':  /* ignore carriage return; handle newline via \n -> CRLF */
            break;

        case '\t': {  /* expand tab to next multiple of tabw */
            int spaces = tabw - ((col - 1) % tabw);
            while (spaces-- > 0 && !done) {
                if (col > eff_cols) {
                    /* wrap to next line */
                    row++;
                    col = 1;
                    if (row > view_rows) { done = 1; break; }
                    emit_crlf();
                }
                putch(' ');
                col++;
            }
            break;
        }

        case '\n':  /* newline: move to start of next line */
            row++;
            col = 1;
            if (row > view_rows) { done = 1; }
            else { emit_crlf(); }
            break;

        default:    /* printable or extended byte: print and wrap if needed */
            if (col > eff_cols) {
                /* wrap before printing */
                row++;
                col = 1;
                if (row > view_rows) { done = 1; break; }
                emit_crlf();
            }
            if (!done) {
                putch(ch);
                col++;
            }
            break;
        }
    }

    /* Clear any leftover lines below the last rendered row within view */
    if (!done) {
        int r;

        /* Clear remainder of current line to the right */
        while (col <= eff_cols && row <= view_rows) {
            putch(' ');
            col++;
        }

        /* Clear subsequent lines (fill with spaces) */
        for (r = row + 1; r <= view_rows; ++r) {
            emit_crlf();          /* move to next line start */
            /* paint the whole line as spaces */
            {
                int c2;
                for (c2 = 0; c2 < eff_cols; ++c2) {
                    putch(' ');
                }
            }
        }
    }

    return bytes;
}
