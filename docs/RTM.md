# Requirements Traceability Matrix (RTM)

| Requirement | Implementation Area | Verification |
|---|---|---|
| FR-001 File Navigation | `MainWindow::setRootFolder`, tree selection flow, tree context menu entrypoint | Manual: open folder + selection tests + context-menu visibility |
| FR-002 Preview Modes | `previewPath()` PDF/text/image/placeholder branches (including basic SVG via image preview path) | Manual + regression smoke |
| FR-003 PDF Navigation | PDF toolbar actions, page input slot, go-to logic; first/last page actions use dual shortcuts (`Ctrl+Home`/`Ctrl+End` plus `Meta+Up`/`Meta+Down` for macOS-friendly Cmd+arrow binding) | Manual page-nav checklist; on macOS verify Cmd+Up/Cmd+Down and Ctrl+Home/Ctrl+End where hardware supports them |
| FR-004 PDF Search | find slots, search model integration, result status | Manual search scenarios |
| FR-005 TOC Navigation | `QPdfBookmarkModel`, TOC tab activation/jump | Manual bookmark navigation tests |
| FR-006 Print | `printCurrentPdf()` options + raster/native paths | Manual print matrix (mode/range/printer) |
| FR-007 Clipboard | `copyCurrentSelection()` sanitization flow | Manual copy/paste checks |
| FR-008 Help/About | help/about slots + menu actions | Manual dialog content checks |
| FR-009 Live Reload | watcher + debounce callbacks | Manual file-change reload scenarios |
| FR-010 Native App Handoff | tree context menu slot + `openPathInDefaultApp()` (`QDesktopServices::openUrl`) | Manual default-app open tests for supported and unsupported preview types |
| FR-011 CLI Launcher Installer | Tools menu action + `installCommandLineTool()` + launcher path/content helpers | Manual installer flow (file creation, executable bit/shim behavior, PATH guidance messaging) |
| FR-012 Open With App Chooser | tree context Open With action + chooser UI + app candidate list assembly | Manual chooser display tests per file type and platform |
| FR-013 Open With Preference Persistence | local preference storage + candidate sorting by last-used/frequency | Manual repeat-launch ordering tests across app restarts |
| FR-014 Cross-Platform Fallback Behavior | best-effort association provider + always-available `Other…` path | Manual fallback tests on platforms with limited association enumeration |
| FR-015 PDF Thumbnails Navigation | left tab `QListWidget` thumbnail rendering + activation jump + page sync | Manual thumbnail rendering/activation tests across short and multi-page PDFs |
| FR-016 UI Theme and Font Preferences | `UiTheme` preference load/save/apply + Theme Settings dialog (`Tools`) | Manual theme/font persistence and chrome-only scope tests |
| NFR-001 Platform Support | CI workflow matrix + packaging scripts | CI run status and release artifacts |
| NFR-004 Maintainability | Header/function doc blocks + `docs/` set | Review checklist |
