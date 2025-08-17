/*================================================================
  fileio.c - text-page reader for HP 95LX with word wrapping
  ---------------------------------------------------------------
  - Builds with Borland Turbo C 2.01 (small model, 8088 code).
  - Needs only the public "95read.h".
  - Improvements in this version:
      - Proper word wrapping to prevent cutting words at the end of display lines.
      - Assumes display width of 40 characters and 16 lines per page (HP 95LX specs).
      - Formatting inserts '\n' at word boundaries (spaces) where possible.
      - Handles long words by hard wrapping if necessary.
      - Respects existing '\n' in the text for paragraph breaks.
      - Maintains low RAM footprint with static buffers, no dynamic allocation.
      - Encoding detection and transliteration integrated with wrapping.
      - Leftover raw bytes saved statically for sequential page reads.
      - Robustness at page boundary: does not consume the next character when page fills.
================================================================*/
#include "95read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strrchr, memcpy, strlen */

/* Fallback only if header doesn't define it (keeps standalone compilability) */
#ifndef PAGE_BUF_SIZE
#define PAGE_BUF_SIZE 1024
#endif

#define RAW_BUF_SIZE 2048  /* Sufficient for UTF-8 expansion and a full page */

/*----------------------------------------------------------------
  CP-1250 -> ASCII lookup table (0x80-0xFF)
  Improved with better mappings for quotation marks, dashes, and other common symbols.
----------------------------------------------------------------*/
static const unsigned char cp1250_to_ascii[128] = {
    /* 128-143 */ 'E','?','\'','?','\"','.','+','#','?','%','S','<','O','?','Z','?',
    /* 144-159 */ '?','\'','\'','\"','\"','*','-','-','?','T','s','>','o','?','z','Y',
    /* 160-175 */ ' ','?','?','L','?','A','?','?','?','?','S','?','?','?','?','Z',
    /* 176-191 */ '?','?','?','l','?','?','?','?','?','a','s','?','L','?','l','z',
    /* 192-207 */ 'R','A','A','A','A','L','C','C','C','E','E','E','E','I','I','D',
    /* 208-223 */ 'D','N','N','O','O','O','O','?','R','U','U','U','U','Y','T','s',
    /* 224-239 */ 'r','a','a','a','a','l','c','c','c','e','e','e','e','i','i','d',
    /* 240-255 */ 'd','n','n','o','o','o','o','?','r','u','u','u','u','y','t','?'
};

/*----------------------------------------------------------------
  UTF-8 edge buffer - keeps a cut multibyte sequence between reads
----------------------------------------------------------------*/
static unsigned char utf8_edge[4];
static int            utf8_edge_len = 0;

/*----------------------------------------------------------------
  Leftover raw buffer - keeps unused full sequences for next page
----------------------------------------------------------------*/
static unsigned char leftover[RAW_BUF_SIZE];
static int           leftover_len = 0;

/*----------------------------------------------------------------
  Track the expected next offset to detect jumps and clear leftovers
----------------------------------------------------------------*/
static long next_expected = -1L;

/*----------------------------------------------------------------
  Prepend saved leading bytes (if any) to freshly read text
----------------------------------------------------------------*/
static int prepend_utf8_edge(char *buf, int n, int cap) {
    int i;
    if (utf8_edge_len == 0) return n;

    if (utf8_edge_len + n >= cap)
        n = cap - 1 - utf8_edge_len;

    for (i = n - 1; i >= 0; --i)          /* move payload right */
        buf[i + utf8_edge_len] = buf[i];

    for (i = 0; i < utf8_edge_len; ++i)   /* copy edge in front */
        buf[i] = (char)utf8_edge[i];

    n += utf8_edge_len;
    buf[n] = '\0';
    utf8_edge_len = 0;
    return n;
}

/*----------------------------------------------------------------
  UTF-8 helper - length of a lead byte
----------------------------------------------------------------*/
static int utf8_lead_len(unsigned char c) {
    if ((c & 0x80U) == 0x00U) return 1;
    if ((c & 0xE0U) == 0xC0U) return 2;
    if ((c & 0xF0U) == 0xE0U) return 3;
    if ((c & 0xF8U) == 0xF0U) return 4;
    return 0;                             /* invalid lead       */
}

/*----------------------------------------------------------------
  Save an incomplete trailing UTF-8 sequence for next read
----------------------------------------------------------------*/
static int clip_and_save_utf8_trailer(char *buf, int n) {
    int k, need;
    if (n <= 0) return 0;

    for (k = n - 1; k >= 0; --k) {              /* walk backwards */
        unsigned char c = (unsigned char)buf[k];
        if ((c & 0xC0U) != 0x80U) {             /* found a lead ? */
            need = utf8_lead_len(c);
            if (need && need > n - k) {         /* incomplete     */
                utf8_edge_len = n - k;
                if (utf8_edge_len > 4) utf8_edge_len = 4;
                memcpy(utf8_edge, buf + k, utf8_edge_len);
                buf[k] = '\0';
                return k;                       /* new length     */
            }
            break;                              /* sequence ok    */
        }
    }
    return n;
}

/*================================================================
  Unicode-to-ASCII transliteration (Latin-1, Latin-Extended-A, symbols)
================================================================*/
typedef struct { unsigned short cp; unsigned char ascii; } UMap;

static const UMap latin1_ext_a[] = {
    {0x00A0,' '},{0x00A9,'C'},{0x00AB,'\"'},{0x00AD,'-'},{0x00B0,'o'},
    {0x00B5,'u'},{0x00B7,'.'},{0x00B9,'1'},{0x00BA,'o'},{0x00BB,'\"'},
    {0x00BC,'1'},{0x00BD,'1'},{0x00BE,'3'},{0x00DF,'s'},
    /* Latin-1 accented letters */
    {0x00C0,'A'},{0x00C1,'A'},{0x00C2,'A'},{0x00C3,'A'},{0x00C4,'A'},{0x00C5,'A'},{0x00C6,'A'},{0x00C7,'C'},
    {0x00C8,'E'},{0x00C9,'E'},{0x00CA,'E'},{0x00CB,'E'},{0x00CC,'I'},{0x00CD,'I'},{0x00CE,'I'},{0x00CF,'I'},
    {0x00D0,'D'},{0x00D1,'N'},{0x00D2,'O'},{0x00D3,'O'},{0x00D4,'O'},{0x00D5,'O'},{0x00D6,'O'},{0x00D7,'x'},
    {0x00D8,'O'},{0x00D9,'U'},{0x00DA,'U'},{0x00DB,'U'},{0x00DC,'U'},{0x00DD,'Y'},{0x00DE,'T'},{0x00DF,'s'},
    {0x00E0,'a'},{0x00E1,'a'},{0x00E2,'a'},{0x00E3,'a'},{0x00E4,'a'},{0x00E5,'a'},{0x00E6,'a'},{0x00E7,'c'},
    {0x00E8,'e'},{0x00E9,'e'},{0x00EA,'e'},{0x00EB,'e'},{0x00EC,'i'},{0x00ED,'i'},{0x00EE,'i'},{0x00EF,'i'},
    {0x00F0,'d'},{0x00F1,'n'},{0x00F2,'o'},{0x00F3,'o'},{0x00F4,'o'},{0x00F5,'o'},{0x00F6,'o'},{0x00F7,'/'},
    {0x00F8,'o'},{0x00F9,'u'},{0x00FA,'u'},{0x00FB,'u'},{0x00FC,'u'},{0x00FD,'y'},{0x00FE,'t'},{0x00FF,'y'},
    /* Latin-Extended-A (Central Europe) */
    {0x0104,'A'},{0x0105,'a'},{0x0106,'C'},{0x0107,'c'},{0x010C,'C'},{0x010D,'c'},
    {0x010E,'D'},{0x010F,'d'},{0x0110,'D'},{0x0111,'d'},{0x0118,'E'},{0x0119,'e'},
    {0x0139,'L'},{0x013A,'l'},{0x013D,'L'},{0x013E,'l'},{0x0141,'L'},{0x0142,'l'},
    {0x0143,'N'},{0x0144,'n'},{0x0147,'N'},{0x0148,'n'},{0x0150,'O'},{0x0151,'o'},
    {0x0154,'R'},{0x0155,'r'},{0x0158,'R'},{0x0159,'r'},{0x015A,'S'},{0x015B,'s'},
    {0x0160,'S'},{0x0161,'s'},{0x0164,'T'},{0x0165,'t'},{0x016E,'U'},{0x016F,'u'},
    {0x0170,'U'},{0x0171,'u'},{0x0178,'Y'},{0x0179,'Z'},{0x017A,'z'},
    {0x017B,'Z'},{0x017C,'z'},{0x017D,'Z'},{0x017E,'z'},
    /* Additional symbols and punctuation */
    {0x0152,'O'},{0x0153,'o'},{0x0178,'Y'},
    {0x20AC,'E'},{0x2030,'%'},{0x2122,'T'},
    {0x2020,'+'},{0x2021,'#'},
    {0x2039,'<'},{0x203A,'>'},
    {0x2013,'-'},{0x2014,'-'},{0x2015,'-'},{0x2212,'-'},
    {0x2018,'\''},{0x2019,'\''},{0x201A,'\''},{0x201B,'\''},
    {0x201C,'\"'},{0x201D,'\"'},{0x201E,'\"'},{0x201F,'\"'},
    {0x2026,'.'},{0x2022,'*'}
};

static unsigned char unicode_to_ascii(unsigned int cp) {
    int i;
    if (cp < 0x80U)                 /* 7-bit ASCII */
        return (unsigned char)cp;

    /* linear scan - table is small */
    for (i = 0; i < (int)(sizeof(latin1_ext_a) / sizeof(UMap)); ++i)
        if (latin1_ext_a[i].cp == cp)
            return latin1_ext_a[i].ascii;

    return '?';                     /* unknown char */
}

/*----------------------------------------------------------------
  Simple encoding detector (improved with BOM, validation, and stats)
  Returns: 0=UTF-8, 1=CP-1250, 2=ASCII
----------------------------------------------------------------*/
static int detect_encoding(const char *buf, int len) {
    int i, utf8_seen = 0, high_seen = 0, valid_utf8_count = 0;

    /* Check for UTF-8 BOM */
    if (len >= 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        return 0; /* UTF-8 */
    }

    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c >= 0x80U) {
            ++high_seen;
            if ((c & 0xE0U) == 0xC0U && i + 1 < len &&
                ((unsigned char)buf[i+1] & 0xC0U) == 0x80U) {
                unsigned int cp = ((c & 0x1FU) << 6) | ((unsigned char)buf[i+1] & 0x3FU);
                if (cp >= 0x80U && cp <= 0x7FFU) { /* Valid 2-byte UTF-8 */
                    utf8_seen = 1;
                    valid_utf8_count++;
                    i += 1;
                }
            } else if ((c & 0xF0U) == 0xE0U && i + 2 < len &&
                       ((unsigned char)buf[i+1] & 0xC0U) == 0x80U &&
                       ((unsigned char)buf[i+2] & 0xC0U) == 0x80U) {
                unsigned int cp = ((c & 0x0FU) << 12) |
                                 (((unsigned char)buf[i+1] & 0x3FU) << 6) |
                                 ((unsigned char)buf[i+2] & 0x3FU);
                if (cp >= 0x800U && cp <= 0xFFFFU &&
                    (cp < 0xD800U || cp > 0xDFFFU)) {
                    utf8_seen = 1;
                    valid_utf8_count++;
                    i += 2;
                }
            } else if ((c & 0xF8U) == 0xF0U && i + 3 < len &&
                       ((unsigned char)buf[i+1] & 0xC0U) == 0x80U &&
                       ((unsigned char)buf[i+2] & 0xC0U) == 0x80U &&
                       ((unsigned char)buf[i+3] & 0xC0U) == 0x80U) {
                /* Valid 4-byte sequence: treat as UTF-8 presence */
                utf8_seen = 1;
                valid_utf8_count++;
                i += 3;
            }
        }
    }

    if (!high_seen) return 2; /* ASCII */
    if (utf8_seen && (valid_utf8_count * 2) > (high_seen / 2)) return 0; /* likely UTF-8 */
    return 1; /* CP-1250 */
}

/*----------------------------------------------------------------
  I/O initialisation
----------------------------------------------------------------*/
void io_init(ReaderState *rs, const char *path) {
    const char *ext = strrchr(path, '.');
    int len = ext ? (int)(ext - path) : (int)strlen(path);
    int max = (int)sizeof(rs->progfile_base) - 1;
    if (len > max) len = max;

    memcpy(rs->progfile_base, path, (size_t)len);
    rs->progfile_base[len] = '\0';

    rs->fp = fopen(path, "rb");
    if (rs->fp == NULL) { printf("Cannot open \"%s\"\n", path); exit(1); }

    if (fseek(rs->fp, 0L, SEEK_END) != 0 ||
        (rs->file_size = ftell(rs->fp)) < 0L ||
        fseek(rs->fp, 0L, SEEK_SET) != 0)
    { printf("File error on \"%s\"\n", path); if (rs->fp) fclose(rs->fp); exit(1); }

    if (rs->file_size == 0L)
        printf("Warning: \"%s\" is empty\n", path);

    rs->offset = 0L;

    utf8_edge_len = 0;
    leftover_len = 0;
    next_expected = -1L;
}

/*----------------------------------------------------------------
  Optional cleanup (safe to call multiple times)
----------------------------------------------------------------*/
void io_done(ReaderState *rs) {
    if (rs && rs->fp) {
        fclose(rs->fp);
        rs->fp = NULL;
    }
    utf8_edge_len = 0;
    leftover_len = 0;
    next_expected = -1L;
}

/*----------------------------------------------------------------
  read_page - public API, word wrapping and full page formatting
  Returns number of bytes written to buf (0 on end-of-file).
----------------------------------------------------------------*/
int read_page(ReaderState *rs, char *buf) {
    static char raw_buf[RAW_BUF_SIZE];
    int raw_n = 0;
    int enc;
    int raw_pos = 0;
    long raw_used = 0;
    char *out = buf;
    int out_len = 0;
    int line_len = 0;
    int num_lines = 0;
    int is_eof = 0;
    int to_read;
    int read_bytes;
    char ch;
    unsigned int cp;
    int seq_len;
    unsigned char lead;
    char *line_start;
    char *last_space;
    char *p;
    int line_width = cfg.screen_cols;
    int page_lines = cfg.screen_lines - 1;  /* reserve status line */

    /* Fallbacks if cfg not initialized */
    if (line_width <= 0) line_width = 40;
    if (page_lines <= 0) page_lines = 15;

    if (rs->offset >= rs->file_size && utf8_edge_len == 0 && leftover_len == 0)
        return 0;

    /* Clear leftovers if not sequential forward read */
    if (next_expected != rs->offset) {
        leftover_len = 0;
        utf8_edge_len = 0;
    }

    /* Load leftover from previous page */
    if (leftover_len > 0) {
        memcpy(raw_buf, leftover, leftover_len);
        raw_n = leftover_len;
        leftover_len = 0;
    }

    /* Read more if needed and possible */
    to_read = RAW_BUF_SIZE - 1 - raw_n;
    if (to_read > 0 && rs->offset < rs->file_size) {
        /* Read immediately after the bytes we already staged (leftover) */
        if (fseek(rs->fp, rs->offset + raw_n, SEEK_SET) != 0) {
            printf("Seek error at %ld\n", rs->offset);
            return 0;
        }
        read_bytes = (int)fread(raw_buf + raw_n, 1, (size_t)to_read, rs->fp);
        if (read_bytes < to_read && ferror(rs->fp)) {
            printf("Read error at %ld\n", rs->offset);
        }
        raw_n += read_bytes;
    }

    raw_buf[raw_n] = '\0';

    /* Prepend any cut UTF-8 sequence from the previous read */
    raw_n = prepend_utf8_edge(raw_buf, raw_n, RAW_BUF_SIZE);

    /* Detect encoding */
    enc = detect_encoding(raw_buf, raw_n);

    /* Clip UTF-8 trailer if UTF-8 and not at file end */
    if (enc == 0 && rs->offset + raw_n < rs->file_size) {
        raw_n = clip_and_save_utf8_trailer(raw_buf, raw_n);
    }

    /* Skip UTF-8 BOM at file start */
    if (enc == 0 && rs->offset == 0 && raw_n >= 3 &&
        (unsigned char)raw_buf[0] == 0xEF &&
        (unsigned char)raw_buf[1] == 0xBB &&
        (unsigned char)raw_buf[2] == 0xBF) {
        memmove(raw_buf, raw_buf + 3, raw_n - 3);
        raw_n -= 3;
    }

    raw_used = 0;  /* Track actually consumed raw bytes */

    /* Build formatted page with word wrapping */
    while (num_lines < page_lines && out_len < PAGE_BUF_SIZE - 1 && !is_eof) {
        seq_len = 1;
        cp = 0;
        ch = 0;

        if (raw_pos >= raw_n) {
            is_eof = 1;
            continue;
        }

        lead = (unsigned char)raw_buf[raw_pos];

        if (enc == 0) {  /* UTF-8 */
            seq_len = utf8_lead_len(lead);
            if (seq_len == 1) {
                cp = lead;
            } else if (seq_len == 2 && raw_pos + 1 < raw_n) {
                cp = ((lead & 0x1FU) << 6) |
                     ((unsigned char)raw_buf[raw_pos + 1] & 0x3FU);
            } else if (seq_len == 3 && raw_pos + 2 < raw_n) {
                cp = ((lead & 0x0FU) << 12) |
                     (((unsigned char)raw_buf[raw_pos + 1] & 0x3FU) << 6) |
                     ((unsigned char)raw_buf[raw_pos + 2] & 0x3FU);
            } else if (seq_len == 4 && raw_pos + 3 < raw_n) {
                /* Consume all 4 bytes even though we will map to '?' */
                cp = 0x10000U; /* beyond BMP -> will become '?' */
            } else {
                /* Invalid or incomplete; treat lead as a single unknown */
                cp = '?';
                seq_len = 1;
            }
            ch = unicode_to_ascii(cp);
        } else if (enc == 1) {  /* CP-1250 */
            ch = (lead < 0x80U) ? (char)lead : (char)cp1250_to_ascii[lead - 128];
        } else {  /* ASCII */
            ch = (char)lead;
        }

        raw_pos += seq_len;
        raw_used += seq_len;

        /* Normalize CRLF: ignore '\r' */
        if (ch == '\r') continue;

        /* Respect existing newlines */
        if (ch == '\n') {
            if (num_lines < page_lines - 1) {
                *out++ = '\n';
                out_len++;
                line_len = 0;
                num_lines++;
            } else {
                raw_pos -= seq_len;
                raw_used -= seq_len;
                break;
            }
            continue;
        }

        /* Lookahead for word fit if starting a new word */
        if (ch != ' ' && (line_len == 0 || *(out - 1) == ' ')) {
            int temp_pos = raw_pos - seq_len;  /* start from current ch */
            int word_len = 0;
            while (temp_pos < raw_n) {
                unsigned char tlead = (unsigned char)raw_buf[temp_pos];
                int tseq = 1;
                unsigned int tcp = 0;
                char tch = 0;
                if (enc == 0) {
                    tseq = utf8_lead_len(tlead);
                    if (tseq == 1) {
                        tcp = tlead;
                    } else if (tseq == 2 && temp_pos + 1 < raw_n) {
                        tcp = ((tlead & 0x1FU) << 6) |
                              ((unsigned char)raw_buf[temp_pos + 1] & 0x3FU);
                    } else if (tseq == 3 && temp_pos + 2 < raw_n) {
                        tcp = ((tlead & 0x0FU) << 12) |
                              (((unsigned char)raw_buf[temp_pos + 1] & 0x3FU) << 6) |
                              ((unsigned char)raw_buf[temp_pos + 2] & 0x3FU);
                    } else if (tseq == 4 && temp_pos + 3 < raw_n) {
                        tcp = 0x10000U;
                    } else {
                        tcp = '?';
                        tseq = 1;
                    }
                    tch = unicode_to_ascii(tcp);
                } else if (enc == 1) {
                    tch = (tlead < 0x80U) ? (char)tlead : (char)cp1250_to_ascii[tlead - 128];
                } else {
                    tch = (char)tlead;
                }
                if (tch == ' ' || tch == '\n' || tch == '\r') break;
                word_len++;
                temp_pos += tseq;
            }
            if (word_len > line_width - line_len) {
                /* Word doesn't fit in remaining space */
                if (num_lines < page_lines - 1) {
                    /* Can wrap to new line */
                    line_start = out - line_len;
                    last_space = NULL;
                    for (p = line_start; p < out; ++p) {
                        if (*p == ' ') last_space = p;
                    }
                    if (last_space) {
                        *last_space = '\n';
                        num_lines++;
                        line_len = (int)(out - (last_space + 1));
                        continue;  /* Don't add ch yet, it will be added on new line */
                    }
                    /* If no space (long word at line start), fall through to add */
                } else {
                    /* Can't wrap (last line of page), don't start word */
                    raw_pos -= seq_len;
                    raw_used -= seq_len;
                    break;
                }
            }
        }

        /* Wrap if current line is already full before adding this char */
        if (line_len == line_width) {
            if (num_lines < page_lines - 1) {
                line_start = out - line_len;
                last_space = NULL;
                for (p = line_start; p < out; ++p) {
                    if (*p == ' ') last_space = p;
                }
                if (last_space) {
                    /* Soft wrap at last space: turn it into newline */
                    *last_space = '\n';
                    num_lines++;
                    line_len = (int)(out - (last_space + 1));
                } else {
                    /* Hard wrap (no space found): break line here */
                    *out++ = '\n';
                    out_len++;
                    num_lines++;
                    line_len = 0;
                }
            } else {
                /* Can't wrap, check if we can skip space or must stop */
                if (ch == ' ') {
                    /* Skip space (consume but don't add) */
                    continue;
                } else {
                    raw_pos -= seq_len;
                    raw_used -= seq_len;
                    break;
                }
            }
        }

        /* Add the character to the current (possibly new) line */
        if (line_len < line_width) {
            *out++ = ch;
            out_len++;
            line_len++;
        } else {
            /* Safety: shouldn't happen due to wrap above */
            continue;
        }
    }

    *out = '\0';

    /* Advance file offset by used raw bytes */
    rs->offset += raw_used;

    /* Save any leftover raw bytes for next page */
    if (raw_pos < raw_n) {
        leftover_len = raw_n - raw_pos;
        if (leftover_len > RAW_BUF_SIZE) leftover_len = RAW_BUF_SIZE; /* clamp */
        memcpy(leftover, raw_buf + raw_pos, leftover_len);
    }

    /* Update expected next offset */
    next_expected = rs->offset;

    return out_len;
}