#ifndef READ95_H
#define READ95_H

/*--------------------------------------------------------------------
  Standard headers
--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------
  Configurable defaults (override in config.h)
--------------------------------------------------------------------*/
#ifndef SCREEN_LINES
  #define SCREEN_LINES   16
#endif

#ifndef SCREEN_COLS
  #define SCREEN_COLS    40
#endif

/* +2 per line for CR+LF, +1 for trailing NUL */
#define PAGE_BUF_SIZE   ((SCREEN_COLS + 2) * SCREEN_LINES + 1)

#ifndef PROG_EXT
  #define PROG_EXT       ".95r"
#endif

#ifndef KEY_NEXT_PAGE
  #define KEY_NEXT_PAGE  'N'
#endif
#ifndef KEY_PREV_PAGE
  #define KEY_PREV_PAGE  'P'
#endif
#ifndef KEY_QUIT
  #define KEY_QUIT       'C'
#endif

/* accept lowercase too ('a'–'A' == 32) */
#define IS_KEY_NEXT(c)  ( (c) == KEY_NEXT_PAGE  || (c) == (KEY_NEXT_PAGE  + 32) )
#define IS_KEY_PREV(c)  ( (c) == KEY_PREV_PAGE  || (c) == (KEY_PREV_PAGE  + 32) )
#define IS_KEY_QUIT(c)  ( (c) == KEY_QUIT       || (c) == (KEY_QUIT       + 32) )

/*--------------------------------------------------------------------
  User overrides
--------------------------------------------------------------------*/
#include "config.h"

/*--------------------------------------------------------------------
  History stack initial size (tweak if you need deeper back/forward)
--------------------------------------------------------------------*/
#define HIST_INIT_CAP   16

/*--------------------------------------------------------------------
  Reader state shared across modules
--------------------------------------------------------------------*/
typedef struct {
    FILE *fp;                   /* open input file */
    long  file_size;            /* total bytes in file */
    long  offset;               /* current page-start offset */

    long *history;              /* array of past offsets */
    int   hist_count;           /* how many saved */
    int   hist_capacity;        /* how many allocated */

    char  progfile_base[80];    /* base filename for .95r file */
} ReaderState;

/*--------------------------------------------------------------------
  Module interfaces
--------------------------------------------------------------------*/

/* fileio.c */
void io_init(ReaderState *rs, const char *path);
int  read_page(ReaderState *rs, char *buf);

/* display.c */
int  display_page(const char *buf);

/* ui.c */
char ui_get_cmd(void);

/* progress.c */
void load_progress(ReaderState *rs);
void save_progress(const ReaderState *rs);

/* config.c */
extern Config cfg;
void load_config(void);

#endif /* READ95_H */
