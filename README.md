# fast-tfm v3.0  —  Terminal File Manager

A feature-complete, dual-pane TUI file manager built with **C++17** and **FTXUI**.

---

## Features

| Category | Feature |
|---|---|
| **Navigation** | Dual panes, breadcrumb bar, history back/forward (Alt+←/→), bookmarks |
| **File ops** | Copy, cut, paste (multi-file), rename, mkdir (nested), touch, symlink, chmod |
| **Deletion** | Soft-trash with undo (`d` / `u`), permanent delete (`D`) |
| **Viewing** | Normal and detail (size + date) view modes, live file preview with line/word count |
| **Selection** | Space-toggle multi-select, select-all, bulk copy/cut/trash/delete |
| **Search** | Live filter (`/`), recursive find overlay (`f`) |
| **Sorting** | 7-mode cycle: Name↑, Name↓, Size↑, Size↓, Date↑, Date↓, Ext |
| **Pane sync** | Mirror-navigation mode (`z`), swap panes (`w`) |
| **External** | Open in `$EDITOR`, xdg-open, open terminal (`t`), yank path to clipboard (`Y`) |
| **Persistence** | Bookmarks saved to `~/.config/fast_tfm/bookmarks` |
| **Color coding** | 8 file-type categories with distinct colors |

---

## Build

### Prerequisites

- C++17 compiler (GCC ≥ 8, Clang ≥ 7, MSVC 2019+)
- CMake ≥ 3.15
- FTXUI (auto-fetched if not installed)

### Quick start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./fast-tfm
```

### Manual (no CMake)

```bash
# Install FTXUI first, e.g.:  sudo apt install libftxui-dev
g++ -std=c++17 -O2 fast-tfm.cpp \
    -lftxui-component -lftxui-dom -lftxui-screen \
    -o fast-tfm
```

---

## Keybindings

### Navigation
| Key | Action |
|---|---|
| `↑↓` / `k j` | Move cursor |
| `PgUp` / `PgDn` | Jump 10 entries |
| `Home` / `End` | First / last entry |
| `Enter` | Open directory or edit file |
| `Backspace` | Go to parent |
| `Alt+←` / `Alt+→` | Navigate history |
| `~` | Jump to `$HOME` |
| `g` | Go to typed path |
| `Tab` | Switch active pane |

### File Operations
| Key | Action |
|---|---|
| `c` | Copy to clipboard |
| `x` | Cut to clipboard |
| `p` | Paste clipboard here |
| `d` | Move to trash (undo with `u`) |
| `D` | Permanently delete |
| `u` | Restore last trashed item |
| `r` | Rename |
| `m` | New directory (nested OK) |
| `n` | New file (touch) |
| `l` | Create symlink from clipboard |
| `P` | Change permissions (chmod) |
| `S` | Calculate directory size |
| `Y` | Yank path to system clipboard |

### View & Search
| Key | Action |
|---|---|
| `v` | Toggle Normal / Detail view |
| `s` | Cycle sort mode |
| `.` | Toggle hidden files |
| `/` | Live filter |
| `f` | Recursive find overlay |
| `Space` | Toggle multi-select |
| `A` | Select all / deselect all |

### Other
| Key | Action |
|---|---|
| `e` | Open in `$EDITOR` explicitly |
| `o` | Open with `xdg-open` |
| `t` | Open terminal here |
| `z` | Toggle pane sync |
| `w` | Swap pane directories |
| `b` | Bookmarks panel |
| `B` | Bookmark current directory |
| `Ctrl+R` | Force refresh |
| `?` | Help overlay |
| `q` | Quit |

---

## Configuration

All config lives in `~/.config/fast_tfm/`:

| Path | Contents |
|---|---|
| `bookmarks` | One absolute path per line |
| `trash/` | Soft-deleted files (prefixed with Unix timestamp) |
