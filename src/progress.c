#include "95read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  Progress file handling (HP 95LX / Turbo C 2.01 friendly)

  Compatibility:
    - Loads the legacy format:
        [int count][long offsets[count]]
    - Saves the new, checksum-protected format:
        "95R1" (4 bytes)
        uint16 count
        long offsets[count]
        uint16 checksum (sum of all bytes of the offsets array)

  Features and improvements:
    - Backward compatible loader (auto-detects legacy vs. new format).
    - Atomic save via temporary file with ".~r" extension, then rename.
    - File size kept small by:
        * Removing consecutive duplicate offsets when saving.
        * Capping the number of saved entries (last MAX_SAVE_ENTRIES).
    - Robust filename construction with fallback to PROG_EXT.
    - Safe resume:
        * If the last offset is past EOF, clamp it.
        * If resuming exactly at EOF, back up by one page.
*/

/* Maximum number of entries to save to keep progress files small on 95LX. */
#ifndef MAX_SAVE_ENTRIES
#define MAX_SAVE_ENTRIES 512
#endif

/* New progress file signature (v1). */
#define PROG_SIG "95R1"

/* Local helper: compute a simple 16-bit checksum over a byte buffer. */
static unsigned short csum16(const unsigned char *buf, size_t n) {
    unsigned long s = 0;
    size_t i;
    for (i = 0; i < n; ++i) s += buf[i];
    /* fold to 16 bits */
    s = (s & 0xFFFFUL) + (s >> 16);
    s = (s & 0xFFFFUL) + (s >> 16);
    return (unsigned short)s;
}

/* Local helper: safe ext string (fallback to PROG_EXT if cfg.prog_ext is empty). */
static const char *effective_ext(void) {
    if (cfg.prog_ext && cfg.prog_ext[0]) return cfg.prog_ext;
    return PROG_EXT;
}

/* Local helper: build "<progfile_base><ext>" into dst with capacity cap. */
static void build_prog_path(char *dst, size_t cap, const char *base, const char *ext) {
    size_t i = 0;
    if (cap == 0) return;

    /* copy base */
    if (base) {
        while (base[i] && i + 1 < cap) {
            dst[i] = base[i];
            ++i;
        }
    }

    /* append ext */
    if (ext) {
        size_t j = 0;
        while (ext[j] && i + 1 < cap) {
            dst[i++] = ext[j++];
        }
    }

    dst[i] = '\0';
}

/* Local helper: build temp path with extension ".~r" (8.3 friendly). */
static void build_temp_path(char *dst, size_t cap, const char *base) {
    build_prog_path(dst, cap, base, ".~r");
}

/* Local helper: clamp and normalize loaded history against current file size. */
static void normalize_loaded_history(ReaderState *rs) {
    int i, w = 0;

    /* Clamp entries to [0, file_size]. */
    for (i = 0; i < rs->hist_count; ++i) {
        long off = rs->history[i];
        if (off < 0L) off = 0L;
        if (off > rs->file_size) off = rs->file_size;
        rs->history[w++] = off;
    }
    rs->hist_count = w;
    rs->hist_capacity = rs->hist_count;

    if (rs->hist_count > 0) {
        rs->offset = rs->history[rs->hist_count - 1];
        /* If resuming at exact EOF, back up by one page if possible. */
        if (rs->offset == rs->file_size && rs->file_size > 0L) {
            long back = (long)PAGE_BUF_SIZE;
            if (rs->file_size > back) rs->offset = rs->file_size - back;
            else rs->offset = 0L;
        }
    } else {
        rs->offset = 0L;
    }
}

/* Try to load new format ("95R1"). Returns 1 on success, 0 on mismatch/failure. */
static int try_load_new_format(FILE *pf, ReaderState *rs, long fsize) {
    unsigned char sig[4];
    unsigned short count;
    unsigned short file_csum, calc_csum;

    /* Minimum: sig(4)+count(2)+csum(2) */
    if (fsize < 8L) return 0;

    if (fread(sig, 1, 4, pf) != 4) return 0;
    if (memcmp(sig, PROG_SIG, 4) != 0) return 0;

    if (fread(&count, sizeof(count), 1, pf) != 1) return 0;

    /* Expected exact file size: 4 + 2 + count*sizeof(long) + 2 */
    {
        long expected = 4L + 2L + (long)count * (long)sizeof(long) + 2L;
        if (expected != fsize) return 0;
    }

    if (count == 0) {
        if (fread(&file_csum, sizeof(file_csum), 1, pf) != 1) return 0;
        rs->history = NULL;
        rs->hist_count = 0;
        rs->hist_capacity = 0;
        rs->offset = 0L;
        return 1;
    }

    /* Sanity cap avoids huge allocations. */
    if (count > 8192) return 0;

    rs->history = (long*)malloc((size_t)count * sizeof(long));
    if (!rs->history) return 0;

    if (fread(rs->history, sizeof(long), count, pf) != (size_t)count) {
        free(rs->history);
        rs->history = NULL;
        return 0;
    }

    if (fread(&file_csum, sizeof(file_csum), 1, pf) != 1) {
        free(rs->history);
        rs->history = NULL;
        return 0;
    }

    calc_csum = csum16((const unsigned char*)rs->history, (size_t)count * sizeof(long));
    if (calc_csum != file_csum) {
        free(rs->history);
        rs->history = NULL;
        return 0;
    }

    rs->hist_count = (int)count;
    rs->hist_capacity = (int)count;

    normalize_loaded_history(rs);
    return 1;
}

/* Try to load legacy format ([int count][long offsets[count]]). Returns 1 on success. */
static int try_load_legacy_format(FILE *pf, ReaderState *rs, long fsize) {
    int count;
    long expected;

    if (fsize < (long)sizeof(int)) return 0;

    rewind(pf);
    if (fread(&count, sizeof(count), 1, pf) != 1 || count <= 0) return 0;

    if (count > 4096) return 0;

    expected = (long)sizeof(int) + (long)count * (long)sizeof(long);
    if (expected != fsize) return 0;

    rs->history = (long*)malloc((size_t)count * sizeof(long));
    if (!rs->history) return 0;

    if (fread(rs->history, sizeof(long), (size_t)count, pf) != (size_t)count) {
        free(rs->history);
        rs->history = NULL;
        return 0;
    }

    rs->hist_count = count;
    rs->hist_capacity = count;

    normalize_loaded_history(rs);
    return 1;
}

void load_progress(ReaderState *rs) {
    char progfile[128];
    const char *ext;
    FILE *pf;
    long fsize;
    int loaded = 0;

    /* Initialize defaults. */
    rs->history       = NULL;
    rs->hist_count    = 0;
    rs->hist_capacity = 0;
    rs->offset        = 0L;

    /* Build "<progfile_base><ext>" safely. */
    ext = effective_ext();
    build_prog_path(progfile, sizeof(progfile), rs->progfile_base, ext);

    pf = fopen(progfile, "rb");
    if (!pf) return; /* No progress file; start fresh. */

    /* Get file size for structural validation. */
    if (fseek(pf, 0L, SEEK_END) == 0) {
        fsize = ftell(pf);
        if (fsize < 0L) fsize = 0L;
        rewind(pf);
    } else {
        fsize = 0L;
        rewind(pf);
    }

    /* Try new format first, then legacy. */
    if (fsize >= 8L) loaded = try_load_new_format(pf, rs, fsize);
    if (!loaded)     loaded = try_load_legacy_format(pf, rs, fsize);

    fclose(pf);

    if (!loaded) {
        if (rs->history) {
            free(rs->history);
            rs->history = NULL;
        }
        rs->hist_count    = 0;
        rs->hist_capacity = 0;
        rs->offset        = 0L;
    }
}

/* Write progress to a temp file, then atomically replace the target. */
static void write_progress_atomic(const char *final_path, const ReaderState *rs) {
    char tmpfile[128];
    FILE *pf;
    unsigned short count_to_write = 0;
    int i;
    long last = -1L;

    /* Build temp file path using ".~r" extension. */
    build_temp_path(tmpfile, sizeof(tmpfile), rs->progfile_base);

    /* Open temp for binary write. */
    pf = fopen(tmpfile, "wb");
    if (!pf) {
        /* Fallback: write directly to final if temp fails. */
        pf = fopen(final_path, "wb");
        if (!pf) return;
    }

    /* First pass: compute count_to_write (dedupe consecutive and cap last N). */
    {
        int start = 0;
        int end = rs->hist_count;
        int kept = 0;

        if (end - start > MAX_SAVE_ENTRIES) start = end - MAX_SAVE_ENTRIES;

        last = -1L;
        for (i = start; i < end; ++i) {
            long v = rs->history ? rs->history[i] : 0L;
            if (i == start || v != last) {
                ++kept;
                last = v;
            }
        }
        count_to_write = (unsigned short)kept;
    }

    /* Write header: signature + count. */
    fwrite(PROG_SIG, 1, 4, pf);
    fwrite(&count_to_write, sizeof(count_to_write), 1, pf);

    /* Second pass: write filtered offsets and compute checksum. */
    {
        unsigned short csum = 0;
        int start = 0;
        int end = rs->hist_count;

        if (end - start > MAX_SAVE_ENTRIES) start = end - MAX_SAVE_ENTRIES;

        last = -1L;
        for (i = start; i < end; ++i) {
            long v = rs->history ? rs->history[i] : 0L;
            if (i == start || v != last) {
                fwrite(&v, sizeof(long), 1, pf);
                csum = (unsigned short)(csum + csum16((const unsigned char*)&v, sizeof(long)));
                last = v;
            }
        }

        /* Trailer checksum. */
        fwrite(&csum, sizeof(csum), 1, pf);
    }

    fclose(pf);

    /* If we successfully wrote to temp, atomically replace final. */
    if (strcmp(tmpfile, final_path) != 0) {
        remove(final_path);
        if (rename(tmpfile, final_path) != 0) {
            /* If rename fails, remove temp to avoid litter. */
            remove(tmpfile);
        }
    }
}

void save_progress(const ReaderState *rs) {
    char progfile[128];
    const char *ext = effective_ext();

    /* Build "<progfile_base><ext>" safely. */
    build_prog_path(progfile, sizeof(progfile), rs->progfile_base, ext);

    /* No history? Still write a minimal header so the file exists. */
    if (!rs->history || rs->hist_count <= 0) {
        FILE *pf = fopen(progfile, "wb");
        unsigned short zero = 0;
        unsigned short csum = 0;
        if (!pf) return;
        fwrite(PROG_SIG, 1, 4, pf);
        fwrite(&zero, sizeof(zero), 1, pf);
        fwrite(&csum, sizeof(csum), 1, pf);
        fclose(pf);
        return;
    }

    write_progress_atomic(progfile, rs);
}
