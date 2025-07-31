#include "95read.h"
#include <string.h>

void io_init(ReaderState *rs, const char *path) {
    const char *ext = strrchr(path, '.');
    int len = ext
              ? (int)(ext - path)
              : (int)strlen(path);

    /* Truncate if base name is too long */
    if (len >= (int)sizeof rs->progfile_base)
        len = sizeof rs->progfile_base - 1;

    memcpy(rs->progfile_base, path, len);
    rs->progfile_base[len] = '\0';

    rs->fp = fopen(path, "rb");
    if (!rs->fp) {
        printf("Cannot open %s\r\n", path);
        exit(1);
    }

    fseek(rs->fp, 0, SEEK_END);
    rs->file_size = ftell(rs->fp);
    rs->offset    = 0;
    fseek(rs->fp, 0, SEEK_SET);
}

int read_page(ReaderState *rs, char *buf) {
    int n;

    if (rs->offset >= rs->file_size) {
        return 0;
    }

    fseek(rs->fp, rs->offset, SEEK_SET);
    n = fread(buf, 1, PAGE_BUF_SIZE, rs->fp);

    /* Null-terminate */
    if (n < PAGE_BUF_SIZE) {
        buf[n] = '\0';
    } else {
        buf[PAGE_BUF_SIZE - 1] = '\0';
    }

    return n;
}
