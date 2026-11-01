/* tests/test_main.c
 * ------------------------------------------------------------------
 * 95read test harness - Turbo C 2.01 (small model), run under DOSBox.
 *
 * Links the real project modules (config.c, fileio.c, progress.c,
 * display.c) and exercises them directly.  Results are printed to
 * stdout and written to tests_results.txt so the host can parse them.
 *
 * Build with build-tests.bat, run with run-tests.bat.
 * ------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "95read.h"

#define RESULTS_FILE "results.txt"
#define CFG_TEST     "tests\\test.cfg"

static int  pass_count = 0;
static int  fail_count = 0;
static int  checks_run = 0;      /* total check() calls (false-green guard) */
static FILE *res = NULL;
static char detail[256];

/* Total number of check() calls in the suite.  The harness verifies at the
   end that every planned check actually executed; a mismatch means the run
   was incomplete (crash / early exit) and the results must not be trusted.
   This is the primary defence against a false green. */
#define PLANNED_CHECKS 71

static void setd(const char *s)
{
    strncpy(detail, s, sizeof(detail) - 1);
    detail[sizeof(detail) - 1] = '\0';
}

static void check(int ok, const char *name)
{
    checks_run++;
    if (ok) {
        pass_count++;
        printf("PASS: %s\n", name);
        if (res) { fprintf(res, "PASS: %s\n", name); fflush(res); }
    } else {
        fail_count++;
        printf("FAIL: %s | %s\n", name, detail);
        if (res) { fprintf(res, "FAIL: %s | %s\n", name, detail); fflush(res); }
    }
}

static int write_file(const char *path, const void *data, int len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (len > 0 && fwrite(data, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* ==================================================================
   config.c tests
   ================================================================== */
static void test_config_defaults(void)
{
    remove(CFG_TEST);
    load_config();

    setd("expected 16");  check(cfg.screen_lines == 16, "config: default screen_lines");
    setd("expected 40");  check(cfg.screen_cols  == 40, "config: default screen_cols");
    setd("expected 8");   check(cfg.tab_width    == 8,  "config: default tab_width");
    setd("expected N");   check(cfg.key_next_page == 'N', "config: default key_next_page");
    setd("expected P");   check(cfg.key_prev_page == 'P', "config: default key_prev_page");
    setd("expected C");   check(cfg.key_quit      == 'C', "config: default key_quit");
    setd("expected I");   check(cfg.key_invert    == 'I', "config: default key_invert");
    setd("expected .95r");
    check(strcmp(cfg.prog_ext, ".95r") == 0, "config: default prog_ext");
    setd("default file should exist");
    check(file_exists(CFG_TEST), "config: writes default file when missing");
}

static void test_config_parse(void)
{
    static const char *cfgtext =
        "# test config\n"
        "screen_lines = 8\n"
        "screen_cols = 20\n"
        "tab_width = 4\n"
        "key_next_page = 'Z'\n"
        "key_prev_page = 0x41\n"
        "key_quit = ENTER\n"
        "key_invert = \"I\"\n"
        "prog_ext = 95r\n";

    remove(CFG_TEST);
    if (!write_file(CFG_TEST, cfgtext, (int)strlen(cfgtext))) {
        setd("could not write test config");
        check(0, "config: write test config");
        return;
    }
    load_config();

    setd("expected 8");   check(cfg.screen_lines == 8,  "config: parse screen_lines");
    setd("expected 20");  check(cfg.screen_cols  == 20, "config: parse screen_cols");
    setd("expected 4");   check(cfg.tab_width    == 4,  "config: parse tab_width");
    setd("expected Z");   check(cfg.key_next_page == 'Z', "config: parse quoted key");
    setd("expected A");   check(cfg.key_prev_page == 'A', "config: parse hex key");
    setd("expected 13");  check(cfg.key_quit == 13,      "config: parse named key ENTER");
    setd("expected I");   check(cfg.key_invert == 'I',    "config: parse double-quoted key");
    setd("expected .95r");
    check(strcmp(cfg.prog_ext, ".95r") == 0, "config: prog_ext gets leading dot");
}

static void test_config_out_of_range(void)
{
    static const char *cfgtext =
        "screen_lines = 1000\n"
        "screen_cols = 2\n"
        "tab_width = 99\n";

    remove(CFG_TEST);
    write_file(CFG_TEST, cfgtext, (int)strlen(cfgtext));
    load_config();

    /* readme.txt: invalid or out-of-range values are clamped to safe bounds. */
    setd("expected clamp to 100"); check(cfg.screen_lines == 100, "config: clamps screen_lines high");
    setd("expected clamp to 4");   check(cfg.screen_cols  == 4,   "config: clamps screen_cols low");
    setd("expected clamp to 16");  check(cfg.tab_width    == 16,  "config: clamps tab_width high");
}

static void test_config_comments(void)
{
    static const char *cfgtext =
        "screen_lines = 10 # inline comment\n"
        "screen_cols = 12 ; semicolon comment\n";

    remove(CFG_TEST);
    write_file(CFG_TEST, cfgtext, (int)strlen(cfgtext));
    load_config();

    setd("expected 10"); check(cfg.screen_lines == 10, "config: inline # comment");
    setd("expected 12"); check(cfg.screen_cols  == 12, "config: inline ; comment");
}

/* ==================================================================
   progress.c tests
   ================================================================== */
static void test_progress_roundtrip(void)
{
    ReaderState rs, rs2;
    long hist[5];

    hist[0] = 0; hist[1] = 100; hist[2] = 250; hist[3] = 250; hist[4] = 900;

    strcpy(rs.progfile_base, "tests\\sample");
    rs.history = hist;
    rs.hist_count = 5;
    rs.hist_capacity = 5;
    rs.file_size = 1000L;
    rs.fp = NULL;
    rs.offset = 900L;

    remove("tests\\sample.95r");
    save_progress(&rs);

    setd("file should exist");
    check(file_exists("tests\\sample.95r"), "progress: save creates file");

    rs2.history = NULL; rs2.hist_count = 0; rs2.hist_capacity = 0;
    rs2.offset = 0L; rs2.file_size = 1000L; rs2.fp = NULL;
    strcpy(rs2.progfile_base, "tests\\sample");
    load_progress(&rs2);

    setd("expected 4 after dedupe");
    check(rs2.hist_count == 4, "progress: consecutive duplicates removed");
    setd("offsets mismatch");
    check(rs2.history && rs2.history[0] == 0 && rs2.history[1] == 100 &&
          rs2.history[2] == 250 && rs2.history[3] == 900,
          "progress: offsets round-trip");
    setd("expected resume at 900");
    check(rs2.offset == 900L, "progress: resumes at last offset");

    if (rs2.history) { free(rs2.history); rs2.history = NULL; }
}

static void test_progress_checksum(void)
{
    ReaderState rs, rs2;
    long hist[3];
    unsigned char bad;
    FILE *f;

    hist[0] = 0; hist[1] = 100; hist[2] = 500;

    strcpy(rs.progfile_base, "tests\\sample");
    rs.history = hist; rs.hist_count = 3; rs.hist_capacity = 3;
    rs.file_size = 1000L; rs.fp = NULL; rs.offset = 500L;

    remove("tests\\sample.95r");
    save_progress(&rs);

    /* Flip one byte inside the offsets area (byte offset 6). */
    f = fopen("tests\\sample.95r", "r+b");
    if (f) {
        fseek(f, 6L, SEEK_SET);
        if (fread(&bad, 1, 1, f) == 1) {
            bad = (unsigned char)(bad ^ 0xFF);
            fseek(f, 6L, SEEK_SET);
            fwrite(&bad, 1, 1, f);
        }
        fclose(f);
    }

    rs2.history = NULL; rs2.hist_count = 0; rs2.hist_capacity = 0;
    rs2.offset = 0L; rs2.file_size = 1000L; rs2.fp = NULL;
    strcpy(rs2.progfile_base, "tests\\sample");
    load_progress(&rs2);

    setd("corrupt file should not load");
    check(rs2.hist_count == 0, "progress: checksum rejects corruption");

    if (rs2.history) { free(rs2.history); rs2.history = NULL; }
}

static void test_progress_legacy(void)
{
    ReaderState rs;
    unsigned char buf[14];

    /* legacy layout: int count (2 bytes LE) + count * long (4 bytes LE) */
    buf[0] = 3; buf[1] = 0;                 /* count = 3 */
    buf[2] = 0;   buf[3] = 0; buf[4] = 0; buf[5] = 0;        /* 0 */
    buf[6] = 200; buf[7] = 0; buf[8] = 0; buf[9] = 0;        /* 200 */
    buf[10] = (unsigned char)(700 & 0xFF);
    buf[11] = (unsigned char)((700 >> 8) & 0xFF);
    buf[12] = 0; buf[13] = 0;                                /* 700 */

    remove("tests\\legacy.95r");
    write_file("tests\\legacy.95r", buf, 14);

    rs.history = NULL; rs.hist_count = 0; rs.hist_capacity = 0;
    rs.offset = 0L; rs.file_size = 1000L; rs.fp = NULL;
    strcpy(rs.progfile_base, "tests\\legacy");
    load_progress(&rs);

    setd("expected 3 entries");
    check(rs.hist_count == 3, "progress: loads legacy format");
    setd("offsets mismatch");
    check(rs.history && rs.history[0] == 0 && rs.history[1] == 200 &&
          rs.history[2] == 700, "progress: legacy offsets");
    setd("expected offset 700");
    check(rs.offset == 700L, "progress: legacy resume offset");

    if (rs.history) { free(rs.history); rs.history = NULL; }
}

static void test_progress_custom_ext(void)
{
    ReaderState rs;
    long hist[2];

    hist[0] = 0; hist[1] = 300;

    strcpy(rs.progfile_base, "tests\\custom");
    rs.history = hist; rs.hist_count = 2; rs.hist_capacity = 2;
    rs.file_size = 1000L; rs.fp = NULL; rs.offset = 300L;

    strcpy(cfg.prog_ext, ".dat");
    remove("tests\\custom.dat");
    save_progress(&rs);

    setd("custom file should exist");
    check(file_exists("tests\\custom.dat"), "progress: honours custom prog_ext");

    strcpy(cfg.prog_ext, ".95r");   /* restore */
}

static void test_progress_empty(void)
{
    ReaderState rs;

    strcpy(rs.progfile_base, "tests\\empty");
    rs.history = NULL; rs.hist_count = 0; rs.hist_capacity = 0;
    rs.file_size = 100L; rs.fp = NULL; rs.offset = 0L;

    remove("tests\\empty.95r");
    save_progress(&rs);

    setd("file should exist");
    check(file_exists("tests\\empty.95r"), "progress: writes header when no history");
}

/* ==================================================================
   fileio.c tests
   ================================================================== */
static void test_fileio_basic(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t1.txt", "ABCDEFGHIJKLMNOP", 16);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t1.txt");

    n = read_page(&rs, buf);
    setd("expected 17");
    check(n == 17, "fileio: basic page length");
    setd(buf);
    check(strncmp(buf, "ABCDEFGHIJ\nKLMNOP", 17) == 0, "fileio: hard wrap at width");
    setd("expected offset 16");
    check(rs.offset == 16L, "fileio: offset advanced");

    io_done(&rs);
}

static void test_fileio_wordwrap(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t2.txt", "HELLO WORLD FOO", 15);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t2.txt");

    n = read_page(&rs, buf);
    setd(buf);
    check(n == 15, "fileio: wordwrap length");
    check(strncmp(buf, "HELLO\nWORLD FOO", 15) == 0, "fileio: wraps at space");

    io_done(&rs);
}

static void test_fileio_crlf(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t3.txt", "AAA\r\nBBB", 8);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t3.txt");

    n = read_page(&rs, buf);
    setd(buf);
    check(n == 7, "fileio: crlf length");
    check(strncmp(buf, "AAA\nBBB", 7) == 0, "fileio: CR ignored, LF newline");

    io_done(&rs);
}

static void test_fileio_utf8(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    static const unsigned char t[] = { 'c', 'a', 'f', 0xC3, 0xA9 };
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t4.txt", t, 5);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t4.txt");

    n = read_page(&rs, buf);
    setd(buf);
    check(n == 4, "fileio: utf8 transliteration length");
    check(strncmp(buf, "cafe", 4) == 0, "fileio: UTF-8 e-acute -> e");

    io_done(&rs);
}

static void test_fileio_cp1250(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    static const unsigned char t[] = { 'c', 'a', 'f', 0xE9 };
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t5.txt", t, 4);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t5.txt");

    n = read_page(&rs, buf);
    setd(buf);
    check(n == 4, "fileio: cp1250 length");
    check(strncmp(buf, "cafe", 4) == 0, "fileio: CP-1250 e-acute -> e");

    io_done(&rs);
}

static void test_fileio_bom(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    static const unsigned char t[] = { 0xEF, 0xBB, 0xBF, 'H', 'i' };
    int n;

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t6.txt", t, 5);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t6.txt");

    n = read_page(&rs, buf);
    setd(buf);
    check(n == 2, "fileio: bom length");
    check(strncmp(buf, "Hi", 2) == 0, "fileio: UTF-8 BOM skipped");

    io_done(&rs);
}

static void test_fileio_eof(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t7.txt", "XY", 2);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t7.txt");

    read_page(&rs, buf);
    setd("expected 0 at EOF");
    check(read_page(&rs, buf) == 0, "fileio: returns 0 at EOF");

    io_done(&rs);
}

/* ==================================================================
   display.c tests (read the emulated screen back with gettext)
   ================================================================== */
static void screen_get_row(int row, int cols, char *out)
{
    char buf[320];
    int i;
    gettext(1, row, cols, row, buf);
    for (i = 0; i < cols; i++) out[i] = buf[i * 2];
    out[cols] = '\0';
}

static int row_is_blank(const char *row, int cols)
{
    int i;
    for (i = 0; i < cols; i++) if (row[i] != ' ') return 0;
    return 1;
}

static void test_display_simple(void)
{
    char r1[80], r2[80], r4[80];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    display_page("HELLO\r\nWORLD");

    /* Read every row before printing anything: harness output would
       otherwise be written onto the screen and pollute the readback. */
    screen_get_row(1, 10, r1);
    screen_get_row(2, 10, r2);
    screen_get_row(4, 10, r4);

    setd(r1);
    check(strncmp(r1, "HELLO", 5) == 0, "display: row1 HELLO");
    setd(r2);
    check(strncmp(r2, "WORLD", 5) == 0, "display: row2 WORLD");
    setd(r4);
    check(row_is_blank(r4, 10), "display: last line reserved for footer");
}

static void test_display_wrap(void)
{
    char r1[80], r2[80], r3[80];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    display_page("ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    screen_get_row(1, 10, r1);
    screen_get_row(2, 10, r2);
    screen_get_row(3, 10, r3);

    setd(r1);
    check(strncmp(r1, "ABCDEFGHIJ", 10) == 0, "display: wrap row1");
    setd(r2);
    check(strncmp(r2, "KLMNOPQRST", 10) == 0, "display: wrap row2");
    setd(r3);
    check(strncmp(r3, "UVWXYZ", 6) == 0, "display: wrap row3");
}

static void test_display_tab(void)
{
    char r1[80];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    cfg.tab_width = 4;
    display_page("\tX");

    screen_get_row(1, 10, r1);
    setd(r1);
    check(strncmp(r1, "    X", 5) == 0, "display: tab expands to 4 spaces");
}

static void test_display_backspace(void)
{
    char r1[80];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    display_page("AB\bC");

    screen_get_row(1, 10, r1);
    setd(r1);
    check(strncmp(r1, "AC", 2) == 0, "display: backspace erases");
}

static void test_display_formfeed(void)
{
    char r1[80];

    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    display_page("ABC\fDEF");

    screen_get_row(1, 10, r1);
    setd(r1);
    check(strncmp(r1, "DEF", 3) == 0, "display: form feed clears screen");
}

/* ==================================================================
   UTF-8 read-boundary regression
   A multibyte sequence straddling the RAW_BUF_SIZE boundary must not
   produce a spurious '?' and must be transliterated exactly once.
   ================================================================== */
static void test_utf8_boundary(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];
    char big[2200];
    int i, nq = 0, ne = 0, n;

    /* 2046 'A's, then a euro sign (E2 82 AC) straddling byte 2047. */
    for (i = 0; i < 2046; ++i) big[i] = 'A';
    big[2046] = (char)0xE2; big[2047] = (char)0x82; big[2048] = (char)0xAC;
    for (i = 2049; i < 2200; ++i) big[i] = 'B';
    write_file("tests\\t9.txt", big, 2200);

    cfg.screen_cols = 40;
    cfg.screen_lines = 16;
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t9.txt");

    while ((n = read_page(&rs, buf)) > 0) {
        int j;
        for (j = 0; j < n; ++j) {
            if (buf[j] == '?') nq++;
            else if (buf[j] == 'E') ne++;
        }
    }
    io_done(&rs);

    setd("no spurious '?' at the UTF-8 read boundary");
    check(nq == 0, "fileio: no spurious '?' at UTF-8 read boundary");
    setd("euro sign transliterates to exactly one E");
    check(ne == 1, "fileio: euro sign transliterated once at boundary");
}

/* ==================================================================
   breadcrumb (display.c) tests
   Page size for these tests: cfg 10x4 -> 10 * (4-1) = 30 bytes/page.
   ================================================================== */
static void breadcrumb_geom(void)
{
    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
}

static void test_breadcrumb_empty(void)
{
    ReaderState rs;
    char out[64];

    breadcrumb_geom();
    memset(&rs, 0, sizeof(rs));
    rs.history = NULL;
    rs.hist_count = 0;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    check(out[0] == '\0', "breadcrumb: empty history yields empty trail");
}

static void test_breadcrumb_single(void)
{
    ReaderState rs;
    long hist[1];
    char out[64];

    breadcrumb_geom();
    hist[0] = 0;
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = 1;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    check(strcmp(out, "1") == 0, "breadcrumb: single entry is page 1");
}

static void test_breadcrumb_multi(void)
{
    ReaderState rs;
    long hist[3];
    char out[64];

    breadcrumb_geom();
    hist[0] = 0; hist[1] = 100; hist[2] = 250;   /* pages 1, 4, 9 */
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = 3;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    check(strcmp(out, "1>4>9") == 0, "breadcrumb: entries joined with >");
}

static void test_breadcrumb_cap(void)
{
    ReaderState rs;
    long hist[3];
    char out[4];

    breadcrumb_geom();
    hist[0] = 0; hist[1] = 100; hist[2] = 250;
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = 3;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    check(strlen(out) < sizeof(out), "breadcrumb: respects output capacity");
    check(out[strlen(out)] == '\0', "breadcrumb: always NUL-terminated");
}

static void test_breadcrumb_max_entries(void)
{
    ReaderState rs;
    long hist[BREADCRUMB_MAX + 2];
    char out[128];
    int i;

    breadcrumb_geom();
    for (i = 0; i < BREADCRUMB_MAX + 2; ++i) hist[i] = (long)i * 100;
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = BREADCRUMB_MAX + 2;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    /* Only the newest BREADCRUMB_MAX entries are kept (pages 7..24). */
    check(strncmp(out, "7>", 2) == 0, "breadcrumb: keeps only newest entries");
}

static void test_breadcrumb_clamp(void)
{
    ReaderState rs;
    long hist[2];
    char out[64];

    breadcrumb_geom();
    hist[0] = -50; hist[1] = 5000;   /* negative clamps to page 1 */
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = 2;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, sizeof(out));
    setd(out);
    check(strcmp(out, "1>167") == 0, "breadcrumb: clamps negative offsets");
}

static void test_breadcrumb_cap_zero(void)
{
    ReaderState rs;
    long hist[1];
    char out[8];

    breadcrumb_geom();
    hist[0] = 0;
    memset(&rs, 0, sizeof(rs));
    rs.history = hist;
    rs.hist_count = 1;
    rs.file_size = 1000L;
    build_breadcrumb(&rs, out, 0);
    setd(out);
    check(out[0] == '\0', "breadcrumb: cap=0 yields empty trail");
}

/* ==================================================================
   Negative controls
   These deliberately exercise invalid / edge inputs.  They exist to
   prove the assertions can fail: if the harness could not detect a
   mismatch, the suite would report a false green.
   ================================================================== */
static void test_negative_controls(void)
{
    ReaderState rs;
    char buf[PAGE_BUF_SIZE];

    /* Invalid UTF-8 must be tolerated, not crash. */
    cfg.screen_cols = 10;
    cfg.screen_lines = 4;
    write_file("tests\\t8.txt", "\xC3\x28", 2);
    memset(&rs, 0, sizeof(rs));
    io_init(&rs, "tests\\t8.txt");
    setd("read_page must not crash on invalid UTF-8");
    check(read_page(&rs, buf) >= 0, "negative: invalid UTF-8 tolerated");
    io_done(&rs);

    /* A deliberately wrong expectation must evaluate false. */
    setd("expected false");
    check((cfg.screen_lines == 9999) == 0, "negative: wrong expectation detected");

    /* A corrupt progress file must be rejected (not silently loaded). */
    {
        ReaderState rs2;
        long hist[2];
        hist[0] = 0; hist[1] = 100;
        strcpy(rs2.progfile_base, "tests\\neg");
        rs2.history = hist; rs2.hist_count = 2; rs2.hist_capacity = 2;
        rs2.file_size = 1000L; rs2.fp = NULL; rs2.offset = 100L;
        remove("tests\\neg.95r");
        save_progress(&rs2);
        {
            FILE *f = fopen("tests\\neg.95r", "r+b");
            if (f) {
                unsigned char b = 0;
                fseek(f, 6L, SEEK_SET);
                if (fread(&b, 1, 1, f) == 1) {
                    b = (unsigned char)(b ^ 0xFF);
                    fseek(f, 6L, SEEK_SET);
                    fwrite(&b, 1, 1, f);
                }
                fclose(f);
            }
        }
        memset(&rs2, 0, sizeof(rs2));
        strcpy(rs2.progfile_base, "tests\\neg");
        rs2.file_size = 1000L;
        load_progress(&rs2);
        setd("corrupt file must not load");
        check(rs2.hist_count == 0, "negative: corrupt sidecar rejected");
        if (rs2.history) { free(rs2.history); rs2.history = NULL; }
    }
}

/* ==================================================================
   Harness self-checks
   ================================================================== */
static void test_harness_selfcheck(void)
{
    /* The results file must be open, otherwise nothing was recorded. */
    setd("results file should be open");
    check(res != NULL, "harness: results file is open");

    /* Counters must stay non-negative (a negative count means corruption). */
    setd("counts must be non-negative");
    check(pass_count >= 0 && fail_count >= 0 && checks_run >= 0,
          "harness: counters are consistent");
}

/* Final sentinel: its presence in results.txt proves the harness reached
   the end of the suite.  The host script checks for this line. */
static void test_end_sentinel(void)
{
    check(1, "harness: end sentinel");
}

/* ==================================================================
   main
   ================================================================== */
int main(void)
{
    setbuf(stdout, NULL);   /* unbuffered so output survives a crash */
    res = fopen(RESULTS_FILE, "w");
    printf("95read test harness\n");
    if (res) fprintf(res, "95read test harness\n");

    /* config */
    test_config_defaults();
    test_config_parse();
    test_config_out_of_range();
    test_config_comments();

    /* progress */
    test_progress_roundtrip();
    test_progress_checksum();
    test_progress_legacy();
    test_progress_custom_ext();
    test_progress_empty();

    /* fileio */
    test_fileio_basic();
    test_fileio_wordwrap();
    test_fileio_crlf();
    test_fileio_utf8();
    test_fileio_cp1250();
    test_fileio_bom();
    test_fileio_eof();

    /* display */
    test_display_simple();
    test_display_wrap();
    test_display_tab();
    test_display_backspace();
    test_display_formfeed();

    /* utf-8 read boundary */
    test_utf8_boundary();

    /* breadcrumb */
    test_breadcrumb_empty();
    test_breadcrumb_single();
    test_breadcrumb_multi();
    test_breadcrumb_cap();
    test_breadcrumb_max_entries();
    test_breadcrumb_clamp();
    test_breadcrumb_cap_zero();

    /* false-green protection */
    test_negative_controls();
    test_harness_selfcheck();
    test_end_sentinel();

    /* Completeness guard: every planned check must have executed. */
    if (checks_run != PLANNED_CHECKS) {
        printf("INCOMPLETE RUN: %d of %d checks executed\n", checks_run, PLANNED_CHECKS);
        if (res) fprintf(res, "INCOMPLETE RUN: %d of %d checks executed\n", checks_run, PLANNED_CHECKS);
    }

    printf("\n==== %d passed, %d failed (%d checks) ====\n",
           pass_count, fail_count, checks_run);
    if (res) {
        fprintf(res, "\n==== %d passed, %d failed (%d checks) ====\n",
                pass_count, fail_count, checks_run);
        fclose(res);
        res = NULL;
    }
    return (fail_count > 0) ? 1 : 0;
}
