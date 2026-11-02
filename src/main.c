/* main.c
   @mod: main
   @breadcrumb: main/entry
   Entry point and main control loop for 95read.
*/

#include "95read.h"
#include <conio.h>   /* clrscr(), gotoxy(), cprintf(), clreol(), getch(), putch(), textattr, window(), gettextinfo */
#include <stdio.h>   /* sprintf, printf, puts */
#include <string.h>  /* strlen, memcpy, strcat */
#include <stdlib.h>  /* realloc, free */

/* @region: video-attrs */
#define ATTR_NORMAL 0x07  /* light grey on black (DOS default) */
#define ATTR_INVERT 0x70  /* black on light grey (reverse video) */

/* @region: video-attrs
   @breadcrumb: main/apply_video_attr */
static void apply_video_attr(int inverted) {
    textattr(inverted ? ATTR_INVERT : ATTR_NORMAL);
}

/* @region: viewport
   @breadcrumb: main/reset_fullscreen_viewport
   Normalize the viewport to the full screen every time we render.
   This prevents lingering conio window/viewport settings from affecting wrapping. */
static void reset_fullscreen_viewport(void) {
    int rows = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
    int cols = (EFF_SCREEN_COLS  > 0) ? EFF_SCREEN_COLS  : SCREEN_COLS;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    window(1, 1, cols, rows);
}

/* @region: viewport
   @breadcrumb: main/print_line_trunc
   Truncate and print a line safely within the visible width (avoids wrap side-effects). */
static void print_line_trunc(int x, int y, const char *s) {
    int cols = (EFF_SCREEN_COLS > 0) ? EFF_SCREEN_COLS : SCREEN_COLS;
    char buf[160];
    size_t n = strlen(s);
    if (cols > 0 && (int)n > cols) n = (size_t)cols;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    gotoxy(x, y);
    clreol();
    cprintf("%s", buf);
}

/*---------------------------------------------------------------
  @region: status
  @breadcrumb: main/draw_status
  Draw a one-line footer with progress, breadcrumb trail and key help.
  Uses last effective screen line to avoid overlapping main text.
  - Adapts to screen width to prevent wrapping on HP 95LX (40 cols).
  - Short form (<=40 cols): "[xxx% NRM] <trail> N/P G B E I T C"
  - Long form (>=48 cols): "[xxx% NRM] <trail> N:Next ... C:Quit"
  The breadcrumb trail is appended when it fits; the legend is then
  dropped so the line never exceeds the visible width.
---------------------------------------------------------------*/
static void draw_status(const ReaderState *rs, int inverted) {
    int row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
    int cols = (EFF_SCREEN_COLS  > 0) ? EFF_SCREEN_COLS  : SCREEN_COLS;
    int percent;
    char line[160];
    char trail[64];
    const char *mode = inverted ? "INV" : "NRM";
    const char *legend;

    /* Compute percentage based on current page start offset */
    if (rs->file_size > 0L) {
        percent = (int)((100L * rs->offset) / rs->file_size);
        if (percent > 100) percent = 100;
        if (percent < 0)   percent = 0;
    } else {
        percent = 0;
    }

    /* Breadcrumb trail of recently visited positions. */
    build_breadcrumb(rs, trail, sizeof(trail));

    if (cols >= 48) {
        legend = "N:Next P:Prev G:Goto J:Line B:Begin E:End F:Find H:Help I:Invert T:Test C:Quit";
    } else {
        legend = "N/P G J B E F H I T C";
    }

    /* Compose: [pct% mode] <trail> <legend>, truncated to the visible width. */
    sprintf(line, "[%3d%% %s]", percent, mode);
    if (trail[0]) {
        strcat(line, " ");
        strcat(line, trail);
    }
    if (legend[0]) {
        size_t len = strlen(line);
        size_t llen = strlen(legend);
        if ((int)(len + 1 + llen) <= cols) {
            strcat(line, " ");
            strcat(line, legend);
        }
    }

    /* Ensure we never exceed the visible width (avoid wrap/overwrite) */
    if (cols > 0 && (int)strlen(line) > cols) {
        line[cols] = '\0';
    }

    /* Show search indicator if active */
    if (rs->search_active && rs->search_len > 0 && cols >= 20) {
        char sline[32];
        sprintf(sline, " [F:%s]", rs->search_str);
        if ((int)strlen(line) + (int)strlen(sline) < cols) {
            strcat(line, sline);
        } else if ((int)strlen(line) < cols - 4) {
            /* truncate search string */
            int slen = cols - (int)strlen(line) - 5;
            if (slen < 1) slen = 1;
            if (slen > MAX_SEARCH_LEN - 1) slen = MAX_SEARCH_LEN - 1;
            line[strlen(line)] = ' ';
            line[strlen(line)] = '[';
            line[strlen(line)] = 'F';
            line[strlen(line)] = ':';
            memcpy(line + strlen(line), rs->search_str, slen);
            line[strlen(line)] = ']';
            line[strlen(line)] = '\0';
        }
    }

    /* Draw on last line */
    gotoxy(1, row);
    clreol();
    cprintf("%s", line);
}

/*---------------------------------------------------------------
  @region: prompt
  @breadcrumb: main/read_percent_prompt
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

/*---------------------------------------------------------------
  @region: prompt
  @breadcrumb: main/read_search_prompt
  Read a search string from keyboard.
  - Uses getch() only (no stdio buffering).
  - Echoes chars, supports backspace, Enter to accept, ESC to cancel.
  - Returns 1 if a search string was entered, 0 if cancelled.
---------------------------------------------------------------*/
static int read_search_prompt(ReaderState *rs) {
    int ch;
    int idx = 0;

    /* Prompt on last line */
    cprintf("Find: ");
    rs->search_str[0] = '\0';
    rs->search_len = 0;

    while (1) {
        ch = getch();

        /* Enter accepts */
        if (ch == '\r' || ch == '\n') {
            if (idx == 0) {
                /* empty search, cancel */
                putch('\r');
                clreol();
                return 0;
            }
            rs->search_str[idx] = '\0';
            rs->search_len = idx;
            rs->search_active = 1;
            rs->search_last_pos = -1L;
            putch('\r');
            clreol();
            return 1;
        }

        /* ESC cancels */
        if (ch == 27) {
            putch('\r');
            clreol();
            return 0;
        }

        /* Backspace */
        if (ch == 8) {
            if (idx > 0) {
                putch('\b'); putch(' '); putch('\b');
                idx--;
            }
            continue;
        }

        /* Accept printable chars */
        if (ch >= 32 && ch < 127 && idx < MAX_SEARCH_LEN - 1) {
            rs->search_str[idx++] = (char)ch;
            putch((char)ch);
        }
        /* Ignore others */
    }
}

/*---------------------------------------------------------------
  @region: prompt
  @breadcrumb: main/read_line_prompt
  Read a small integer line number from keyboard.
  - Uses getch() only (no stdio buffering).
  - Echoes digits, supports backspace, Enter to accept, ESC to cancel.
  - Returns value >=1 or -1 on cancel.
---------------------------------------------------------------*/
static int read_line_prompt(void) {
    int ch;
    long val = 0;
    int digits = 0;

    cprintf("Goto line: ");
    while (1) {
        ch = getch();

        if (ch == '\r' || ch == '\n') {
            if (digits == 0) {
                putch('\r');
                clreol();
                return -1;
            }
            putch('\r');
            clreol();
            if (val < 1) val = 1;
            return (int)val;
        }

        if (ch == 27) {
            putch('\r');
            clreol();
            return -1;
        }

        if (ch == 8) {
            if (digits > 0) {
                putch('\b'); putch(' '); putch('\b');
                val /= 10;
                digits--;
            }
            continue;
        }

        if (ch >= '0' && ch <= '9') {
            if (digits < 6) {
                int d = ch - '0';
                long new_val = val * 10 + d;
                if (new_val <= 1000000) {
                    val = new_val;
                    digits++;
                    putch((char)ch);
                }
            }
            continue;
        }
    }
}

/*---------------------------------------------------------------
  @region: help
  @breadcrumb: main/show_help
  Show help screen.
---------------------------------------------------------------*/
static void show_help(void) {
    struct text_info ti;
    int orig_attr;

    gettextinfo(&ti);
    orig_attr = ti.attribute;

    reset_fullscreen_viewport();
    textattr(ATTR_NORMAL);
    clrscr();

    print_line_trunc(1, 1, "95Read - Help");
    print_line_trunc(1, 2, "");
    print_line_trunc(1, 3, "N/Space/Enter  Next page");
    print_line_trunc(1, 4, "P/BS/PgUp/Left  Prev page");
    print_line_trunc(1, 5, "B/Home          Begin");
    print_line_trunc(1, 6, "E/End           End");
    print_line_trunc(1, 7, "G               Goto %%");
    print_line_trunc(1, 8, "J               Goto line");
    print_line_trunc(1, 9, "F               Find");
    print_line_trunc(1, 10, "N               Find next (after F)");
    print_line_trunc(1, 11, "R               Reload from start");
    print_line_trunc(1, 12, "I               Invert colors");
    print_line_trunc(1, 13, "T               Test screen");
    print_line_trunc(1, 14, "H               This help");
    print_line_trunc(1, 15, "C/Q/ESC         Quit");

    gotoxy(1, 16);
    cprintf("Press any key to return...");

    getch();

    window(ti.winleft, ti.wintop, ti.winright, ti.winbottom);
    textattr(orig_attr);
    apply_video_attr(orig_attr == ATTR_INVERT ? 1 : 0);
}

/* @region: test-screen
   @breadcrumb: main/show_invert_test
   Interactive inversion test screen.
   - Press 'I' inside to toggle inversion live (updates caller's flag).
   - Press any other key to exit.
   - Does not alter file offset/history; caller handles redraw after return.
   - Saves/restores conio viewport and attributes to avoid breaking wrapping.
*/
static void show_invert_test(int *pinverted) {
    struct text_info ti;          /* @note: save current conio state */
    int orig_attr;
    int done = 0;

    /* Snapshot current text info */
    gettextinfo(&ti);
    orig_attr = ti.attribute;

    /* Force a clean full-screen viewport for the test to avoid altering caller's window */
    reset_fullscreen_viewport();

    while (!done) {
        /* Always start test screen with a clear, readable header in normal attr */
        textattr(ATTR_NORMAL);
        clrscr();

        /* Header */
        print_line_trunc(1, 1, (*pinverted ? "Inversion test (current: INV)" : "Inversion test (current: NRM)"));
        print_line_trunc(1, 2, "Press I to toggle, any other key to return");

        /* Normal panel */
        textattr(ATTR_NORMAL);
        print_line_trunc(1, 4, "[Normal] Readability sample:");
        print_line_trunc(1, 5, "The quick brown fox jumps over the lazy dog 1234567890");
        print_line_trunc(1, 6, "ASCII: !@#$%^&*()_+-=[]{};':\",.<>/?|\\");

        /* Inverted panel */
        textattr(ATTR_INVERT);
        print_line_trunc(1, 8, "[Inverted] Readability sample:");
        print_line_trunc(1, 9, "The quick brown fox jumps over the lazy dog 1234567890");
        print_line_trunc(1,10, "ASCII: !@#$%^&*()_+-=[]{};':\",.<>/?|\\");

        /* Footer hint (normal) */
        textattr(ATTR_NORMAL);
        print_line_trunc(1,12, "Tip: Choose the mode that feels easier on your eyes.");

        /* Wait for key */
        {
            int ch = getch();
            if (ch == 'I' || ch == 'i') {
                *pinverted = !*pinverted; /* toggle and redraw loop */
            } else {
                done = 1; /* exit test */
            }
        }
    }

    /* Restore the caller's viewport and attribute */
    window(ti.winleft, ti.wintop, ti.winright, ti.winbottom);
    textattr(orig_attr);

    /* Apply the chosen mode so caller continues in the user's selection */
    apply_video_attr(*pinverted);
}

/*---------------------------------------------------------------
  @region: search
  @breadcrumb: main/find_next
  Search forward for search string from current position.
  Returns 1 if found and sets rs->offset, 0 if not found.
---------------------------------------------------------------*/
static int find_next(ReaderState *rs) {
    char buf[512];
    long pos;
    long start_pos;
    int found = 0;
    long found_pos = -1L;
    size_t slen;
    int i;

    if (!rs->search_active || rs->search_len == 0 || !rs->search_str[0]) {
        return 0;
    }

    slen = (size_t)rs->search_len;
    if (slen == 0) return 0;

    /* Start search from current position */
    start_pos = rs->offset;
    if (rs->search_last_pos >= 0 && rs->search_last_pos == start_pos) {
        /* If we just searched here, start after */
        start_pos = rs->offset + 1;
        if (start_pos > rs->file_size) start_pos = 0;
    }

    /* Search from start_pos to end of file */
    for (pos = start_pos; pos < rs->file_size && !found; pos++) {
        /* Read a window */
        if (fseek(rs->fp, pos, SEEK_SET) != 0) break;
        if (fread(buf, 1, 511, rs->fp) == 0) break;
        buf[511] = '\0';

        /* Simple case-insensitive search */
        for (i = 0; i < 511 - (int)slen && !found; i++) {
            int match = 1;
            int j;
            for (j = 0; j < (int)slen; j++) {
                char c1 = buf[i + j];
                char c2 = rs->search_str[j];
                /* Case insensitive */
                if (c1 >= 'a' && c1 <= 'z') c1 = c1 - ('a' - 'A');
                if (c2 >= 'a' && c2 <= 'z') c2 = c2 - ('a' - 'A');
                if (c1 != c2) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                found = 1;
                found_pos = pos + i;
                break;
            }
        }
    }

    /* If not found from start_pos to end, wrap around from beginning */
    if (!found && start_pos > 0) {
        for (pos = 0; pos < start_pos && !found; pos++) {
            if (fseek(rs->fp, pos, SEEK_SET) != 0) break;
            if (fread(buf, 1, 511, rs->fp) == 0) break;
            buf[511] = '\0';

            for (i = 0; i < 511 - (int)slen && !found; i++) {
                int match = 1;
                int j;
                for (j = 0; j < (int)slen; j++) {
                    char c1 = buf[i + j];
                    char c2 = rs->search_str[j];
                    if (c1 >= 'a' && c1 <= 'z') c1 = c1 - ('a' - 'A');
                    if (c2 >= 'a' && c2 <= 'z') c2 = c2 - ('a' - 'A');
                    if (c1 != c2) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    found = 1;
                    found_pos = pos + i;
                    break;
                }
            }
        }
    }

    if (found && found_pos >= 0) {
        rs->offset = found_pos;
        rs->search_last_pos = found_pos;
        return 1;
    }

    return 0;
}

/* @region: main-loop
   @breadcrumb: main/main */
int main(int argc, char **argv) {
    ReaderState rs;
    char        buffer[PAGE_BUF_SIZE];
    int         bytes_read;
    char        cmd;
    int         new_cap;
    long       *new_hist;
    int         inverted = 0; /* @note: default normal screen (inverted=0) */

    /* initialize dynamic history */
    rs.history       = NULL;
    rs.hist_count    = 0;
    rs.hist_capacity = 0;
    rs.fp            = NULL;
    rs.file_size     = 0L;
    rs.offset        = 0L;
    rs.progfile_base[0] = '\0';
    rs.search_str[0] = '\0';
    rs.search_len    = 0;
    rs.search_active = 0;
    rs.search_last_pos = -1L;

    load_config();

    if (argc < 2) {
        puts("Usage: 95read <file.txt>");
        return 1;
    }

    io_init(&rs, argv[1]);
    load_progress(&rs);

    while (1) {
        /* Record the current page only when it differs from the top of the
           history stack.  This avoids duplicate entries when a key merely
           re-renders the same page (test screen, invert, unknown key). */
        if (rs.hist_count == 0 || rs.offset != rs.history[rs.hist_count - 1]) {
            if (rs.hist_count == rs.hist_capacity) {
                if (rs.hist_capacity >= HIST_MAX_CAP) {
                    /* Drop oldest entry: shift left, reuse last slot. */
                    memmove(rs.history, rs.history + 1,
                            (size_t)(rs.hist_capacity - 1) * sizeof(long));
                    rs.history[rs.hist_count - 1] = rs.offset;
                } else {
                    new_cap = rs.hist_capacity ? rs.hist_capacity * 2 : HIST_INIT_CAP;
                    if (new_cap > HIST_MAX_CAP) new_cap = HIST_MAX_CAP;
                    new_hist = (long*)realloc(rs.history, (unsigned)new_cap * sizeof(long));
                    if (!new_hist) {
                        printf("Out of memory tracking history\n");
                        break;
                    }
                    rs.history       = new_hist;
                    rs.hist_capacity = new_cap;
                    rs.history[rs.hist_count++] = rs.offset;
                }
            } else {
                rs.history[rs.hist_count++] = rs.offset;
            }
        }

        /* read and display page from current offset */
        bytes_read = read_page(&rs, buffer);
        if (bytes_read <= 0) {
            /* nothing left to show */
            break;
        }

        /* @note: Normalize viewport each iteration to prevent residual wrap/viewport issues */
        reset_fullscreen_viewport();
        apply_video_attr(inverted);             /* @note: Use helper; default is normal unless toggled */
        clrscr();
        display_page(buffer);

        /* show footer with % and key help on last line */
        draw_status(&rs, inverted);

        /* wait for a command */
        cmd = ui_get_cmd();

        /* quit */
        if (IS_KEY_QUIT(cmd) || cmd == 'Q' || cmd == 'q') {
            break;
        }
        /* previous page (go back to prior saved offset) */
        else if (IS_KEY_PREV(cmd)) {
            /* Pop the current page and move to the previous one. */
            if (rs.hist_count > 1) {
                rs.hist_count--;
                rs.offset = rs.history[rs.hist_count - 1];
            } else {
                rs.hist_count = 0;
                rs.offset = 0L;
            }
        }
        /* jump to beginning of file */
        else if (cmd == 'B' || cmd == 'b') {
            rs.offset = 0L;
        }
        /* jump near end of file (best-effort without touching fileio) */
        else if (cmd == 'E' || cmd == 'e') {
            long back = (long)((EFF_SCREEN_LINES - 1) * EFF_SCREEN_COLS);
            if (rs.file_size > back) {
                rs.offset = rs.file_size - back;
            } else {
                rs.offset = 0L;
            }
        }
        /* prompt and jump to percentage of file */
        else if (cmd == 'G' || cmd == 'g') {
            int row;
            int pct;
            long target;

            /* Ensure fullscreen viewport for absolute positioning of the prompt */
            reset_fullscreen_viewport();

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
        /* find/search */
        else if (cmd == 'F' || cmd == 'f') {
            int row;
            int found;

            /* Ensure fullscreen viewport for absolute positioning of the prompt */
            reset_fullscreen_viewport();

            /* place prompt on last line */
            row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
            gotoxy(1, row);
            clreol();
            if (read_search_prompt(&rs)) {
                found = find_next(&rs);
                if (!found) {
                    /* Show not found message briefly */
                    gotoxy(1, row);
                    clreol();
                    cprintf("Not found: %s", rs.search_str);
                    /* small delay */
                    { int k = 0; for (k = 0; k < 1000; k++); }
                }
            }
        }
        /* goto line */
        else if (cmd == 'J' || cmd == 'j') {
            int row;
            int line_num;
            long line = 0;
            long pos = 0;
            int ch;

            reset_fullscreen_viewport();
            row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
            gotoxy(1, row);
            clreol();
            line_num = read_line_prompt();
            if (line_num > 0) {
                fseek(rs.fp, 0L, SEEK_SET);
                line = 1;
                pos = 0;
                while (line < line_num && pos < rs.file_size) {
                    ch = fgetc(rs.fp);
                    if (ch == EOF) break;
                    if (ch == '\n') {
                        line++;
                    }
                    pos++;
                }
                if (pos >= rs.file_size) pos = 0;
                rs.offset = pos;
                if (rs.fp) fseek(rs.fp, rs.offset, SEEK_SET);
            }
        }
        /* reload from beginning */
        else if (cmd == 'R' || cmd == 'r') {
            rs.offset = 0L;
        }
        /* help */
        else if (cmd == 'H' || cmd == 'h') {
            show_help();
        }
        /* @note: Add test screen command (non-destructive) */
        else if (cmd == 'T' || cmd == 't') {
            /* Show the test screen; allow toggling inversion inside it.
               Do not alter history or offset; we simply re-render the same page. */
            show_invert_test(&inverted);
        }
        /* invert colors */
        else if (cmd == 'I' || cmd == 'i') {
            inverted = !inverted;
            /* Re-display current page under the new attribute */
            if (rs.hist_count > 0) {
                rs.offset = rs.history[rs.hist_count - 1];
            }
        }
        /* next page or next search result */
        else if (IS_KEY_NEXT(cmd)) {
            if (rs.search_active && rs.search_len > 0) {
                int found = find_next(&rs);
                if (!found) {
                    int row = (EFF_SCREEN_LINES > 0) ? EFF_SCREEN_LINES : SCREEN_LINES;
                    reset_fullscreen_viewport();
                    gotoxy(1, row);
                    clreol();
                    cprintf("Not found: %s", rs.search_str);
                    { int k = 0; for (k = 0; k < 500; k++); }
                }
            }
            /* else nothing; offset already advanced by read_page */
        }
        /* ignore unknown keys: reset offset to redraw current page */
        else {
            if (rs.hist_count > 0) {
                rs.offset = rs.history[rs.hist_count - 1];
            }
        }
    }

    save_progress(&rs);

    /* clean up */
    io_done(&rs);
    if (rs.history) {
        free(rs.history);
        rs.history = NULL;
    }

    return 0;
}
