#include "95read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* for strrchr(), memcpy(), strlen() */

void io_init(ReaderState *rs, const char *path) {
    const char *ext = strrchr(path, '.');
    int len, max;

    /* compute filename base (without extension) */
    if (ext)
        len = (int)(ext - path);
    else
        len = (int)strlen(path);

    /* leave room for terminating null in progfile_base[] */
    max = sizeof(rs->progfile_base) - 1;
    if (len > max)
        len = max;

    memcpy(rs->progfile_base, path, len);
    rs->progfile_base[len] = '\0';

    /* open file in binary mode */
    rs->fp = fopen(path, "rb");
    if (rs->fp == NULL) {
        printf("Cannot open \"%s\"\n", path);
        exit(1);
    }

    /* determine file size */
    if (fseek(rs->fp, 0L, SEEK_END) != 0) {
        printf("Seek error on \"%s\"\n", path);
        fclose(rs->fp);
        exit(1);
    }
    rs->file_size = ftell(rs->fp);
    if (rs->file_size < 0) {
        printf("ftell error on \"%s\"\n", path);
        fclose(rs->fp);
        exit(1);
    }
    fseek(rs->fp, 0L, SEEK_SET);

    /* warn if file is empty */
    if (rs->file_size == 0) {
        printf("Warning: \"%s\" is empty\n", path);
    }
}

int read_page(ReaderState *rs, char *buf) {
    int n;

    /* nothing left to read? */
    if (rs->offset >= rs->file_size)
        return 0;

    /* position to last offset */
    if (fseek(rs->fp, rs->offset, SEEK_SET) != 0) {
        printf("Seek error at offset %ld\n", rs->offset);
        return 0;
    }

    /* read into buffer, leave room for terminating NUL */
    n = (int)fread(buf, 1, PAGE_BUF_SIZE - 1, rs->fp);
    buf[n] = '\0';

    /* report any read errors */
    if (n < PAGE_BUF_SIZE - 1 && ferror(rs->fp)) {
        printf("Read error at offset %ld\n", rs->offset);
    }

    return n;
}
