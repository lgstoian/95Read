/* src/config.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/* global configuration instance */
Config cfg;

void load_config(void) {
    FILE *fp;
    char line[80];

    /* 1) Hard‐coded defaults */
    cfg.screen_lines  = 16;     /* default SCREEN_LINES */
    cfg.screen_cols   = 40;     /* default SCREEN_COLS  */
    cfg.tab_width     = DEFAULT_TAB_WIDTH;
    cfg.key_next_page = 'N';
    cfg.key_prev_page = 'P';
    cfg.key_quit      = 'C';
    strncpy(cfg.prog_ext, ".95r", MAX_PROG_EXT_LEN);
    cfg.prog_ext[MAX_PROG_EXT_LEN] = '\0';

    /* 2) Try opening existing config for read */
    fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        /* not found—create a new one with comments + defaults */
        FILE *fw = fopen(CONFIG_FILE, "w");
        if (fw) {
            fprintf(fw,
                "# HP95LX Reader Configuration\n"
                "# screen_lines  (4..100)\n"
                "# screen_cols   (4..200)\n"
                "# tab_width     (1..16)\n"
                "# key_next_page (single char)\n"
                "# key_prev_page (single char)\n"
                "# key_quit      (single char)\n"
                "# prog_ext      (include dot, up to %d chars)\n\n",
                MAX_PROG_EXT_LEN
            );
            fprintf(fw, "screen_lines   = %d\n", cfg.screen_lines);
            fprintf(fw, "screen_cols    = %d\n", cfg.screen_cols);
            fprintf(fw, "tab_width      = %d\n", cfg.tab_width);
            fprintf(fw, "key_next_page  = %c\n", cfg.key_next_page);
            fprintf(fw, "key_prev_page  = %c\n", cfg.key_prev_page);
            fprintf(fw, "key_quit       = %c\n", cfg.key_quit);
            fprintf(fw, "prog_ext       = %s\n", cfg.prog_ext);
            fclose(fw);
        }
        return;
    }

    /* 3) Parse existing config */
    while (fgets(line, sizeof(line), fp) != NULL) {
        char key[24], val[24];
        int  iv;
        char c;

        /* skip comments or blank lines */
        if (line[0] == '#' || line[0] == ';' || line[0] == '\n')
            continue;

        /* expect "key = value" */
        if (sscanf(line, "%23s = %23s", key, val) != 2)
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
            c = val[0];
            if (c >= 32 && c <= 126)
                cfg.key_next_page = c;
        }
        else if (strcmp(key, "key_prev_page") == 0) {
            c = val[0];
            if (c >= 32 && c <= 126)
                cfg.key_prev_page = c;
        }
        else if (strcmp(key, "key_quit") == 0) {
            c = val[0];
            if (c >= 32 && c <= 126)
                cfg.key_quit = c;
        }
        else if (strcmp(key, "prog_ext") == 0) {
            strncpy(cfg.prog_ext, val, MAX_PROG_EXT_LEN);
            cfg.prog_ext[MAX_PROG_EXT_LEN] = '\0';
        }
    }

    fclose(fp);
}
