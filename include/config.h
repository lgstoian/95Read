#ifndef CONFIG_H
#define CONFIG_H

/*--------------------------------------------------------------------
  Configuration file name (must fit 8.3 file-system on HP95LX)
  Used unless overridden by READ95CFG environment variable.
--------------------------------------------------------------------*/
#define CONFIG_FILE       "95read.cfg"

/*--------------------------------------------------------------------
  Defaults to use when a setting is missing or invalid in CONFIG_FILE
  (Actual defaults for screen sizes and key codes come from read95.h)
--------------------------------------------------------------------*/
#define DEFAULT_TAB_WIDTH   8

/*--------------------------------------------------------------------
  Maximum length of the save-file extension (excluding trailing NUL)
  Can be safely overridden in read95.h via PROG_EXT up to this length.
  Keep DOS/95LX-friendly (<= 15 is safe; e.g., ".95r").
--------------------------------------------------------------------*/
#define MAX_PROG_EXT_LEN   15

/*--------------------------------------------------------------------
  Optional behavior flags (no code changes required elsewhere)
  - If set to 1, config loader may skip writing a default config file
    when none exists (useful on read-only media). If 0, behavior is
    unchanged and a default file is created when missing.
--------------------------------------------------------------------*/
#ifndef CFG_SKIP_WRITE_DEFAULT
#define CFG_SKIP_WRITE_DEFAULT 0
#endif

/*--------------------------------------------------------------------
  Minimum and maximum values for configuration clamping
--------------------------------------------------------------------*/
#define MIN_SCREEN_LINES   4
#define MAX_SCREEN_LINES   100
#define MIN_SCREEN_COLS    4
#define MAX_SCREEN_COLS    200
#define MIN_TAB_WIDTH      1
#define MAX_TAB_WIDTH      16

/*--------------------------------------------------------------------
  In-memory configuration structure
--------------------------------------------------------------------*/
typedef struct {
    int  screen_lines;      /* lines per page (overrides SCREEN_LINES) */
    int  screen_cols;       /* cols per line  (overrides SCREEN_COLS) */
    int  tab_width;         /* number of spaces per tab */
    unsigned char key_next_page;     /* command key to advance page */
    unsigned char key_prev_page;     /* command key to go back */
    unsigned char key_quit;          /* command key to exit */
    unsigned char key_invert;        /* command key to invert colors */
    char prog_ext[MAX_PROG_EXT_LEN + 1];
                            /* user-specified save-file extension */
} Config;

/* The global config instance */
extern Config cfg;

/*--------------------------------------------------------------------
  Reads CONFIG_FILE ("95read.cfg") or the file set in READ95CFG.
  If missing, writes a new one filled with defaults (unless
  CFG_SKIP_WRITE_DEFAULT is set to 1).
  On load, invalid or out-of-range values are clamped to safe bounds:
    MIN_SCREEN_LINES <= screen_lines <= MAX_SCREEN_LINES
    MIN_SCREEN_COLS  <= screen_cols  <= MAX_SCREEN_COLS
    MIN_TAB_WIDTH    <= tab_width    <= MAX_TAB_WIDTH
  Keys may be a single character, a name (ESC, SPACE, ENTER, TAB,
  BS, DEL), a quoted character (e.g., 'N' or '\n'), decimal (27),
  or hex (0xNN). Extension strings are length-limited and should
  begin with a '.' (e.g., ".95r").
--------------------------------------------------------------------*/
void load_config(void);

/*--------------------------------------------------------------------
  Compile-time sanity checks (ANSI C89-friendly)
--------------------------------------------------------------------*/
#if sizeof(CONFIG_FILE) > 13
  #error "CONFIG_FILE must fit 8.3 (max 8 chars name + dot + 3 chars ext)"
#endif

/* Ensure MAX_PROG_EXT_LEN is a sane positive value */
#if (MAX_PROG_EXT_LEN <= 0)
  #error "MAX_PROG_EXT_LEN must be > 0"
#endif

/* If PROG_EXT is provided at compile time, ensure it fits the buffer
   (including the terminating NUL). This is a compile-time expression
   that evaluates to 1 or -1; Turbo C will flag -1 as an error. */
#ifdef PROG_EXT
enum {
    _cfg_ext_fits = (sizeof(PROG_EXT) <= (MAX_PROG_EXT_LEN + 1)) ? 1 : -1
};
#endif

#endif /* CONFIG_H */