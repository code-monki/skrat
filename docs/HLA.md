# High-Level Architecture (HLA)

## 1. Architectural Overview

`skrat` is a Qt Widgets desktop application using a single main window (`MainWindow`) that composes:
- Left tabbed panel (`Files`, `TOC`, `Thumbnails`)
- Right preview panel (PDF/text/image/placeholder stack)
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
  - Text view (`QPlainTextEdit`)
  - Image view (`QScrollArea` + `QLabel` pixmap, including basic SVG rasterized preview)
  - Placeholder (`QLabel` HTML)

### 2.4 PDF Services
- Page navigation via custom scene/scroll logic in `PdfGraphicsView`
- Search model (`QPdfSearchModel`)
- Bookmark model (`QPdfBookmarkModel`) for TOC
- Thumbnail list rendering and page-jump activation for PDF tab navigation
- Print mode selection (raster vs native handoff)

### 2.5 System Integration
- `QFileSystemWatcher` for content reload
- Native URI opening for vector print handoff and tree-level "Open in Default App"
- Platform CI packaging/deploy scripts

### 2.6 Open With Services
- **Association Provider:** platform-specific handler discovery for a file extension/MIME type.
- **Preference Store:** local persistence of per-filetype app usage (last-used + launch count).
- **Launcher Adapter:** platform-specific execution path for opening selected file with selected app.

### 2.7 UI Theme Services
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
