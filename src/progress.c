#include "95read.h"
#include <stdio.h>
#include <string.h>

/*
  progfile_base is char[80], PROG_EXT is ".95r" (sizeof == 5),
  so we need 80 + 5 = 85 bytes in total.
*/
#define PROGFILE_BUF_SIZE  (80 + sizeof PROG_EXT)

void load_progress(ReaderState *rs) {
    char progfile[PROGFILE_BUF_SIZE];
    FILE *pf;

    /* Build "<base>.95r" */
    strcpy(progfile, rs->progfile_base);
    strcat(progfile, PROG_EXT);

    pf = fopen(progfile, "rb");
    if (pf) {
        /* Read one offset; if it fails, leave rs->offset unchanged */
        fread(&rs->offset, sizeof rs->offset, 1, pf);
        fclose(pf);
    }
}

void save_progress(const ReaderState *rs) {
    char progfile[PROGFILE_BUF_SIZE];
    FILE *pf;

    strcpy(progfile, rs->progfile_base);
    strcat(progfile, PROG_EXT);

    pf = fopen(progfile, "wb");
    if (pf) {
        fwrite(&rs->offset, sizeof rs->offset, 1, pf);
        fclose(pf);
    }
}
