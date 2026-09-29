<p align="center">
  <img src="data/icons/hicolor/scalable/apps/notepadx.svg" width="128" height="128" alt="NotepadX Logo" />
</p>

<h1 align="center">NotepadX</h1>

<p align="center">
  <b>Fast, bloat-free text and code editor for Linux inspired by Notepad++.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/GTK-3.0-green.svg?style=flat-square" alt="GTK 3" />
  <img src="https://img.shields.io/badge/Editor-Scintilla%205.5-orange.svg?style=flat-square" alt="Scintilla" />
  <img src="https://img.shields.io/badge/Tests-38%2F38%20Passing-brightgreen.svg?style=flat-square" alt="Tests" />
  <img src="https://img.shields.io/badge/Platform-Wayland%20%7C%20X11-purple.svg?style=flat-square" alt="Platform" />
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg?style=flat-square" alt="License" />
</p>

---

## Overview

**NotepadX** is a native, distraction-free text and source code editor engineered for Linux. It combines the speed and responsiveness of classic desktop editors like Notepad++ with modern C++20 architecture and GTK 3 integration.

- **Instant startup**: Cold-launches in under **180 ms** without Electron or heavy runtime dependencies.
- **Resource hygiene**: Idles at **~23 MB PSS** memory, scaling gracefully even across 50+ open tabs.
- **Deep modules**: Built on proven editing foundations—**Scintilla 5.5** for high-performance text manipulation and **Lexilla** for syntax analysis.

---

## Verified Benchmarks

*All benchmarks measured on Linux 6.18 LTS (GCC 16.2, Release build). Ground truth logged in [`benchmarks/RESULTS.md`](benchmarks/RESULTS.md).*

| Metric | Target | Verified Result | Notes |
| :--- | :--- | :--- | :--- |
| **Cold Startup** | < 200 ms | **176.9 ms** | Realization, buffer allocation & frame paint |
| **Memory (Idle, 1 tab)** | < 25 MB | **23.29 MB PSS** | 49.00 MB RSS (shared libraries factored out) |
| **Memory (Heavy, 50 tabs)**| < 80 MB | **31.97 MB PSS** | 57.63 MB RSS |
| **File Load (10 MB file)** | < 500 ms | **403.8 ms** | Encoding conversion + Scintilla buffer paint |
| **Single-Instance IPC** | < 50 ms | **~38 ms** | Fast socket handoff to existing window |
| **Memory Safety** | 0 leaks | **0 leaks / 0 UB** | Tested under ASan and UBSan |

---

## Key Features

### ⚡ Editing Engine
- **Scintilla Core**: High-speed line layout, multi-selection, folding, rectangular blocks, and customizable line wrapping.
- **Atomic Undo Chunking**: Debounces continuous typing pauses (750 ms) and seals undo records at word/line boundaries to prevent single-character undo fatigue.
- **Jitter-Free Gutter**: Right-aligned line numbers with a 2-digit minimum baseline (`"99"`), preventing awkward viewport shifting between lines 1 and 99.
- **Subtle Line Bookmarks**: Click on line numbers or press `Ctrl+F2` to toggle bookmarks, rendered with clean translucent line highlights.

### 🎨 Syntax Highlighting (Lexilla)
- Native language recognition for **C/C++**, **Python**, **Rust**, **Go**, **JavaScript**, **TypeScript**, **Bash**, **Markdown**, **HTML**, **CSS**, **JSON**, **YAML**, **SQL**, and more.
- Built-in **Dark / Light theme** toggling with contrast-optimized color palettes.

### 📑 Tab Management
- **Gap-Safe Slot Recycling**: Seamlessly reclaims closed untitled slots (`New 1`, `New 2`, `New 3`).
- **Tab Bar Overflow Menu**: Dedicated `▼` button provides a searchable menu of all open documents.
- **Smooth Tab Navigation**: Rate-limited mouse wheel and trackpad scroll filtering prevents runaway tab cycling.
- **Duplicate Name Disambiguation**: Intelligently appends parent directory paths when files share the same filename.

### 🔍 Search & Replace
- **PCRE2 Regex Support**: Full Perl-compatible regular expressions with capture group substitutions (`$1`, `$2`).
- **Non-blocking Find Bar**: Incremental match highlighting, reverse search (`Shift+Enter`), whole word, and case sensitivity toggles.

### 🛡️ Persistence & Reliability
- **Atomic Autosave**: 500 ms debounced background autosaving via POSIX `.tmp` write and atomic `rename()` to eliminate data corruption risks during power outages or crashes.
- **Session Restoration**: Restores opened tabs, cursor offsets, and active tab index across reboots.
- **Encoding Auto-Detection**: Validates UTF-8, handles UTF-8 BOM, UTF-16LE, UTF-16BE, and gracefully converts legacy Latin-1 fallbacks.
- **Desktop Self-Sufficiency**: Native Wayland app ID mapping and embedded multi-resolution pixbufs (`16×16` through `256×256`) guarantee clean icon rendering on KDE, GNOME, X11, and standalone portable packages.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + N` | New Tab |
| `Ctrl + O` | Open File |
| `Ctrl + S` | Save File |
| `Ctrl + Shift + S` | Save As |
| `Ctrl + W` | Close Tab |
| `Ctrl + Shift + T` | Reopen Closed Tab |
| `Ctrl + Tab` / `Ctrl + Shift + Tab` | Next / Previous Tab |
| `Ctrl + F` | Find |
| `Ctrl + H` | Replace |
| `Ctrl + G` | Go to Line |
| `Ctrl + F2` | Toggle Bookmark |
| `F2` / `Shift + F2` | Next / Previous Bookmark |
| `Ctrl + D` | Duplicate Line |
| `Ctrl + Shift + K` | Delete Line |
| `Alt + Up` / `Alt + Down` | Move Line Up / Down |
| `Ctrl + /` | Toggle Comment |
| `Ctrl + +` / `Ctrl + -` | Zoom In / Out |

---

## Building from Source

### Prerequisites

#### Arch Linux / Manjaro
```bash
sudo pacman -S gtk3 pcre2 cmake gcc make pkgconf
```

#### Ubuntu / Debian / Linux Mint
```bash
sudo apt update
sudo apt install libgtk-3-dev libpcre2-dev cmake build-essential pkg-config
```

#### Fedora / RHEL
```bash
sudo dnf install gtk3-devel pcre2-devel cmake gcc-c++ make pkgconf
```

---

### Compilation

```bash
# 1. Clone the repository
git clone https://github.com/<username>/notepadx.git
cd notepadx

# 2. Configure build with optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build executable and tests
cmake --build build -j$(nproc)
```

### Running Tests

```bash
# Run unit test suite
ctest --test-dir build --output-on-failure

# Run comprehensive automated smoke test suite
./scripts/smoke_test.sh
```

### Installation

```bash
sudo cmake --install build
```

This installs:
- Executable: `/usr/local/bin/notepadx`
- Desktop Entry: `/usr/local/share/applications/notepadx.desktop`
- Application Icon: `/usr/local/share/icons/hicolor/scalable/apps/notepadx.svg`
- Man Page: `/usr/local/share/man/man1/notepadx.1`

---

## Project Structure

```
NotepadX/
├── benchmarks/         # Performance benchmarks and verification scripts
├── data/               # Desktop entry file and scalable SVG application icon
├── doc/                # Manual page (man 1 notepadx)
├── packaging/          # Distribution packaging specs (Arch PKGBUILD, etc.)
├── scripts/            # CI smoke tests and build helpers
├── src/
│   ├── app/            # Application lifecycle, MainWindow & menu wiring
│   ├── config/         # Settings persistence (JSON)
│   ├── document/       # Document model, slot recycling & metadata
│   ├── editor/         # ScintillaAdapter & high-level Editor abstraction
│   ├── ipc/            # UNIX domain socket single-instance messaging
│   ├── platform/       # Encoding detection & POSIX filesystem utilities
│   ├── search/         # PCRE2 regex engine & search bar controller
│   ├── session/        # Document store & autosave recovery
│   ├── syntax/         # Lexilla language manager & theme definitions
│   └── ui/             # TabBar, SearchBar, StatusBar, MenuBar components
├── tests/              # GoogleTest unit test suite (38 test fixtures)
└── third_party/        # Scintilla 5.5.4 and Lexilla 5.4.3 sources
```

---

## License

NotepadX is open-source software licensed under the **[GNU General Public License v3.0](LICENSE)**.
