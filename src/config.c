/* config.c
   HP 95LX-friendly config loader for 95read

   Notes:
   - Pure ANSI C89; avoids modern constructs.
   - No dependency on read95.h to prevent include-order issues.
   - Supports named/hex/decimal/quoted/escaped key values and READ95CFG path override.
   - Supports inline comments (# or ;) anywhere outside quotes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/* global configuration instance */
Config cfg;

/*--------------------------------------------------------------
  Basic string helpers (ASCII only, no <ctype.h>)
--------------------------------------------------------------*/
static int is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static void trim_eol(char *s) {
    char *p;
    if (!s) return;
    p = s + strlen(s);
    while (p > s) {
        --p;
        if (*p == '\n' || *p == '\r') *p = '\0';
        else break;
    }
}

static void ltrim(char *s) {
    char *p = s;
    if (!s) return;
    while (*p && is_space(*p)) ++p;
    if (p != s) {
        size_t n = strlen(p);
        memmove(s, p, n + 1);
    }
}

static void rtrim(char *s) {
    char *p;
    if (!s || !*s) return;
    p = s + strlen(s) - 1;
    while (p >= s && is_space(*p)) {
        *p = '\0';
        if (p == s) break;
        --p;
    }
}

/* Remove inline comments that start with # or ; (outside quotes). */
static void strip_inline_comment(char *s) {
    int in_s = 0;
    int in_d = 0;
    char *p = s;
    if (!s) return;
    while (*p) {
        char c = *p;
        if (!in_d && c == '\'') {
            in_s = !in_s;
        } else if (!in_s && c == '\"') {
            in_d = !in_d;
        } else if (!in_s && !in_d && (c == '#' || c == ';')) {
            *p = '\0';
            break;
        }
        ++p;
    }
}

/* Uppercase ASCII in-place (for keyword comparisons). */
static void to_upper_inplace(char *s) {
    while (s && *s) {
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - ('a' - 'A'));
        ++s;
    }
}

/*--------------------------------------------------------------
  Key token parser
    Accepts:
      - 'X' (single quoted, supports escapes: \n \r \t \b \e \' \")
      - "X" (double quoted, first character used; escapes supported)
      - Named: ESC, SPACE, ENTER, CR, LF, TAB, BS, DEL
      - Hex: 0xNN
      - Decimal: 0..255
      - Single character: A
  Returns 0..255 on success, -1 on failure.
--------------------------------------------------------------*/
static int parse_escape_char(const char *p) {
    switch (*p) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'e': return 27;   /* ESC */
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        default: return (unsigned char)(*p); /* unknown, literal */
    }
}

static int parse_decimal(const char *val) {
    const char *p = val;
    int v = 0;
    if (!val || !*val) return -1;
    while (*p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
        if (v > 255) return -1;
        ++p;
    }
    return v;
}

static int parse_quoted_char(const char *val) {
    char quote;
    const char *p;
    int out;

    if (!val || !*val) return -1;
    quote = *val;
    if (quote != '\'' && quote != '\"') return -1;

    p = val + 1;
    if (*p == '\0') return -1;

    if (*p == '\\') {
        ++p;
        if (*p == '\0') return -1;
        out = parse_escape_char(p);
        ++p;
    } else {
        out = (unsigned char)(*p);
        ++p;
    }

    if (*p != quote) return -1;
    if (*(p + 1) != '\0') return -1; /* nothing after closing quote */

    return out;
}

static int parse_key_token(const char *val) {
    /* Hex 0xNN */
    if (val && val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) {
        unsigned int x = 0U;
        const char *p = val + 2;
        if (*p == '\0') return -1;
        while (*p) {
            char c = *p;
            unsigned int v;
            if (c >= '0' && c <= '9') v = (unsigned int)(c - '0');
            else if (c >= 'a' && c <= 'f') v = (unsigned int)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = (unsigned int)(c - 'A' + 10);
            else return -1;
            x = (x << 4) | v;
            if (x > 255U) return -1;
            ++p;
        }
        return (int)(x & 0xFF);
    }

    /* Decimal 0..255 */
    {
        int d = parse_decimal(val);
        if (d >= 0) return d;
    }

    /* Quoted char: 'X' or "X" (escapes supported) */
    if (val && (val[0] == '\'' || val[0] == '\"')) {
        int ch = parse_quoted_char(val);
        if (ch >= 0) return ch;
    }

    /* Named words and fallback */
    if (val && *val) {
        char buf[16];
        size_t n = strlen(val);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, val, n);
        buf[n] = '\0';
        to_upper_inplace(buf);

        if (strcmp(buf, "ESC") == 0)   return 27;
        if (strcmp(buf, "SPACE") == 0) return ' ';
        if (strcmp(buf, "ENTER") == 0) return 13;  /* CR */
        if (strcmp(buf, "CR") == 0)    return 13;
        if (strcmp(buf, "LF") == 0)    return 10;
        if (strcmp(buf, "TAB") == 0)   return 9;
        if (strcmp(buf, "BS") == 0)    return 8;
        if (strcmp(buf, "DEL") == 0)   return 127;

        /* Fallback: first character, only if single char */
        if (strlen(val) == 1) {
            return (unsigned char)val[0];
        }
    }

    return -1;
}

/*--------------------------------------------------------------
  Acquire config file path:
    1) READ95CFG environment variable, if set and non-empty
    2) Fallback to CONFIG_FILE in current directory
--------------------------------------------------------------*/
static const char* get_config_path(void) {
    const char *env = getenv("READ95CFG");
    if (env && *env) return env;
    return CONFIG_FILE;
}

/* Ensure prog_ext starts with '.', is non-empty, and fits buffer. */
static void set_prog_ext_safe(const char *src) {
    char tmp[MAX_PROG_EXT_LEN + 1];
    size_t i = 0, j = 0;

    if (!src || !*src) {
        /* default ".95r" */
        tmp[0] = '.';
        tmp[1] = '9';
        tmp[2] = '5';
        tmp[3] = 'r';
        tmp[4] = '\0';
    } else {
        const char *p = src;
        size_t len = strlen(src);

        /* strip surrounding quotes if present and length >= 2 */
        if (len >= 2 && ((src[0] == '\'' && src[len - 1] == '\'') ||
                         (src[0] == '"'  && src[len - 1] == '"'))) {
            p = src + 1;
            len -= 2;
        }

        /* prepend '.' if missing */
        if (*p != '.') {
            tmp[i++] = '.';
        }

        for (j = 0; j < len && i < MAX_PROG_EXT_LEN; ++j) {
            char c = p[j];
            if (c == '#' || c == ';') break; /* stop at comment markers */
            tmp[i++] = c;
        }
        tmp[i] = '\0';

        if (i == 0) {
            /* fallback to default */
            tmp[0] = '.';
            tmp[1] = '9';
            tmp[2] = '5';
            tmp[3] = 'r';
            tmp[4] = '\0';
        }
    }

    /* finalize copy */
    strncpy(cfg.prog_ext, tmp, MAX_PROG_EXT_LEN);
    cfg.prog_ext[MAX_PROG_EXT_LEN] = '\0';
}

/* Parse a line into key and value, returns 1 on success, 0 otherwise. */
static int parse_kv(char *line, char *key_out, size_t key_cap, char *val_out, size_t val_cap) {
    char *eq;
    char *k, *v;

    if (!line || !*line) return 0;

    strip_inline_comment(line);
    trim_eol(line);
    ltrim(line);
    rtrim(line);
    if (*line == '\0') return 0;

    /* Try key = value */
    eq = strchr(line, '=');
    if (eq) {
        *eq = '\0';
        k = line;
        v = eq + 1;
    } else {
        /* Try space separated: key value */
        k = line;
        v = line;
        while (*v && !is_space(*v)) ++v;
        if (*v == '\0') return 0;
        *v++ = '\0';
    }

    ltrim(k); rtrim(k);
    ltrim(v); rtrim(v);

    if (*k == '\0' || *v == '\0') return 0;

    /* copy with caps */
    if (key_cap > 0) {
        strncpy(key_out, k, key_cap - 1);
        key_out[key_cap - 1] = '\0';
    }
    if (val_cap > 0) {
        strncpy(val_out, v, val_cap - 1);
        val_out[val_cap - 1] = '\0';
    }

    return 1;
}

void load_config(void)
{
    FILE *fp;
    char line[128];
    char key[32];
    char val[64];

    /* 1) Hard-coded defaults */
    cfg.screen_lines  = 16;                 /* default SCREEN_LINES on 95LX */
    cfg.screen_cols   = 40;                 /* default SCREEN_COLS on 95LX  */
    cfg.tab_width     = DEFAULT_TAB_WIDTH;  /* from config.h */
    cfg.key_next_page = 'N';
    cfg.key_prev_page = 'P';
    cfg.key_quit      = 'C';
    cfg.key_invert    = 'I';
    /* default extension */
    cfg.prog_ext[0] = '\0';
    set_prog_ext_safe(".95r");

    /* 2) Open existing config (env override supported) */
    {
        const char *cfg_path = get_config_path();
        fp = fopen(cfg_path, "r");
        if (fp == NULL) {
            /* Not found: create a new one with comments + defaults */
            FILE *fw = fopen(cfg_path, "w");
            if (fw) {
                fprintf(fw,
                    "# HP95LX Reader Configuration (95read.cfg)\n"
                    "# You can use end-of-line comments starting with # or ;\n"
                    "# Format: key = value   (quotes allowed)\n"
                    "# screen_lines   (4..100)\n"
                    "# screen_cols    (4..200)\n"
                    "# tab_width      (1..16)\n"
                    "# key_next_page  (char like 'N', name: ESC/SPACE/ENTER/TAB/BS/DEL, decimal 27, or hex 0x1B)\n"
                    "# key_prev_page  (same forms as above)\n"
                    "# key_quit       (same forms as above; ESC always quits)\n"
                    "# key_invert     (same forms as above)\n"
                    "# prog_ext       (include dot, e.g., .95r)\n"
                    "# Tip: ESC always quits, regardless of key_quit.\n"
                );
                fprintf(fw, "screen_lines   = %d\n", cfg.screen_lines);
                fprintf(fw, "screen_cols    = %d\n", cfg.screen_cols);
                fprintf(fw, "tab_width      = %d\n", cfg.tab_width);
                fprintf(fw, "key_next_page  = '%c'\n", cfg.key_next_page);
                fprintf(fw, "key_prev_page  = '%c'\n", cfg.key_prev_page);
                fprintf(fw, "key_quit       = '%c'\n", cfg.key_quit);
                fprintf(fw, "key_invert     = '%c'\n", cfg.key_invert);
                fprintf(fw, "prog_ext       = %s\n", cfg.prog_ext);
                fclose(fw);
            }
            return;
        }
    }

    /* 3) Parse existing config */
    while (fgets(line, sizeof(line), fp) != NULL) {
        int iv;

        if (!parse_kv(line, key, sizeof(key), val, sizeof(val)))
            continue;

        if (strcmp(key, "screen_lines") == 0) {
            iv = atoi(val);
            if (iv >= 4 && iv <= 100)
                cfg.screen_lines = iv;
        }
        else if (strcmp(key, "screen_cols") == 0) {
            iv = atoi(val);
            if (iv >= 4 && iv <= 200)
                cfg.screen_cols = iv;
        }
        else if (strcmp(key, "tab_width") == 0) {
            iv = atoi(val);
            if (iv >= 1 && iv <= 16)
                cfg.tab_width = iv;
        }
        else if (strcmp(key, "key_next_page") == 0) {
            iv = parse_key_token(val);
            if (iv >= 0 && iv <= 255)
                cfg.key_next_page = (char)iv;
        }
        else if (strcmp(key, "key_prev_page") == 0) {
            iv = parse_key_token(val);
            if (iv >= 0 && iv <= 255)
                cfg.key_prev_page = (char)iv;
        }
        else if (strcmp(key, "key_quit") == 0) {
            iv = parse_key_token(val);
            if (iv >= 0 && iv <= 255)
                cfg.key_quit = (char)iv;
        }
        else if (strcmp(key, "key_invert") == 0) {
            iv = parse_key_token(val);
            if (iv >= 0 && iv <= 255)
                cfg.key_invert = (char)iv;
        }
        else if (strcmp(key, "prog_ext") == 0) {
            set_prog_ext_safe(val);
        }
        /* Unknown keys are ignored to remain forward-compatible. */
    }

    fclose(fp);

    /* 4) Final clamping for safety (matches HP 95LX constraints) */
    if (cfg.screen_lines < 4)   cfg.screen_lines = 4;
    if (cfg.screen_lines > 100) cfg.screen_lines = 100;
    if (cfg.screen_cols  < 4)   cfg.screen_cols  = 4;
    if (cfg.screen_cols  > 200) cfg.screen_cols  = 200;

    if (cfg.tab_width < 1)  cfg.tab_width = 1;
    if (cfg.tab_width > 16) cfg.tab_width = 16;

    if (cfg.prog_ext[0] == '\0') {
        set_prog_ext_safe(".95r");
    }
}