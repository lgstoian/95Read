#include "95read.h"
#include <string.h>   /* for strrchr and memcpy */

void io_init(ReaderState *rs, const char *path) {
    const char *ext = strrchr(path, '.');
    int len = ext ? (int)(ext - path) : (int)strlen(path);

    /* Truncate if base name is too long */
    if (len >= (int)sizeof rs->progfile_base) {
        len = sizeof rs->progfile_base - 1;
    }
    memcpy(rs->progfile_base, path, len);
    rs->progfile_base[len] = '\0';

    rs->fp = fopen(path, "rb");
    if (!rs->fp) {
        printf("Cannot open %s\n", path);
        exit(1);
    }

    /* Determine file size */
    fseek(rs->fp, 0L, SEEK_END);
    rs->file_size = ftell(rs->fp);
    rs->offset    = 0L;
    fseek(rs->fp, 0L, SEEK_SET);
}

int read_page(ReaderState *rs, char *buf) {
    int n;

    if (rs->offset >= rs->file_size) {
        return 0;
    }

    fseek(rs->fp, rs->offset, SEEK_SET);

    /* Read up to PAGE_BUF_SIZE-1 bytes; reserve room for '\0' */
    n = fread(buf, 1, PAGE_BUF_SIZE - 1, rs->fp);
    buf[n] = '\0';

    return n;
}
