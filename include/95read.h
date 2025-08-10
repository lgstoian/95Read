#ifndef READ95_H
#define READ95_H

/*--------------------------------------------------------------------
  Standard headers
--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------
  Configurable defaults (override at runtime via 95read.cfg)
  Note: These defaults also define compile-time buffer sizes.
--------------------------------------------------------------------*/
#ifndef SCREEN_LINES
  #define SCREEN_LINES   16
#endif

#ifndef SCREEN_COLS
  #define SCREEN_COLS    40
#endif

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

/* Additional navigation keys (compile-time only; no cfg dependency) */
#ifndef KEY_BEGIN
  #define KEY_BEGIN      'B'   /* jump to start of file */
#endif
#ifndef KEY_END
  #define KEY_END        'E'   /* jump near end of file */
#endif
#ifndef KEY_GOTO
  #define KEY_GOTO       'G'   /* prompt for percent and jump */
#endif

/*--------------------------------------------------------------------
  User overrides and config structure
--------------------------------------------------------------------*/
#include "config.h"

/*--------------------------------------------------------------------
  Static page buffer size (compile-time). Kept small for HP 95LX.
  +2 per line for CR+LF, +1 for trailing NUL.
--------------------------------------------------------------------*/
#define PAGE_BUF_SIZE   ((SCREEN_COLS + 2) * SCREEN_LINES + 1)

/*--------------------------------------------------------------------
  Key helpers
  - Accept compile-time defaults (KEY_*).
  - Accept runtime-configured keys from cfg where applicable.
  - Accept lowercase too ('a' - 'A' == 32).
  - Quit also accepts ESC (27) as a convenience.
--------------------------------------------------------------------*/
#define IS_KEY_NEXT(c) ( \
    ((c) == KEY_NEXT_PAGE) || ((c) == (KEY_NEXT_PAGE + 32)) || \
    ((c) == cfg.key_next_page) || ((c) == (cfg.key_next_page + 32)) \
)

#define IS_KEY_PREV(c) ( \
    ((c) == KEY_PREV_PAGE) || ((c) == (KEY_PREV_PAGE + 32)) || \
    ((c) == cfg.key_prev_page) || ((c) == (cfg.key_prev_page + 32)) \
)

#define IS_KEY_QUIT(c) ( \
    ((c) == KEY_QUIT) || ((c) == (KEY_QUIT + 32)) || \
    ((c) == cfg.key_quit) || ((c) == (cfg.key_quit + 32)) || \
    ((c) == 27) \
)

/* compile-time only helpers for begin/end/goto */
#define IS_KEY_BEGIN(c) ( ((c) == KEY_BEGIN) || ((c) == (KEY_BEGIN + 32)) )
#define IS_KEY_ENDK(c)  ( ((c) == KEY_END)   || ((c) == (KEY_END + 32)) )
#define IS_KEY_GOTO(c)  ( ((c) == KEY_GOTO)  || ((c) == (KEY_GOTO + 32)) )

/*--------------------------------------------------------------------
  Effective dimensions if needed by other modules:
  These let code honor runtime settings but still fit compile buffers.
--------------------------------------------------------------------*/
#define EFF_SCREEN_LINES ( (cfg.screen_lines <= SCREEN_LINES) ? cfg.screen_lines : SCREEN_LINES )
#define EFF_SCREEN_COLS  ( (cfg.screen_cols  <= SCREEN_COLS ) ? cfg.screen_cols  : SCREEN_COLS  )

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
