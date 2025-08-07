#ifndef CONFIG_H
#define CONFIG_H

/*--------------------------------------------------------------------
  Configuration file name (must fit 8.3 file‐system on HP95LX)
--------------------------------------------------------------------*/
#define CONFIG_FILE       "95read.cfg"

/*--------------------------------------------------------------------
  Defaults to use when a setting is missing or invalid in CONFIG_FILE
  (Actual defaults for screen sizes and key codes come from read95.h)
--------------------------------------------------------------------*/
#define DEFAULT_TAB_WIDTH   8

/*--------------------------------------------------------------------
  Maximum length of the save‐file extension (excluding trailing NUL)
  Can be safely overridden in read95.h via PROG_EXT up to this length.
--------------------------------------------------------------------*/
#define MAX_PROG_EXT_LEN   15

/*--------------------------------------------------------------------
  In‐memory configuration structure
--------------------------------------------------------------------*/
typedef struct {
    int  screen_lines;      /* lines per page (overrides SCREEN_LINES) */
    int  screen_cols;       /* cols per line  (overrides SCREEN_COLS) */
    int  tab_width;         /* number of spaces per tab */
    char key_next_page;     /* command key to advance page */
    char key_prev_page;     /* command key to go back */
    char key_quit;          /* command key to exit */
    char prog_ext[MAX_PROG_EXT_LEN + 1];
                            /* user‐specified save‐file extension */
} Config;

/* The global config instance */
extern Config cfg;

/*--------------------------------------------------------------------
  Reads CONFIG_FILE ("95read.cfg") if present.  
  If missing, writes a new one filled with defaults.  
  On load, invalid or out‐of‐range values are clamped to safe bounds:
    4 <= screen_lines <= 100
    4 <= screen_cols  <= 200
    1 <= tab_width     <= 16
  Keys and extension strings are taken as‐is (up to MAX_PROG_EXT_LEN).
--------------------------------------------------------------------*/
void load_config(void);

/*--------------------------------------------------------------------
  Compile‐time sanity checks
--------------------------------------------------------------------*/
#if sizeof(CONFIG_FILE) > 12
  #error "CONFIG_FILE must fit 8.3 (max 8 chars name + dot + 3 chars ext)"
#endif

#ifdef PROG_EXT
enum {
    _cfg_ext_fits = (sizeof(PROG_EXT) <= (MAX_PROG_EXT_LEN + 1)) ? 1 : -1
};
#endif

#endif /* CONFIG_H */
