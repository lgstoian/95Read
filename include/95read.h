#ifndef READ95_H
#define READ95_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* now using memcpy and strlen */

/* Screen dimensions */
#define SCREEN_LINES   16
#define SCREEN_COLS    40

/* Maximum bytes per page buffer (include line breaks) */
#define PAGE_BUF_SIZE  ((SCREEN_COLS + 2) * SCREEN_LINES)

/* Save-file extension */
#define PROG_EXT       ".95r"

/* Key commands */
#define KEY_NEXT_PAGE  'N'
#define KEY_PREV_PAGE  'P'
#define KEY_QUIT       'C'

/* How many pages of history to remember */
#define MAX_HISTORY    1024    /* was 128—now can remember over 1000 pages */

/* Reader state stored across modules */
typedef struct {
    FILE *fp;                   /* open text file */
    long  file_size;            /* total bytes in file */
    long  offset;               /* current byte offset in file */

    long  history[MAX_HISTORY]; /* page-start offsets */
    int   hist_count;           /* how many entries are valid */

    /* Base name for progress file (“foo” → “foo.95r”) */
    char  progfile_base[80];
} ReaderState;

/* fileio.c */
void io_init(ReaderState *rs, const char *path);
int  read_page(ReaderState *rs, char *buf);

/* display.c */
/* Returns how many bytes were consumed (displayed) from buf */
int  display_page(const char *buf);

/* ui.c */
char ui_get_cmd(void);

/* progress.c */
void save_progress(const ReaderState *rs);
void load_progress(ReaderState *rs);

#endif /* READ95_H */
