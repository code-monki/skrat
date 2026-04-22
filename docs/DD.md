# Detailed Design (DD)

## 1. Key Classes and Responsibilities

### `MainWindow`
- Builds and wires all UI elements.
- Routes user actions to preview-specific logic.
- Maintains state for search index, current preview path, watch/debounce.
- Handles tree context menu handoff to OS-native apps.

### `QPdfDocument` / `QPdfView`
- Core PDF load and rendering backend.
- Navigation and viewport behavior.

### `QPdfSearchModel`
- Match extraction for active query.
- Used for next/previous navigation and result counting.

### `QPdfBookmarkModel`
- Supplies outline tree data to TOC tab view.

## 2. State Flows

### 2.1 Preview Loading
- Input path -> existence/type check -> PDF/text/image/placeholder branch.
- PDF path updates: document load, toolbar/tab state, watcher registration.
- Image path updates: image decode, image widget update, watcher registration.

### 2.2 Search Flow
- Query change updates search model.
- Model updates trigger first-match auto-selection and status refresh.
- Next/previous actions select indexed match and jump.

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

## 3. Error and Edge Handling

- Missing file -> placeholder.
- Invalid page input -> warning + reset.
- No search results -> informative status.
- Native print unavailable -> warning dialog.

## 4. Design Constraints

- Read-only application model (no source document mutation).
- Qt Widgets architecture retained (no QML migration).
- Maintain Qt 6.4+ compatibility.
