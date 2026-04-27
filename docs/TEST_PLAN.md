# Test Plan

## 1. Objectives

- Validate core read-only viewing workflows for PDF/HTML/Markdown/text/image.
- Validate usability and correctness of search, TOC, thumbnails, theme/font preferences, print, and help/about.
- Prevent regressions across CI platform matrix.

## 2. Test Scope

### In Scope
- File browsing + preview routing
- Tree context menu native-app handoff
- Tools menu CLI launcher installation
- Open With chooser and app-ranking persistence
- PDF navigation/search/TOC/thumbnails
- Print workflows (raster + native handoff)
- Clipboard sanitization
- Theme/font settings dialog behavior
- Help/About dialogs
- File watcher reload behavior

### Out of Scope
- Editing/writing source files (application is read-only)
- External printer driver defects

## 3. Test Environments

- macOS latest (local + CI)
- Ubuntu 24.04 (x86_64, aarch64)
- Fedora latest (x86_64, aarch64)
- Windows latest (x86_64)

## 4. Test Types

- Manual exploratory and scenario-based validation
- Build/packaging validation via CI
- Regression smoke per release tag

## 5. Core Test Scenarios

1. **Preview routing**: select PDF/text/image/unsupported and verify expected page/widget.
2. **PDF navigation**: first/prev/next/last + page input valid/invalid behavior. **First/last page**: confirm **Ctrl+Home** / **Ctrl+End** (where applicable) and **Cmd+Up** / **Cmd+Down** (**Meta+Up** / **Meta+Down**) on macOS; tooltips should show native shortcut text for both bindings.
3. **Search**: query -> auto-jump first result -> next/previous traversal.
4. **TOC**: tab enablement by context + bookmark activation jumps.
5. **Print (raster)**: 300/600 DPI + page range output.
6. **Print (native)**: open in system viewer and proceed with native print controls.
7. **Clipboard**: copied content pastes without background style artifacts.
8. **Help/About**: dialog visibility/content/link checks.
9. **File reload**: modify currently previewed file and verify debounced reload.
10. **Native app handoff**: right-click selected file and use **Open in Default App**; verify OS-associated application opens (`.docx`, `.xlsx`, `.pptx`, `.odt`, `.ods`, and unsupported preview formats).
11. **CLI launcher installer**: run **Tools → Install Command-Line Tool…**, verify launcher file created at platform path, execute `skrat <dir-or-file>` from terminal, and verify app opens expected root/selection.
12. **Open With chooser list**: open chooser for file type with multiple handlers and verify candidates render in expected order (most recently/frequently used first).
13. **Open With Other…**: choose **Other…**, select app executable/bundle, verify launch and persisted ranking update on subsequent chooser open.
14. **Open With fallback**: on environment where association discovery is partial, verify chooser still offers **Other…** and successful launch path.
15. **PDF thumbnails tab**: with active PDF, verify Thumbnails tab enables, displays one entry per page, and clicking a thumbnail jumps to expected page while current-page indicator updates.
16. **Theme/font preferences**: switch between System/Light/Dark/Warm Sepia and UI font settings, verify immediate chrome update, persistence across restart, and unchanged document rendering semantics.
17. **Basic SVG preview**: open simple SVG files and verify in-app rendering in image preview path; for malformed/unsupported SVGs verify clear fallback/error messaging and native-app handoff guidance.
18. **HTML/Markdown mode toggle**: open `.html`/`.htm` and `.md`/`.markdown`, verify default rendered Preview, toggle to Text source, and confirm mode persists after restart.
19. **Rendered preview link handling**: in rendered HTML/Markdown mode, click `http`/`https` links and verify system browser opens; click local file links and verify in-app navigation when target exists.
20. **Quick-view limitation disclosure**: verify docs/help text explicitly state rendered in-app HTML quick view may not faithfully support flexbox/grid layouts and recommend opening in external browser from file-tree context menu for full fidelity.

## 6. Exit Criteria

- All critical scenarios pass on at least one local platform.
- CI matrix for tag release is green.
- No blocker/high severity defects open for release scope.
