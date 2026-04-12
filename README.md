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
  - **View → PDF pages** (and the **PDF navigation** toolbar): first / previous / next / last page, a **page counter**, and shortcuts (**Ctrl+Home**, **Alt+PgUp**, **Alt+PgDown**, **Ctrl+End**). The status bar summarizes navigation when a PDF is open.
  - **Disk changes:** the file shown in the preview (PDF or text) is watched with **`QFileSystemWatcher`**. If it changes on disk, the preview **reloads** after a short debounce. If it **disappears** (deleted or renamed away), the preview shows that the file is **no longer available**.

## Requirements

- **CMake** 3.16 or newer  
- **Qt 6** with these modules available to CMake: **Core**, **Gui**, **Widgets**, **Pdf**, **PdfWidgets**  
  - Qt **6.4+** is required by `CMakeLists.txt` (PDF APIs used here); some distributions package PDF widgets separately—if CMake cannot find `Qt6::PdfWidgets`, install your OS “Qt6 PDF” development packages or point `CMAKE_PREFIX_PATH` at a full Qt kit from the Qt Online Installer.
- A **C++17** toolchain

### Optional tooling (Homebrew, Chocolatey, npm)

You can install **CMake**, **Ninja**, **Qt 6**, and compilers through a package manager instead of (or in addition to) distro installers or the Qt Online Installer:

| Manager | Typical use | Notes |
|--------|-------------|--------|
| **[Homebrew](https://brew.sh/)** (macOS / Linux) | `brew install cmake ninja qt` (or `qt@6`), then set **`CMAKE_PREFIX_PATH`** to **`$(brew --prefix qt)`** (see [Platform notes](#platform-notes)) | A **`Brewfile`** can live in the repo for **`brew bundle`**. **`Brewfile.lock.json`** is **gitignored** here so it is not committed by default; remove that line from **`.gitignore`** if you want a pinned Homebrew snapshot in Git. |
| **[Chocolatey](https://chocolatey.org/)** (Windows) | e.g. **`choco install cmake ninja visualstudio2022buildtools`** — match whatever matches your MSVC / Qt layout | Local **`choco pack`** output (**`*.nupkg`**) at the repo root is **gitignored**. |
| **[npm](https://www.npmjs.com/)** | Only if you add **Node-based scripts** (release automation, lint, etc.) beside the Qt app | **`node_modules/`** and common npm/yarn/pnpm debug logs are **gitignored**. **`package.json` / `package-lock.json`** are not ignored so you can lock script dependencies when you add them. |

The application itself stays **C++/Qt**; these entries are for **developer environments** and optional tooling, not runtime requirements for end users of the built app.

## Build (Makefile orchestration)

The repository root **`Makefile`** is a thin wrapper around CMake.

```bash
make            # configure (if needed) + compile
make run        # build, then launch the app
make clean      # remove the build directory
```

### Continuous integration

[GitHub Actions](https://github.com/code-monki/skrat/actions/workflows/ci.yml) builds **`master`** / **`main`** on every push and pull request, and also runs when you push a **version tag** matching **`v*`** (for example **`v0.2.0`**). Workflow file: [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

| Where | Link |
|--------|------|
| **Stable downloads (tagged releases)** | [github.com/code-monki/skrat/releases](https://github.com/code-monki/skrat/releases) — see also [`releases/README.md`](releases/README.md) |
| Workflow runs (pick a green run) | [github.com/code-monki/skrat/actions/workflows/ci.yml](https://github.com/code-monki/skrat/actions/workflows/ci.yml) |
| All Actions activity | [github.com/code-monki/skrat/actions](https://github.com/code-monki/skrat/actions) |

**Pre-built bundles** from CI are attached as **Artifacts** (GitHub may wrap each file in a `.zip`). Open a run, scroll to **Artifacts**, and download the name that matches your platform. **Tagged `v*` builds** also publish the same portable archives on **Releases**.

Each artifact zip often contains **two files** where applicable:

- **`*-portable.tar.gz` / `*-portable.zip`**: Qt runtime bundled (**linuxdeploy** + Qt plugin on Ubuntu, **macdeployqt** on macOS, **windeployqt** on Windows). Use these on machines **without** a Qt SDK.
- **`skrat` / `skrat.exe`**: raw build output (still needs Qt libraries on the system, like a local compile).

| Artifact | Portable bundle | Raw binary |
|----------|-----------------|------------|
| `skrat-ubuntu-x86_64` | `skrat-ubuntu-x86_64-portable.tar.gz` — extract, run `AppDir/AppRun` | `skrat` |
| `skrat-ubuntu-aarch64` | same pattern | `skrat` |
| `skrat-fedora-x86_64` | — (Fedora job ships raw binary only) | `skrat` |
| `skrat-fedora-aarch64` | — | `skrat` |
| `skrat-macos-universal` | `skrat-macos-universal-portable.tar.gz` — extract `skrat.app` | — |
| `skrat-windows-x86_64` | `skrat-windows-x86_64-portable.zip` — extract, run `skrat.exe` | `skrat.exe` |

**Linux portable:** run **`./AppDir/AppRun`** from the extracted tree. **macOS:** CI does **not** notarize; Gatekeeper may prompt the first open. **Windows:** **SmartScreen** may warn for unsigned builds. For raw Unix binaries after unzip, **`chmod +x skrat`** may be needed.

### Troubleshooting: `Could not find a package configuration file provided by "Qt6"`

CMake only finds Qt when **`CMAKE_PREFIX_PATH`** points at a **Qt kit directory** (the folder that contains `lib/cmake/Qt6/Qt6Config.cmake`), for example **`$HOME/Qt/6.9.3/macos`**.

Fix it in one of these ways:

1. **Recommended:** copy `config.local.mk.example` to **`config.local.mk`** in the repo root (that file is **gitignored**) and set **`CMAKE_PREFIX_PATH`** there. Then run **`make`** again.
2. **One-shot:**  
   `make CMAKE_ARGS="-DCMAKE_PREFIX_PATH=$HOME/Qt/6.9.3/macos"`  
   (change **`macos`** to your real kit name if different; use **`ls "$HOME/Qt/6.9.3"`** to list kits.)
3. **Environment:**  
   `export CMAKE_PREFIX_PATH="$HOME/Qt/6.9.3/macos"`  
   then **`make`** (CMake reads this variable).

If you already configured without Qt, remove the stale build tree before reconfiguring: **`make clean`**, then **`make`**.

Optional knobs:

```bash
make BUILD_DIR=out
make GENERATOR=Ninja
make CMAKE_ARGS='-DCMAKE_BUILD_TYPE=Debug'
```

With **Ninja**, if the `ninja` binary is not on your `PATH`, point CMake at it (used only when `GENERATOR=Ninja`):

```bash
make GENERATOR=Ninja NINJA=/opt/homebrew/bin/ninja
# or, if your install keeps the binary under /opt/homebrew/ninja:
make GENERATOR=Ninja NINJA=/opt/homebrew/ninja/bin/ninja
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

If you use **`GENERATOR=Ninja`** and **`ninja`** is not on `PATH`, set **`NINJA`** to the full path to the `ninja` executable (for example under `/opt/homebrew/bin` or `/opt/homebrew/ninja/bin`). Uncomment the `GENERATOR` / `NINJA` lines in `config.local.mk.example` when you copy it to `config.local.mk`.

You can also invoke CMake directly:

```bash
/opt/homebrew/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.9.3/macos"
/opt/homebrew/bin/cmake --build build --parallel
```

### Platform notes

- **macOS (Homebrew Qt):** `brew install qt cmake` then use `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"` if `find_package(Qt6)` fails. Release builds use a **`skrat.app`** bundle (see **Run** below).
- **macOS (Qt Online Installer + Homebrew CMake):** use `-DCMAKE_PREFIX_PATH` pointing at `~/Qt/<ver>/<kit>` as above; use `config.local.mk` to avoid retyping.
- **Linux:** install Qt6 base **and** Qt6 PDF development packages for your distribution (names vary: `qt6-pdf-dev`, `qt6-pdf-widgets`, etc.).
- **Windows:** install Qt 6 with the **MSVC** or **MinGW** kit including **Qt PDF**; configure CMake from Qt Creator or a “x64 Native Tools” prompt with `CMAKE_PREFIX_PATH` pointing at your Qt `lib/cmake` parent (for example `...\6.x.x\msvc2019_64`).

## Run

After a successful build, the binary is typically:

- **macOS:** `build/skrat.app` (bundle) — run **`open build/skrat.app`** or **`build/skrat.app/Contents/MacOS/skrat`**
- **Linux:** `build/skrat`
- **Windows (single-configuration generators):** `build/Release/skrat.exe` or `build/Debug/skrat.exe`

The **`make run`** target opens **`build/skrat.app`** on macOS, else tries `build/skrat`, then `build/Release/skrat`, then `build/Debug/skrat`.

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

This application’s **source code** is licensed under the **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`). Qt is used under **your** Qt license terms (for example the open-source Qt LGPL/GPL offerings, or a commercial Qt license).

**Documentation** in this repository (including this `README.md`) is licensed under the [**Creative Commons Attribution-ShareAlike 4.0 International** license](https://creativecommons.org/licenses/by-sa/4.0/) (`CC-BY-SA-4.0`). The full legal text is in [`LICENSE-CC-BY-SA-4.0.txt`](LICENSE-CC-BY-SA-4.0.txt).
