# High-Level Architecture (HLA)

## 1. Architectural Overview

`skrat` is a Qt Widgets desktop application using a single main window (`MainWindow`) that composes:
- Left tabbed panel (`Files`, `TOC`)
- Right preview panel (PDF/text/image/placeholder stack)
- Menu bar, toolbars, status bar
- File-watcher-driven live reload loop

## 2. Main Components

### 2.1 UI Shell
- `MainWindow` orchestrates all top-level UI interactions and action routing.
- Menu actions and toolbar actions share slot handlers.
- `Tools` menu hosts command-line launcher installation flow.

### 2.2 File Browsing
- `QFileSystemModel` + `QTreeView`
- Root path managed by Open Folder command
- Tree context menu for native open handoff to OS-associated apps

### 2.3 Preview Engine
- `QStackedWidget` with:
  - PDF view (`PdfGraphicsView` + `QPdfDocument` + `QGraphicsScene` page items)
  - Text view (`QPlainTextEdit`)
  - Image view (`QScrollArea` + `QLabel` pixmap)
  - Placeholder (`QLabel` HTML)

### 2.4 PDF Services
- Page navigation via custom scene/scroll logic in `PdfGraphicsView`
- Search model (`QPdfSearchModel`)
- Bookmark model (`QPdfBookmarkModel`) for TOC
- Print mode selection (raster vs native handoff)

### 2.5 System Integration
- `QFileSystemWatcher` for content reload
- Native URI opening for vector print handoff and tree-level "Open in Default App"
- Platform CI packaging/deploy scripts

## 3. Runtime Interaction Summary

1. User selects file in tree.
2. `MainWindow::previewPath()` determines mode and loads renderer.
3. UI state update methods refresh actions/labels/tabs.
4. Optional operations (search/print/copy/TOC) operate on active preview context.

## 4. Cross-Cutting Concerns

- **Compatibility:** Qt 6.4+ API guards for search-related properties.
- **Usability:** Status messages, tooltips, in-app help/about.
- **Resilience:** Placeholder/error messaging for invalid states.
