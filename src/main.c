#include "95read.h"
#include <conio.h>   /* clrscr(), gotoxy(), cprintf(), clreol(), getch(), putch() */
#include <stdio.h>   /* sprintf */
#include <string.h>  /* strlen */

/*---------------------------------------------------------------
  Draw a one-line footer with progress and key help.
  Uses last effective screen line to avoid overlapping main text.
  - Adapts to screen width to prevent wrapping on HP 95LX (40 cols).
  - Short form (<=40 cols): "[xxx%] N/P G B E C"
  - Long form (>=48 cols):  "[xxx%] N:Next P:Prev G:Goto B:Begin E:End C:Quit"
---------------------------------------------------------------*/
static void draw_status(const ReaderState *rs) {
    int row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
    int cols = (EFF_SCREEN_COLS > 0) ? EFF_SCREEN_COLS : SCREEN_COLS;
    int percent;
    char line[128];

    /* Compute percentage based on current page start offset */
    if (rs->file_size > 0L) {
        percent = (int)((100L * rs->offset) / rs->file_size);
        if (percent > 100) percent = 100;
        if (percent < 0)   percent = 0;
    } else {
        percent = 0;
    }

    /* Choose compact vs. verbose legend based on width */
    if (cols >= 48) {
        /* Verbose legend for wider screens */
        sprintf(line, "[%3d%%] N:Next P:Prev G:Goto B:Begin E:End C:Quit", percent);
    } else {
        /* Compact legend guaranteed to fit 40 columns */
        sprintf(line, "[%3d%%] N/P G B E C", percent);
    }

    /* Ensure we never exceed the visible width (avoid wrap/overwrite) */
    if (cols > 0) {
        size_t len = strlen(line);
        if ((int)len > cols) {
            line[cols] = '\0';
        }
    }

    /* Draw on last line */
    gotoxy(1, row);
    clreol();
    cprintf("%s", line);
}

/*---------------------------------------------------------------
  Read a small unsigned integer (0..100) from keyboard.
  - Uses getch() only (no stdio buffering).
  - Echoes digits, supports backspace, Enter to accept.
  - Returns value 0..100. If file is empty, 0 is fine.
---------------------------------------------------------------*/
static int read_percent_prompt(void) {
    int ch;
    int val = 0;
    int digits = 0;

    /* Prompt on last line; caller should position cursor */
    cprintf("Goto %% (0-100): ");
    while (1) {
        ch = getch();

        /* Enter accepts */
        if (ch == '\r' || ch == '\n') {
            break;
        }

        /* Backspace */
        if (ch == 8) {
            if (digits > 0) {
                /* remove last digit visually and logically */
                putch('\b'); putch(' '); putch('\b');
                val /= 10;
                digits--;
            }
            continue;
        }

        /* Only digits */
        if (ch >= '0' && ch <= '9') {
            if (digits < 3) {
                int d = ch - '0';
                int new_val = val * 10 + d;
                if (new_val <= 100) {
                    val = new_val;
                    digits++;
                    putch((char)ch);
                }
            }
            continue;
        }

        /* Ignore others */
    }

    return val;
}

int main(int argc, char **argv) {
    ReaderState rs;
    char        buffer[PAGE_BUF_SIZE];
    int         bytes_read;
    char        cmd;
    int         new_cap;
    long       *new_hist;

    /* initialize dynamic history */
    rs.history       = NULL;
    rs.hist_count    = 0;
    rs.hist_capacity = 0;
    rs.fp            = NULL;
    rs.file_size     = 0L;
    rs.offset        = 0L;
    rs.progfile_base[0] = '\0';

    load_config();

    if (argc < 2) {
        puts("Usage: 95read <file.txt>");
        return 1;
    }

    io_init(&rs, argv[1]);
    load_progress(&rs);

    while (1) {
        /* grow history on demand */
        if (rs.hist_count == rs.hist_capacity) {
            new_cap = rs.hist_capacity ? rs.hist_capacity * 2 : HIST_INIT_CAP;
            new_hist = (long*)realloc(rs.history, (unsigned)new_cap * sizeof(long));
            if (!new_hist) {
                printf("Out of memory tracking history\n");
                break;
            }
            rs.history        = new_hist;
            rs.hist_capacity  = new_cap;
        }

        /* remember current page start */
        rs.history[rs.hist_count++] = rs.offset;

        /* read and display page from current offset */
        bytes_read = read_page(&rs, buffer);
        if (bytes_read <= 0) {
            /* nothing left to show */
            break;
        }

        display_page(buffer);

        /* show footer with % and key help on last line */
        draw_status(&rs);

        /* wait for a command */
        cmd = ui_get_cmd();

        /* quit */
        if (IS_KEY_QUIT(cmd)) {
            break;
        }
        /* previous page (go back to prior saved offset) */
        else if (IS_KEY_PREV(cmd)) {
            if (rs.hist_count > 1) {
                rs.hist_count -= 2;
                if (rs.hist_count < 0) rs.hist_count = 0;
                rs.offset = rs.history[rs.hist_count];
            } else {
                rs.hist_count = 0;
                rs.offset = 0L;
            }
        }
        /* jump to beginning of file */
        else if (cmd == 'B') {
            rs.offset = 0L;
        }
        /* jump near end of file (best-effort without touching fileio) */
        else if (cmd == 'E') {
            long back = (long)((cfg.screen_lines - 1) * cfg.screen_cols);
            if (rs.file_size > back) {
                rs.offset = rs.file_size - back;
            } else {
                rs.offset = 0L;
            }
        }
        /* prompt and jump to percentage of file */
        else if (cmd == 'G') {
            int row;
            int pct;
            long target;

            /* place prompt on last line */
            row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
            gotoxy(1, row);
            clreol();
            pct = read_percent_prompt();

            /* clamp and compute target offset safely */
            if (pct < 0)   pct = 0;
            if (pct > 100) pct = 100;

            if (rs.file_size > 0L) {
                target = (long)((rs.file_size * (long)pct) / 100L);
                if (target < 0L) target = 0L;
                if (target > rs.file_size) target = rs.file_size;
                rs.offset = target;
            } else {
                rs.offset = 0L;
            }
        }
        /* next page (default) */
        else if (IS_KEY_NEXT(cmd)) {
            /* nothing; offset already advanced by read_page */
        }
        /* ignore unknown keys: reset offset to redraw current page */
        else {
            rs.offset = rs.history[rs.hist_count - 1];
        }
    }

    save_progress(&rs);

    /* clean up */
    if (rs.history) {
        free(rs.history);
        rs.history = NULL;
    }

    return 0;
}