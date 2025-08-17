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
#ifndef KEY_INVERT
  #define KEY_INVERT     'I'
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
  Helper to uppercase a char (ASCII only)
--------------------------------------------------------------------*/
#define TO_UPPER(ch) (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - ('a' - 'A')) : (ch))

/*--------------------------------------------------------------------
  Key helpers
  - Accept compile-time defaults (KEY_*).
  - Accept runtime-configured keys from cfg where applicable.
  - Input is uppercased for letters in ui_get_cmd, so no lowercase checks needed.
  - For configured keys, uppercase them for comparison to handle mixed cases.
  - Quit also accepts ESC (27) as a convenience.
  - Note: For best compatibility, use uppercase letters in config file.
--------------------------------------------------------------------*/
#define IS_KEY_NEXT(c) ( \
    (TO_UPPER(c) == KEY_NEXT_PAGE) || \
    (TO_UPPER(c) == TO_UPPER(cfg.key_next_page)) \
)

#define IS_KEY_PREV(c) ( \
    (TO_UPPER(c) == KEY_PREV_PAGE) || \
    (TO_UPPER(c) == TO_UPPER(cfg.key_prev_page)) \
)

#define IS_KEY_QUIT(c) ( \
    (TO_UPPER(c) == KEY_QUIT) || \
    (TO_UPPER(c) == TO_UPPER(cfg.key_quit)) || \
    ((c) == 27) \
)

#define IS_KEY_INVERT(c) ( \
    (TO_UPPER(c) == KEY_INVERT) || \
    (TO_UPPER(c) == TO_UPPER(cfg.key_invert)) \
)

/* compile-time only helpers for begin/end/goto */
#define IS_KEY_BEGIN(c) ( ((c) == KEY_BEGIN) )
#define IS_KEY_END(c)  ( ((c) == KEY_END) )
#define IS_KEY_GOTO(c)  ( ((c) == KEY_GOTO) )

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