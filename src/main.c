#include "95read.h"
#include <conio.h>   /* for clrscr(), gotoxy(), putch() */

int main(int argc, char **argv) {
    ReaderState rs;
    char        buffer[PAGE_BUF_SIZE];
    int         bytes_read;
    int         bytes_used;
    char        cmd;

    if (argc < 2) {
        puts("Usage: 95read <file.txt>");
        return 1;
    }

    io_init(&rs, argv[1]);
    load_progress(&rs);

    rs.hist_count = 0;

    while (1) {
        /* Record the start offset of this page */
        if (rs.hist_count < MAX_HISTORY) {
            rs.history[rs.hist_count++] = rs.offset;
        }

        /* Load up to PAGE_BUF_SIZE bytes from file */
        bytes_read = read_page(&rs, buffer);
        if (bytes_read <= 0) {
            break;  /* EOF or error */
        }

        /* Display and get actual byte‐count drawn */
        bytes_used = display_page(buffer);

        /* Wait for user command */
        cmd = ui_get_cmd();
        if (cmd == KEY_QUIT) {
            break;
        }
        else if (cmd == KEY_PREV_PAGE) {
            if (rs.hist_count > 1) {
                /* Step back one page */
                rs.hist_count -= 2;
                rs.offset = rs.history[rs.hist_count];
            } else {
                /* Already at first page */
                rs.hist_count = 0;
                rs.offset = 0;
            }
        }
        else {  /* KEY_NEXT_PAGE */
            rs.offset += bytes_used;
            if (rs.offset > rs.file_size) {
                rs.offset = rs.file_size;
            }
        }
    }

    save_progress(&rs);
    return 0;
}
