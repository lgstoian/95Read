#include "95read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  Now saves entire history dynamically—no fixed MAX_HISTORY.
  File format: [int count][long offsets[count]].
*/

void load_progress(ReaderState *rs) {
    char   progfile[sizeof rs->progfile_base + sizeof cfg.prog_ext];
    FILE  *pf;
    int    count;
    size_t base_len;

    /* initialize defaults */
    rs->history       = NULL;
    rs->hist_count    = 0;
    rs->hist_capacity = 0;
    rs->offset        = 0L;

    /* build "<progfile_base><prog_ext>" */
    base_len = strlen(rs->progfile_base);
    if (base_len >= sizeof progfile - sizeof cfg.prog_ext)
        base_len = sizeof progfile - sizeof cfg.prog_ext - 1;

    memcpy(progfile, rs->progfile_base, base_len);
    progfile[base_len] = '\0';
    strcat(progfile, cfg.prog_ext);

    pf = fopen(progfile, "rb");
    if (!pf) {
        return;     /* no progress file, start fresh */
    }

    /* read saved entry count */
    if (fread(&count, sizeof count, 1, pf) != 1 || count <= 0) {
        fclose(pf);
        return;
    }

    /* sanity‐check count to avoid huge allocations */
    if (count > 4096) {
        /* too many entries—probably corrupted */
        fclose(pf);
        return;
    }

    /* allocate history array */
    rs->history = (long*)malloc(count * sizeof(long));
    if (!rs->history) {
        printf("Cannot allocate history (%d entries)\n", count);
        fclose(pf);
        return;
    }

    /* read offsets */
    if (fread(rs->history, sizeof(long), count, pf) != (size_t)count) {
        free(rs->history);
        rs->history = NULL;
        fclose(pf);
        return;
    }
    fclose(pf);

    /* restore counts and last offset */
    rs->hist_count    = count;
    rs->hist_capacity = count;
    rs->offset        = rs->history[count - 1];

    /* clamp offset: if it’s past EOF, discard progress */
    if (rs->offset >= rs->file_size) {
        free(rs->history);
        rs->history       = NULL;
        rs->hist_count    = 0;
        rs->hist_capacity = 0;
        rs->offset        = 0L;
    }
}

void save_progress(const ReaderState *rs) {
    char   progfile[sizeof rs->progfile_base + sizeof cfg.prog_ext];
    FILE  *pf;
    size_t base_len;

    /* build "<progfile_base><prog_ext>" */
    base_len = strlen(rs->progfile_base);
    if (base_len >= sizeof progfile - sizeof cfg.prog_ext)
        base_len = sizeof progfile - sizeof cfg.prog_ext - 1;

    memcpy(progfile, rs->progfile_base, base_len);
    progfile[base_len] = '\0';
    strcat(progfile, cfg.prog_ext);

    pf = fopen(progfile, "wb");
    if (!pf) {
        return;     /* cannot write progress—ignore */
    }

    /* write number of entries */
    fwrite(&rs->hist_count, sizeof rs->hist_count, 1, pf);

    /* write history array */
    if (rs->hist_count > 0 && rs->history) {
        fwrite(rs->history, sizeof(long), rs->hist_count, pf);
    }
    fclose(pf);
}
