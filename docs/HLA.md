# High-Level Architecture (HLA)

## 1. Architectural Overview

`skrat` is a Qt Widgets desktop application using a single main window (`MainWindow`) that composes:
- Left tabbed panel (`Files`, `TOC`, `Thumbnails`)
- Right preview panel (PDF/rendered-rich-text/text/image/placeholder stack)
- Menu bar, toolbars, status bar
- File-watcher-driven live reload loop

## 2. Main Components

### 2.1 UI Shell
- `MainWindow` orchestrates all top-level UI interactions and action routing.
- Menu actions and toolbar actions share slot handlers.
- `Tools` menu hosts theme settings and command-line launcher installation flows.

### 2.2 File Browsing
- `QFileSystemModel` + `QTreeView`
- Root path managed by Open Folder command
- Tree context menu for native open handoff to OS-associated apps
- Tree context menu for **Open With…** chooser (best-effort app discovery + manual app selection)

### 2.3 Preview Engine
- `QStackedWidget` with:
  - PDF view (`PdfGraphicsView` + `QPdfDocument` + `QGraphicsScene` page items)
  - Rich preview view (`QTextBrowser`) for rendered HTML/Markdown
  - Text view (`QPlainTextEdit`)
  - Image view (`QScrollArea` + `QLabel` pixmap, including basic SVG rasterized preview when SVG is in **preview** mode)
  - Placeholder (`QLabel` HTML)
- HTML/Markdown mode controls (`Preview`/`Text`) shown only for `.html`/`.htm`/`.md`/`.markdown` and persisted in local settings (`preview/richModeRendered`).
- SVG (`.svg`) mode controls (**SVG preview** /**SVG source**) shown only for that suffix: raster preview vs XML source in the text view; preference persisted (`preview/svgModeRendered`).

### 2.4 Document find (unified find bar)
- Single **Find** toolbar and **Edit → Find…** / next / previous actions apply to whichever searchable preview is active:
  - **PDF:** `QPdfSearchModel` + highlights/jumps in `PdfGraphicsView`.
  - **Plain text** (`QPlainTextEdit`) and **rendered HTML/Markdown** (`QTextBrowser`): scan `QTextDocument` for substring matches; show match index and scroll selection without stealing focus from the find field while typing.
- Image preview (including SVG **preview** mode) and placeholder are not searchable in-app.

### 2.5 PDF Services
- Page navigation via custom scene/scroll logic in `PdfGraphicsView`
- Search model (`QPdfSearchModel`) used only when the PDF stack page is active
- Bookmark model (`QPdfBookmarkModel`) for TOC
- Thumbnail list rendering and page-jump activation for PDF tab navigation
- Print mode selection (raster vs native handoff)

### 2.6 System Integration
- `QFileSystemWatcher` for content reload
- Native URI opening for vector print handoff and tree-level "Open in Default App"
- Native URI opening for external links clicked in rendered HTML/Markdown preview
- Platform CI packaging/deploy scripts

### 2.7 Open With Services
- **Association Provider:** platform-specific handler discovery for a file extension/MIME type.
- **Preference Store:** local persistence of per-filetype app usage (last-used + launch count).
- **Launcher Adapter:** platform-specific execution path for opening selected file with selected app.

### 2.8 UI Theme Services
- **Theme Preference Store:** local persistence of selected theme mode + UI font preferences.
- **Theme Applicator:** applies palette/font to Qt Widgets chrome while preserving document rendering content.

## 3. Runtime Interaction Summary

1. User selects file in tree.
2. `MainWindow::previewPath()` determines mode and loads renderer.
3. UI state update methods refresh actions/labels/tabs.
4. Optional operations (search/print/copy/TOC) operate on active preview context.

For **Open With…**:
1. User requests Open With for selected file.
2. Association Provider returns discovered app candidates (best effort).
3. Preference Store reorders candidates by local usage metadata.
4. User chooses candidate or **Other…** (manual executable/bundle path).
5. Launcher Adapter opens file via selected app and updates Preference Store.

## 4. Cross-Cutting Concerns

- **Compatibility:** Qt 6.4+ API guards for search-related properties.
- **Usability:** Status messages, tooltips, in-app help/about.
- **Resilience:** Placeholder/error messaging for invalid states.
- **Portability:** Open With candidate enumeration is intentionally best-effort across desktop environments.
