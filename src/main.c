#include "95read.h"
#include <conio.h>   /* for clrscr(), gotoxy(), putch() */

int main(int argc, char **argv) {
    ReaderState rs;
    char        buffer[PAGE_BUF_SIZE];
    int         bytes_read;
    int         bytes_used;
    char        cmd;
    int         new_cap;
    long       *new_hist;

    /* initialize dynamic history */
    rs.history      = NULL;
    rs.hist_count   = 0;
    rs.hist_capacity= 0;

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
            new_cap = rs.hist_capacity ? rs.hist_capacity * 2 : 16;
            new_hist = (long*)realloc(rs.history, new_cap * sizeof(long));
            if (!new_hist) {
                printf("Out of memory tracking history\n");
                break;
            }
            rs.history       = new_hist;
            rs.hist_capacity= new_cap;
        }
        rs.history[rs.hist_count++] = rs.offset;

        bytes_read = read_page(&rs, buffer);
        if (bytes_read <= 0) {
            break;
        }

        bytes_used = display_page(buffer);

        cmd = ui_get_cmd();
        if (cmd == cfg.key_quit) {
            break;
        }
        else if (cmd == cfg.key_prev_page) {
            if (rs.hist_count > 1) {
                rs.hist_count -= 2;
                rs.offset = rs.history[rs.hist_count];
            } else {
                rs.hist_count = 0;
                rs.offset = 0;
            }
        }
        else {
            rs.offset += bytes_used;
            if (rs.offset > rs.file_size) {
                rs.offset = rs.file_size;
            }
        }
    }

    save_progress(&rs);

    /* clean up */
    if (rs.history) {
        free(rs.history);
    }
    return 0;
}
