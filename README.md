# skrat

`skrat` is a small **read-only desktop viewer** with a **two-pane layout**: a **file tree** on the left and a **preview** on the right. It is aimed at quickly checking **PDF layout** (for example, PDFs exported from **Playwright / Chromium**) and skimming **plain text** files.

This is **not** an editor.

## Features

- **Left tabbed view:** `Files` tab is the `QTreeView` file tree (`QFileSystemModel`); `TOC` tab shows PDF **Table of contents** (outline/bookmarks when available; placeholder when unavailable). The TOC tab is enabled only while previewing a PDF.
- **Right pane:** preview stack
  - **PDF** via Qt’s **`QtPdf` / `QPdfView`** (PDFium-based rendering).
  - **Text** for common extensions (UTF-8; see `MainWindow.cpp` for the allowlist).
- **Menus**
  - **File → Open Folder…** sets the tree root.
  - **File → Print PDF…** prints the currently selected/open PDF (**Ctrl+P**), with a pre-print options dialog: **Native PDF (vector, opened in system viewer for native print controls)** or **Rasterized by skrat** at **300/600 DPI**. Raster mode honors print dialog page ranges.
  - **Edit → Copy** copies selection from text/PDF; HTML payloads are sanitized to remove background color styling before paste (**Ctrl+C**).
  - **Edit → Find in PDF… / Find Next / Find Previous** searches in the active PDF (**Ctrl+F**, **F3**, **Shift+F3**). A **Find** toolbar (icon buttons + search field) shows the query and match count; hover for shortcuts and hints. On new queries, the preview auto-jumps to the first match. Search results are shown with rectangular background highlights; the loud "current result" frame is suppressed for a softer look.
  - **View → PDF Fit Width / Zoom In / Out** (shortcuts match the platform defaults where applicable).
  - **View → PDF pages** (and the **PDF** toolbars): **icon-only** controls with tooltips for page navigation (first / previous / next / last) plus a **page number input** between previous/next (press **Return** to jump; out-of-range values show a warning), along with **print** and **find**. The **page counter** is centered in the main bar over the **preview** (right) column so it lines up with the document, not the file tree. **Ctrl+G** on a PDF asks for a **page number**; on plain text it asks for a **line number** (text previews do not have PDF-style pages). The status bar summarizes navigation when a PDF is open.
  - **Help menu:** **Help / Shortcuts…** opens an in-app usage + shortcuts screen; **About skrat…** shows version, author/maintainer, license summary (including Qt licensing notice), and project link.
  - **Disk changes:** the file shown in the preview (PDF or text) is watched with **`QFileSystemWatcher`**. If it changes on disk, the preview **reloads** after a short debounce. If it **disappears** (deleted or renamed away), the preview shows that the file is **no longer available**.

## Requirements

- **CMake** 3.16 or newer  
- **Qt 6** with these modules available to CMake: **Core**, **Gui**, **Widgets**, **PrintSupport**, **Pdf**, **PdfWidgets**  
  - Qt **6.4+** is required by `CMakeLists.txt` (PDF APIs used here); some distributions package PDF/print modules separately—if CMake cannot find `Qt6::PdfWidgets` or `Qt6::PrintSupport`, install your OS Qt6 PDF/print development packages or point `CMAKE_PREFIX_PATH` at a full Qt kit from the Qt Online Installer.
- A **C++17** toolchain

### Optional tooling (Homebrew, Chocolatey, npm)

You can install **CMake**, **Ninja**, **Qt 6**, and compilers through a package manager instead of (or in addition to) distro installers or the Qt Online Installer:

| Manager | Typical use | Notes |
|--------|-------------|--------|
| **[Homebrew](https://brew.sh/)** (macOS / Linux) | `brew install cmake ninja qt` (or `qt@6`), then set **`CMAKE_PREFIX_PATH`** to **`$(brew --prefix qt)`** (see [Platform notes](#platform-notes)) | A **`Brewfile`** can live in the repo for **`brew bundle`**. **`Brewfile.lock.json`** is **gitignored** here so it is not committed by default; remove that line from **`.gitignore`** if you want a pinned Homebrew snapshot in Git. |
| **[Chocolatey](https://chocolatey.org/)** (Windows) | e.g. **`choco install cmake ninja visualstudio2022buildtools`** — match whatever matches your MSVC / Qt layout | Local **`choco pack`** output (**`*.nupkg`**) at the repo root is **gitignored**. |
| **[npm](https://www.npmjs.com/)** | Only if you add **Node-based scripts** (release automation, lint, etc.) beside the Qt app | **`node_modules/`** and common npm/yarn/pnpm debug logs are **gitignored**. **`package.json` / `package-lock.json`** are not ignored so you can lock script dependencies when you add them. |

The application itself stays **C++/Qt**; these entries are for **developer environments** and optional tooling, not runtime requirements for end users of the built app.

### Cursor agent sandbox (optional)

The repo ships **`.cursor/sandbox.json`**: **`workspace_readwrite`**, **`enableSharedBuildCache`**, extra **read/write** on **`/private/tmp`** and **`/tmp`**, and **read-only** **`/usr/bin`** + **`/System/Library`** so **`packaging/macos/build-icns.sh`** (**`sips`** / **`iconutil`**) can run under a sandboxed agent. If icon generation still fails, temporarily set **`"type": "insecure_none"`** in that file (see [Cursor sandbox docs](https://cursor.com/docs/reference/sandbox)).

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

- **`*-portable.tar.gz` / `*-portable.zip`**: Qt runtime bundled (**linuxdeploy** + Qt plugin on **Ubuntu** and **Fedora** CI, **macdeployqt** on macOS, **windeployqt** on Windows). Use these on machines **without** a Qt SDK.
- **`skrat` / `skrat.exe`**: raw build output (still needs Qt libraries on the system, like a local compile).

| Artifact | Portable bundle | Raw binary |
|----------|-----------------|------------|
| `skrat-ubuntu-x86_64` | `skrat-ubuntu-x86_64-portable.tar.gz` — extract, run `AppDir/AppRun` | `skrat` |
| `skrat-ubuntu-aarch64` | same pattern | `skrat` |
| `skrat-fedora-x86_64` | `skrat-fedora-x86_64-portable.tar.gz` — extract, run `AppDir/AppRun` | `skrat` |
| `skrat-fedora-aarch64` | same pattern | `skrat` |
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

### macOS app icon

The Dock / Finder icon comes from **`packaging/macos/skrat.icns`** and **`CFBundleIconFile`** in **`Info.plist`**. CMake runs **`packaging/macos/install-bundle-icon.sh`** on each link so **`Contents/Resources/skrat.icns`** exists and the plist is set (CMake’s default plist often left **`CFBundleIconFile`** empty). **`macdeployqt`** can strip that again — re-run:

```bash
bash packaging/macos/install-bundle-icon.sh build/skrat.app
```

After changing **`packaging/icons/`**, regenerate **`.icns`** on a Mac, then reinstall the icon:

```bash
./packaging/macos/build-icns.sh
bash packaging/macos/install-bundle-icon.sh build/skrat.app
```

If Finder still shows a generic icon, **`touch build/skrat.app`** or toggle the folder view so LaunchServices refreshes its cache.

### Linux portable icon

The vector source is **`packaging/icons/skrat.svg`** (acorn with two circular “tooth” bites—an homage to the Ice Age squirrel). CI bundles the pre-rendered **`packaging/icons/hicolor/*/apps/skrat.png`** tree. After editing the SVG, refresh the PNGs (needs **`rsvg-convert`** from librsvg, or **ImageMagick** `convert` / **`magick`** on `PATH`):

```bash
./packaging/icons/export-pngs.sh
```

## Run

After a successful build, the binary is typically:

- **macOS:** `build/skrat.app` (bundle) — run **`open build/skrat.app`** or **`build/skrat.app/Contents/MacOS/skrat`**
- **Linux:** `build/skrat`
- **Windows (single-configuration generators):** `build/Release/skrat.exe` or `build/Debug/skrat.exe`

The **`make run`** target opens **`build/skrat.app`** on macOS, else tries `build/skrat`, then `build/Release/skrat`, then `build/Debug/skrat`.

### macOS Gatekeeper (unsigned / CI builds)

CI and local builds are **not** signed with a **Developer ID** or **notarized**. Gatekeeper may show **“skrat.app” Not Opened** and *Apple could not verify … is free of malware*. That is normal for this distribution model until you add your own signing pipeline.

**First launch:** In **Finder**, **Control‑click (right‑click)** `skrat.app` → **Open** → confirm **Open** once. Alternatively open **System Settings → Privacy & Security**, attempt launch once, then use **Open Anyway** when macOS offers it.

**Downloads from GitHub** carry **quarantine** metadata. If you trust the artifact, you can clear it (advanced / at your own risk):

```bash
xattr -dr com.apple.quarantine /path/to/skrat.app
```

**Opening from Terminal:** use the **`.app` path**, not **`open -a skrat.app`** (the **`-a`** flag expects a registered **application name**, not a bundle filename). Examples:

```bash
open build/skrat.app
open build/skrat.app --args .
open build/skrat.app --args "$(pwd)"
```

### Apple Developer + CI (keep credentials out of Git)

Do **not** commit `.p12`, `.p8`, or passwords. Store them only as **GitHub Actions secrets** (Repository **Settings → Secrets and variables → Actions**). Optionally put those secrets on a protected **Environment** (e.g. only **`refs/tags/v*`** or manual approval) so ordinary branch runs never see them. When **all** required secrets are set, **`.github/workflows/ci.yml`** signs and notarizes **`skrat.app`** on the **macOS** job (see **`packaging/macos-ci-signing.md`** for exact secret names and troubleshooting).

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

## System Documentation

Project system/design/test documents live under [`docs/`](docs/README.md), including:
- SRS (with acceptance criteria)
- HLA
- DD
- RTM
- Test Plan
