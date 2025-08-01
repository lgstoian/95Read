#include "95read.h"
#include <stdio.h>
#include <string.h>

/*
  progfile_base is char[80], PROG_EXT is ".95r" (5 chars),
  so total buffer needs at least 80+5+1 bytes.
*/
#define PROGFILE_BUF_SIZE  (sizeof rs->progfile_base + sizeof PROG_EXT)

void load_progress(ReaderState *rs) {
    char progfile[PROGFILE_BUF_SIZE];
    FILE *pf;
    size_t base_len;

    /* Build "<base>.95r" safely */
    base_len = strlen(rs->progfile_base);
    if (base_len > sizeof(progfile) - sizeof(PROG_EXT) - 1) {
        base_len = sizeof(progfile) - sizeof(PROG_EXT) - 1;
    }
    memcpy(progfile, rs->progfile_base, base_len);
    progfile[base_len] = '\0';
    strcat(progfile, PROG_EXT);

    pf = fopen(progfile, "rb");
    if (pf) {
        /* Read last saved offset */
        if (fread(&rs->offset, sizeof rs->offset, 1, pf) != 1) {
            /* leave offset unchanged on read failure */
        }
        fclose(pf);
    }
}

void save_progress(const ReaderState *rs) {
    char progfile[PROGFILE_BUF_SIZE];
    FILE *pf;
    size_t base_len;

    /* Build "<base>.95r" safely */
    base_len = strlen(rs->progfile_base);
    if (base_len > sizeof(progfile) - sizeof(PROG_EXT) - 1) {
        base_len = sizeof(progfile) - sizeof(PROG_EXT) - 1;
    }
    memcpy(progfile, rs->progfile_base, base_len);
    progfile[base_len] = '\0';
    strcat(progfile, PROG_EXT);

    pf = fopen(progfile, "wb");
    if (pf) {
        fwrite(&rs->offset, sizeof rs->offset, 1, pf);
        fclose(pf);
    }
}
