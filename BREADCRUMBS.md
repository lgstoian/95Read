# BREADCRUMBS.md

Machine-readable navigation map for AI coding agents (and humans).
Every source file carries inline breadcrumb tags; this file is the index.

## Tag convention

All tags are `/* ... */` comments with a leading machine-readable key.
A tool can extract them with:

```
grep -rE '@(mod|region|breadcrumb|note):' include src
```

| Tag | Meaning | Placement |
|-----|---------|-----------|
| `@mod: <module>` | Module entry point (one per source file) | Top of each source file |
| `@region: <name>` | Logical region inside a module | Before a function/block |
| `@breadcrumb: <path>` | Navigation breadcrumb `module/function` or `module/data` | Before a function/block |
| `@note: <text>` | Concise factual note (replaces old `CHANGED` markers) | Inline |

## Module index

| Module | File | Public entry points | Breadcrumb root |
|--------|------|---------------------|-----------------|
| `main` | `src/main.c` | `main` | `main/entry` |
| `fileio` | `src/fileio.c` | `io_init`, `io_done`, `read_page` | `fileio/encoding` |
| `display` | `src/display.c` | `display_page`, `build_breadcrumb` | `display/render` |
| `ui` | `src/ui.c` | `ui_get_cmd` | `ui/input` |
| `progress` | `src/progress.c` | `load_progress`, `save_progress` | `progress/sidecar` |
| `config` | `src/config.c` | `load_config` | `config/load` |
| `95read.h` | `include/95read.h` | types, constants, interfaces | `95read/shared` |
| `config.h` | `include/config.h` | config struct + limits | `config/types` |

## Call / data flow

```
config.c: load_config()          -> fills global cfg
main.c:   main()                 -> load_config(); io_init(); load_progress()
main.c:   main() loop            -> read_page()  -> display_page()
                                  -> build_breadcrumb() -> draw_status()
                                  -> ui_get_cmd()  (blocks for a key)
fileio.c: read_page()            -> detect_encoding() + word-wrap
progress.c: save_progress()      -> checksum + atomic rename of sidecar
```

## Region index

| Region | File | Responsibility |
|--------|------|----------------|
| `video-attrs` | `src/main.c` | Text attributes, viewport helpers |
| `status` | `src/main.c` | Footer + breadcrumb rendering |
| `prompt` | `src/main.c` | Goto-percentage keyboard prompt |
| `test-screen` | `src/main.c` | Inversion test screen |
| `main-loop` | `src/main.c` | Command dispatch and navigation |
| `render` | `src/display.c` | Page buffer → text screen |
| `breadcrumb` | `src/display.c` | Navigation trail builder |
| `input` | `src/ui.c` | Key press → command translation |
| `sidecar` | `src/progress.c` | Progress file load/save |
| `config` | `src/config.c` | Config file parsing |
| `io` | `src/fileio.c` | File open/close, edge buffers |
| `encoding` | `src/fileio.c` | ASCII / UTF-8 / CP-1250 detection |
| `paging` | `src/fileio.c` | Page reads with word wrapping |

## UI breadcrumb

The reader shows a navigation breadcrumb trail in the status footer, built
by `display/build_breadcrumb()` from `ReaderState.history` (the most recent
`BREADCRUMB_MAX` page offsets, rendered as 1-based page numbers and joined
with `>`).  Example footer: `[  0% NRM] 2>3>3>4>4 N/P G B E I T C`.

## Tests

`tests/tmain.c` links the real modules and exercises them directly.  It also
contains the false-green guards: a planned-check count, an end sentinel, and
negative controls.  See `scripts/test.ps1`.
