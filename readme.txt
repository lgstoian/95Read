95read
A lightweight, Turbo C 2.0–based text viewer for the HP 95LX palmtop. 95read displays plain‐text files in fixed 16 × 40 pages, supports forward/back paging, and saves your last‐read position.

Features :
Fixed‐size pages (16 lines × 40 columns) with no scrolling

Forward (N) and backward (P) page navigation

Quit (C) command to exit gracefully

Automatic uppercase mapping of input keys

Progress saving (.95r sidecar file) to remember your last offset

Pure BIOS keyboard polling—no conio.h or stdio.h input dependencies

Modular C code: fileio.c, display.c, ui.c, progress.c

Requirements
Turbo C 2.0 (or equivalent DOS C compiler)
OSBox or native DOS environment
HP 95LX or compatible 16 × 40 text console emulator

Repository Layout :
95read/
├── include/

│   └── 95read.h        – Public headers & constants

├── src/

│   ├── main.c          – Program entry & control flow

│   ├── fileio.c        – File opening, sizing, page‐buffer reads

│   ├── display.c       – 16×40 page rendering (handles CR/LF)

│   ├── ui.c            – BIOS INT 16h keyboard polling

│   └── progress.c      – Load/save byte‐offset progress file

└── build.bat           – Compile & link script for Turbo C

Building
Install Turbo C 2.0 under C:\tc (or update paths in build.bat).
Open a DOS prompt (or DOSBox) and navigate to the project root.
Run: build.bat

Usage :
From a DOS prompt on your HP 95LX (or emulator):
95read mynotes.txt

Controls:
Press N to advance to the next page
Press P to return to the previous page
Press C to close the reader and save progress
A sidecar file mynotes.95r is created/updated to store your last reading offset.