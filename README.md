# skrat

`skrat` is a small **read-only desktop viewer** with a **two-pane layout**: a **file tree** on the left and a **preview** on the right. It is aimed at quickly checking **PDF layout** (for example, PDFs exported from **Playwright / Chromium**) and skimming **plain text** files.

This is **not** an editor.

## Features

- **Left pane:** `QTreeView` backed by `QFileSystemModel` (navigate folders, select files).
- **Right pane:** preview stack
  - **PDF** via Qt’s **`QtPdf` / `QPdfView`** (PDFium-based rendering).
  - **Text** for common extensions (UTF-8; see `MainWindow.cpp` for the allowlist).
- **Menus**
  - **File → Open Folder…** sets the tree root.
  - **View → PDF Fit Width / Zoom In / Out** (shortcuts match the platform defaults where applicable).

## Requirements

- **CMake** 3.16 or newer  
- **Qt 6** with these modules available to CMake: **Core**, **Gui**, **Widgets**, **Pdf**, **PdfWidgets**  
  - Qt **6.2+** is the project’s stated minimum; some distributions package PDF widgets separately—if CMake cannot find `Qt6::PdfWidgets`, install your OS “Qt6 PDF” development packages or point `CMAKE_PREFIX_PATH` at a full Qt kit from the Qt Online Installer.
- A **C++17** toolchain

## Build (Makefile orchestration)

The repository root **`Makefile`** is a thin wrapper around CMake.

```bash
make            # configure (if needed) + compile
make run        # build, then launch the app
make clean      # remove the build directory
```

Optional knobs:

```bash
make BUILD_DIR=out
make GENERATOR=Ninja
make CMAKE_ARGS='-DCMAKE_BUILD_TYPE=Debug'
```

If CMake cannot find Qt6, pass your Qt installation prefix (examples):

```bash
make CMAKE_ARGS='-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt'
# Qt Online Installer layout (version + kit directory, often `macos`)
make CMAKE_ARGS='-DCMAKE_PREFIX_PATH=$HOME/Qt/6.9.3/macos'
```

**Your setup (Qt 6.9.3 under `~/Qt`, CMake from Homebrew):** pick the kit folder that actually exists under `~/Qt/6.9.3/` (commonly `macos`; some installs use `macos_arm64`). It must be the directory that contains `lib/cmake/Qt6`.

```bash
ls "$HOME/Qt/6.9.3"
```

Then either pass flags once:

```bash
make CMAKE=/opt/homebrew/bin/cmake \
  CMAKE_ARGS="-DCMAKE_PREFIX_PATH=$HOME/Qt/6.9.3/macos"
```

…or copy the template and build normally:

```bash
cp config.local.mk.example config.local.mk
# edit CMAKE and CMAKE_PREFIX_PATH if your kit name or cmake path differs
make
```

If your `cmake` binary is not on `PATH`, set **`CMAKE`** to the full path (for example `/opt/homebrew/bin/cmake`, or `.../cmake/bin/cmake` under a custom prefix).

You can also invoke CMake directly:

```bash
/opt/homebrew/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.9.3/macos"
/opt/homebrew/bin/cmake --build build --parallel
```

### Platform notes

- **macOS (Homebrew Qt):** `brew install qt cmake` then use `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"` if `find_package(Qt6)` fails.
- **macOS (Qt Online Installer + Homebrew CMake):** use `-DCMAKE_PREFIX_PATH` pointing at `~/Qt/<ver>/<kit>` as above; use `config.local.mk` to avoid retyping.
- **Linux:** install Qt6 base **and** Qt6 PDF development packages for your distribution (names vary: `qt6-pdf-dev`, `qt6-pdf-widgets`, etc.).
- **Windows:** install Qt 6 with the **MSVC** or **MinGW** kit including **Qt PDF**; configure CMake from Qt Creator or a “x64 Native Tools” prompt with `CMAKE_PREFIX_PATH` pointing at your Qt `lib/cmake` parent (for example `...\6.x.x\msvc2019_64`).

## Run

After a successful build, the binary is typically:

- **macOS / Linux:** `build/skrat`
- **Windows (single-configuration generators):** `build/Release/skrat.exe` or `build/Debug/skrat.exe`

The `make run` target tries `build/skrat`, then `build/Release/skrat`, then `build/Debug/skrat`.

### Command-line arguments

You may pass **one** optional path:

- **Directory:** opens that folder as the tree root.
- **File:** sets the tree root to the file’s parent directory and selects the file (if it is visible under that root).

Examples:

```bash
./build/skrat .
./build/skrat ./101-cargos-101-travellers.pdf
```

## License

This application’s source is licensed under the **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`). Qt is used under **your** Qt license terms (for example the open-source Qt LGPL/GPL offerings, or a commercial Qt license).
