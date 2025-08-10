95read – Updated Readme (HP 95LX, Turbo C 2.01)
A lightweight, Turbo C 2.01–based text viewer for the HP 95LX palmtop. 95read displays plain‑text files in fixed 16 × 40 pages, supports forward/back paging, goto by percentage, begin/end jumps, and saves your last‑read position. It includes minimal encoding handling (ASCII/CP1250/UTF‑8 → ASCII) for better readability of mixed files.
Features
    • Fixed‑size pages (defaults 16 lines × 40 columns), with configurable effective size via 95read.cfg
    • Navigation:
        ◦ N: next page
        ◦ P: previous page (history-based)
        ◦ G: goto percentage (0–100) of the file
        ◦ B: begin (start of file)
        ◦ E: end (last page)
        ◦ C or ESC: quit and save progress
    • Status footer:
        ◦ Shows current progress percentage and key hints on the last line
    • Progress saving with checksummed sidecar file:
        ◦ Writes .95r (or custom extension) with a compact, robust format
        ◦ Atomic save (temp file then rename) to avoid corruption
        ◦ Loads legacy format automatically if present
    • Encoding tolerance:
        ◦ Detects ASCII/UTF‑8/CP1250 per page and converts to ASCII in-place
        ◦ Safely handles partial UTF‑8 at page boundaries
    • Configurable keys and layout via 95read.cfg
    • Pure BIOS/conio I/O for keyboard and screen; no heavy dependencies
    • Modular C code, ANSI C89, small footprint for the HP 95LX
Requirements
    • Turbo C 2.01 (or equivalent DOS C compiler)
    • DOS environment (or DOSBox)
    • HP 95LX or compatible 16 × 40 text console emulator
Repository layout
95read/
├── include/
│   └── 95read.h        – Public headers & constants
├── src/
│   ├── main.c          – Program entry & control flow
│   ├── fileio.c        – File opening, sizing, encoding-aware page reads
│   ├── display.c       – Page rendering (reserves last line for status)
│   ├── ui.c            – Keyboard input (N/P/G/B/E/C, ESC)
│   ├── progress.c      – Load/save progress with checksum and atomic writes
│   └── config.c        – Robust config parsing (comments, quoted/hex keys)
└── build.bat           – Compile & link script for Turbo C
Building
    1. Install Turbo C 2.01 under C:\TC (or adjust paths in build.bat).
    2. Open DOS/DOSBox and navigate to the project root.
    3. Run: build.bat
Example build.bat (Turbo C 2.01):
@echo off
set TC=C:\TC
set INCLUDE=%TC%\INCLUDE
set LIB=%TC%\LIB

tcc -Ic:\tc\include -Lc:\tc\lib -ml -O -Z -G
tcc -ml -O -Z -G src\main.c src\fileio.c src\display.c src\ui.c src\progress.c src\config.c -e 95read.exe
    • Flags:
        ◦ -ml small model, -O optimize, -Z no stack overflow check (saves bytes), -G 80286 codegen (OK for 95LX’s 80C186).
Usage
From a DOS prompt on your HP 95LX (or emulator):
95read MYNOTES.TXT
Controls:
    • N: Next page
    • P: Previous page
    • G: Goto percentage (0–100)
    • B: Begin (start of file)
    • E: End (last page)
    • C or ESC: Quit (saves progress)
A sidecar file MYNOTES.95R (or custom extension) is created/updated to store your reading history and last offset.
Config file (95read.cfg)
    • Stored in the working directory by default, or set READ95CFG to a full path.
    • Comments (# or ;) allowed anywhere outside quotes.
    • Values accept names, hex (0xNN), decimal (27), or quoted chars ('N', '\n').
Keys:
    • screen_lines (4..100)
    • screen_cols (4..200)
    • tab_width (1..16)
    • key_next_page (char/name/hex/decimal/quoted)
    • key_prev_page (char/name/hex/decimal/quoted)
    • key_quit (char/name/hex/decimal/quoted)
    • prog_ext (include dot, up to 15 chars, e.g., .95r)
Tip: ESC always quits, regardless of key_quit.
Example 95read.cfg:
# HP95LX Reader Configuration
screen_lines   = 16
screen_cols    = 40
tab_width      = 8
key_next_page  = 'N'
key_prev_page  = 'P'
key_quit       = 'C'
prog_ext       = .95r
Notes and limits
    • HP 95LX 40×16 text mode is assumed; effective screen area may be adjusted via config, but buffers cap at compile-time limits to keep memory minimal.
    • End (E) navigates near the end by backing up one page from EOF, ensuring the last screen has content.
    • History is dynamic but capped when saved to keep .95r files small; duplicates are removed.

