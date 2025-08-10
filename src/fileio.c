#include "95read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* for strrchr(), memcpy(), strlen() */

/*---------------------------------------------------------------
  CP1250 to ASCII lookup table for characters 128-255
  '?' is used as a safe fallback for unmapped glyphs.
---------------------------------------------------------------*/
static const unsigned char cp1250_to_ascii[128] = {
    /* 128-143 */ '?','?','?','?','?','?','?','?','?','?','S','?','S','T','Z','Z',
    /* 144-159 */ '?','?','?','?','?','?','?','?','?','?','s','?','s','t','z','z',
    /* 160-175 */ ' ','?','?','L','?','A','?','?','?','?','S','?','?','?','?','Z',
    /* 176-191 */ '?','?','?','l','?','?','?','?','?','a','s','?','L','?','l','z',
    /* 192-207 */ 'R','A','A','A','A','L','C','C','C','E','E','E','E','I','I','D',
    /* 208-223 */ 'D','N','N','O','O','O','O','?','R','U','U','U','U','Y','T','s',
    /* 224-239 */ 'r','a','a','a','a','l','c','c','c','e','e','e','e','i','i','d',
    /* 240-255 */ 'd','n','n','o','o','o','o','?','r','u','u','u','u','y','t','?'
};

/*---------------------------------------------------------------
  Edge-state for UTF-8 chunking across reads:
  - When a multibyte sequence is cut by the buffer boundary, we
    save the lead bytes here and prepend them to the next read.
  - This keeps conversions correct without modifying other modules.
---------------------------------------------------------------*/
static unsigned char utf8_edge[4];
static int utf8_edge_len = 0;

/*---------------------------------------------------------------
  Append UTF-8 edge bytes (from previous read) in front of new data.
  Returns combined length (<= cap-1), keeps buf NUL-terminated.
---------------------------------------------------------------*/
static int prepend_utf8_edge(char *buf, int n, int cap) {
    int i;
    if (utf8_edge_len <= 0) return n;
    if (utf8_edge_len + n >= cap) n = cap - 1 - utf8_edge_len;
    /* move current data to the right */
    for (i = n - 1; i >= 0; --i) buf[i + utf8_edge_len] = buf[i];
    /* copy edge bytes at start */
    for (i = 0; i < utf8_edge_len; ++i) buf[i] = (char)utf8_edge[i];
    n += utf8_edge_len;
    buf[n] = '\0';
    utf8_edge_len = 0;
    return n;
}

/*---------------------------------------------------------------
  Detect UTF-8 lead length from first byte, 0 if not a valid lead.
---------------------------------------------------------------*/
static int utf8_lead_len(unsigned char c) {
    if ((c & 0x80U) == 0x00U) return 1;
    if ((c & 0xE0U) == 0xC0U) return 2;
    if ((c & 0xF0U) == 0xE0U) return 3;
    if ((c & 0xF8U) == 0xF0U) return 4;
    return 0;
}

/*---------------------------------------------------------------
  Save incomplete trailing UTF-8 sequence for the next read.
  Returns the trimmed length (excluding the saved sequence).
---------------------------------------------------------------*/
static int clip_and_save_utf8_trailer(char *buf, int n) {
    int k, need;
    if (n <= 0) return 0;

    /* walk back from end to find a valid lead */
    for (k = n - 1; k >= 0; --k) {
        unsigned char c = (unsigned char)buf[k];
        if ((c & 0xC0U) != 0x80U) {
            /* lead candidate */
            need = utf8_lead_len(c);
            if (need > 1) {
                int have = n - k;
                if (have < need && have <= 4) {
                    /* save partial sequence */
                    int i;
                    utf8_edge_len = have;
                    for (i = 0; i < have; ++i)
                        utf8_edge[i] = (unsigned char)buf[k + i];
                    buf[k] = '\0';
                    return k;
                }
            }
            break; /* either complete or 1-byte ASCII */
        }
    }
    return n;
}

/*---------------------------------------------------------------
  UTF-8 to ASCII (lossy) conversion in-place.
  Replaces any non-ASCII code point with '?'.
---------------------------------------------------------------*/
static void utf8_to_ascii_inplace(char *buf) {
    unsigned char *src = (unsigned char*)buf;
    unsigned char *dst = (unsigned char*)buf;

    while (*src) {
        unsigned char c = *src;
        if (c < 0x80U) {
            *dst++ = (char)c;
            ++src;
        } else if ((c & 0xE0U) == 0xC0U) {
            /* 2-byte */
            if ((src[1] & 0xC0U) == 0x80U && src[1] != 0) {
                *dst++ = '?';
                src += 2;
            } else {
                /* invalid continuation: skip lead */
                *dst++ = '?';
                ++src;
            }
        } else if ((c & 0xF0U) == 0xE0U) {
            if ((src[1] & 0xC0U) == 0x80U && (src[2] & 0xC0U) == 0x80U && src[1] != 0 && src[2] != 0) {
                *dst++ = '?';
                src += 3;
            } else {
                *dst++ = '?';
                ++src;
            }
        } else if ((c & 0xF8U) == 0xF0U) {
            if ((src[1] & 0xC0U) == 0x80U && (src[2] & 0xC0U) == 0x80U &&
                (src[3] & 0xC0U) == 0x80U && src[1] != 0 && src[2] != 0 && src[3] != 0) {
                *dst++ = '?';
                src += 4;
            } else {
                *dst++ = '?';
                ++src;
            }
        } else {
            /* stray continuation or invalid lead */
            *dst++ = '?';
            ++src;
        }
    }
    *dst = '\0';
}

/*---------------------------------------------------------------
  CP1250 to ASCII conversion in-place using lookup table.
---------------------------------------------------------------*/
static void cp1250_to_ascii_inplace(char *buf) {
    unsigned char *src = (unsigned char*)buf;
    unsigned char *dst = (unsigned char*)buf;

    while (*src) {
        unsigned char c = *src++;
        if (c < 0x80U) {
            *dst++ = (char)c;
        } else {
            *dst++ = (char)cp1250_to_ascii[c - 128];
        }
    }
    *dst = '\0';
}

/*---------------------------------------------------------------
  Simple encoding detection:
    0 = UTF-8 (some valid sequences seen)
    1 = CP1250 (high bytes but no valid UTF-8 sequences)
    2 = ASCII (no high bytes)
---------------------------------------------------------------*/
static int detect_encoding(const char *buf, int len) {
    int i;
    int utf8_sequences = 0;
    int high_chars = 0;

    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)buf[i];

        if (c >= 0x80U) {
            ++high_chars;

            /* Detect valid 2- or 3-byte UTF-8 sequences. */
            if ((c & 0xE0U) == 0xC0U) {
                if (i + 1 < len) {
                    unsigned char c1 = (unsigned char)buf[i + 1];
                    if ((c1 & 0xC0U) == 0x80U) {
                        ++utf8_sequences;
                        ++i;
                    }
                }
            } else if ((c & 0xF0U) == 0xE0U) {
                if (i + 2 < len) {
                    unsigned char c1 = (unsigned char)buf[i + 1];
                    unsigned char c2 = (unsigned char)buf[i + 2];
                    if ((c1 & 0xC0U) == 0x80U && (c2 & 0xC0U) == 0x80U) {
                        ++utf8_sequences;
                        i += 2;
                    }
                }
            }
        }
    }

    if (high_chars == 0) return 2;  /* ASCII */
    if (utf8_sequences > 0) return 0; /* UTF-8 */
    return 1;                        /* CP1250 (assumed) */
}

/*---------------------------------------------------------------
  io_init: open the file and prepare ReaderState.
---------------------------------------------------------------*/
void io_init(ReaderState *rs, const char *path) {
    const char *ext = strrchr(path, '.');
    int len, max;

    /* compute filename base (without extension) */
    if (ext) len = (int)(ext - path);
    else     len = (int)strlen(path);

    /* leave room for terminating null in progfile_base[] */
    max = (int)sizeof(rs->progfile_base) - 1;
    if (len > max) len = max;

    memcpy(rs->progfile_base, path, (size_t)len);
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
    if (rs->file_size < 0L) {
        printf("ftell error on \"%s\"\n", path);
        fclose(rs->fp);
        exit(1);
    }
    fseek(rs->fp, 0L, SEEK_SET);

    /* warn if file is empty */
    if (rs->file_size == 0L) {
        printf("Warning: \"%s\" is empty\n", path);
    }

    /* reset UTF-8 edge state */
    utf8_edge_len = 0;
}

/*---------------------------------------------------------------
  read_page:
  - Reads up to PAGE_BUF_SIZE-1 bytes at current rs->offset.
  - Preserves trailing partial UTF-8 sequences for next read.
  - Converts to ASCII (UTF-8 or CP1250) in-place.
  - Returns number of bytes consumed from the file (pre-conversion).
---------------------------------------------------------------*/
int read_page(ReaderState *rs, char *buf) {
    int n;
    int cap = PAGE_BUF_SIZE;
    int enc;

    /* nothing left to read? */
    if (rs->offset >= rs->file_size)
        return 0;

    /* position to current offset */
    if (fseek(rs->fp, rs->offset, SEEK_SET) != 0) {
        printf("Seek error at offset %ld\n", rs->offset);
        return 0;
    }

    /* read into buffer, leave room for terminating NUL */
    n = (int)fread(buf, 1, (size_t)(cap - 1), rs->fp);
    buf[n] = '\0';

    /* prepend any saved UTF-8 leading bytes from previous read */
    n = prepend_utf8_edge(buf, n, cap);

    /* if the chunk ends with an incomplete UTF-8 lead, save it for next time */
    n = clip_and_save_utf8_trailer(buf, n);

    /* detect encoding on the raw bytes */
    enc = detect_encoding(buf, n);

    /* convert to ASCII in place (lossy) */
    if (enc == 0) {
        utf8_to_ascii_inplace(buf);
    } else if (enc == 1) {
        cp1250_to_ascii_inplace(buf);
    } else {
        /* ASCII: buf already fine */
    }

    /* report any read errors (Turbo C: ferror is fine) */
    if (n < cap - 1 && ferror(rs->fp)) {
        printf("Read error at offset %ld\n", rs->offset);
    }

    /* Return bytes consumed from file (before edge-trim).
       This keeps pagination accurate even when we stash a few
       trailing bytes for the next page. */
    return n;
}
