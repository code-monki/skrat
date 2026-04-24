# Detailed Design (DD)

## 1. Key Classes and Responsibilities

### `MainWindow`
- Builds and wires all UI elements.
- Routes user actions to preview-specific logic.
- Maintains state for search index, current preview path, watch/debounce.
- Handles tree context menu handoff to OS-native apps.
- Hosts Open With chooser entrypoint and delegates app selection/launch logic.

### `QPdfDocument` / `PdfGraphicsView`
- Core PDF load backend (`QPdfDocument`) with custom rendering in `PdfGraphicsView`.
- Deterministic scroll/jump behavior using scene/page geometry instead of `QPdfView` defaults.

### `QPdfSearchModel`
- Match extraction for active query.
- Used for next/previous navigation and result counting.

### `QPdfBookmarkModel`
- Supplies outline tree data to TOC tab view.

### `PdfGraphicsView`
- Renders PDF pages to `QGraphicsPixmapItem` instances.
- Draws search overlays from `QPdfSearchModel` result rectangles.
- Maintains current-page/current-search-result state and jump behavior.

### `OpenWith` support components (planned)
- **Association discovery helper:** platform-specific discovery of candidate apps for file type.
- **Usage preference store:** persisted local ranking metadata per extension.
- **App launcher helper:** executes selected app with selected file path safely.

## 2. State Flows

### 2.1 Preview Loading
- Input path -> existence/type check -> PDF/text/image/placeholder branch.
- PDF path updates: document load, toolbar/tab state, watcher registration.
- Image path updates: image decode, image widget update, watcher registration.

### 2.2 Search Flow
- Query change updates search model.
- Model updates trigger first-match auto-selection and status refresh.
- Next/previous actions select indexed match and jump via `PdfGraphicsView::jumpTo()`.

### 2.3 Print Flow
- Print options dialog chooses mode:
  - Raster: Qt print dialog + optional page range + DPI render
  - Native: open PDF in system viewer for native print controls

### 2.4 TOC Flow
- TOC tab enabled only when PDF active.
- Bookmark row activation jumps to (page, location, zoom).

### 2.5 PDF first/last page shortcuts (cross-platform)
- **First page** and **Last page** actions (`m_pdfActFirst`, `m_pdfActLast`) each register **two** key sequences via `QAction::setShortcuts`:
  - **Ctrl+Home** / **Ctrl+End** — conventional on Windows and Linux; on macOS these keys are often absent or awkward on laptop keyboards.
  - **Meta+Up** / **Meta+Down** — interpreted as **Cmd+Up** / **Cmd+Down** on macOS (`QKeySequence::NativeText` in tooltips reflects the platform).
- Both bindings invoke the same slots (`pdfGoFirstPage`, `pdfGoLastPage`); behavior is identical. Tooltips list both shortcuts so testers can confirm parity without reading source.

### 2.6 Tree context menu handoff
- Right-click in the `Files` tree opens a context menu with:
  - `Open in skrat` (existing internal preview routing)
  - `Open in Default App` (calls `QDesktopServices::openUrl` on the selected local path)
- Handoff failures produce warning dialogs; success posts a short status message.

### 2.7 CLI launcher installation flow
- `Tools → Install Command-Line Tool…` computes platform-specific launcher target path.
- Installer writes a wrapper script/shim that forwards CLI args to the app executable.
- UI reports success and whether install directory is present on current `PATH`.

### 2.8 Open With chooser + persistence flow
- User triggers **Open With…** on selected file.
- App discovers candidate handlers for extension/MIME (best effort per platform).
- App loads local usage metadata and reorders candidate list:
  1) most recently used,
  2) higher frequency,
  3) platform-discovered fallback order.
- Chooser always appends **Other…**.
- If **Other…** is selected, app-pick dialog returns executable/bundle path; app attempts launch and persists usage on success.

## 3. Error and Edge Handling

- Missing file -> placeholder.
- Invalid page input -> warning + reset.
- No search results -> informative status.
- Native print unavailable -> warning dialog.
- Open With launch failure -> warning dialog with app path and file path context.
- Association enumeration unavailable -> show **Other…** fallback without hard failure.

## 4. Design Constraints

- Read-only application model (no source document mutation).
- Qt Widgets architecture retained (no QML migration).
- Maintain Qt 6.4+ compatibility.
- Open With ranking persistence remains local-only (no cloud sync/telemetry dependency).
