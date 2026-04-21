# Test Plan

## 1. Objectives

- Validate core read-only viewing workflows for PDF/text.
- Validate usability and correctness of search, TOC, print, and help/about.
- Prevent regressions across CI platform matrix.

## 2. Test Scope

### In Scope
- File browsing + preview routing
- PDF navigation/search/TOC
- Print workflows (raster + native handoff)
- Clipboard sanitization
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

1. **Preview routing**: select PDF/text/unsupported and verify expected page/widget.
2. **PDF navigation**: first/prev/next/last + page input valid/invalid behavior. **First/last page**: confirm **Ctrl+Home** / **Ctrl+End** (where applicable) and **Cmd+Up** / **Cmd+Down** (**Meta+Up** / **Meta+Down**) on macOS; tooltips should show native shortcut text for both bindings.
3. **Search**: query -> auto-jump first result -> next/previous traversal.
4. **TOC**: tab enablement by context + bookmark activation jumps.
5. **Print (raster)**: 300/600 DPI + page range output.
6. **Print (native)**: open in system viewer and proceed with native print controls.
7. **Clipboard**: copied content pastes without background style artifacts.
8. **Help/About**: dialog visibility/content/link checks.
9. **File reload**: modify currently previewed file and verify debounced reload.

## 6. Exit Criteria

- All critical scenarios pass on at least one local platform.
- CI matrix for tag release is green.
- No blocker/high severity defects open for release scope.
