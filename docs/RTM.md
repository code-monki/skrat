# Requirements Traceability Matrix (RTM)

| Requirement | Implementation Area | Verification |
|---|---|---|
| FR-001 File Navigation | `MainWindow::setRootFolder`, tree selection flow, tree context menu entrypoint | Manual: open folder + selection tests + context-menu visibility |
| FR-002 Preview Modes | `previewPath()` PDF/text/image/placeholder branches | Manual + regression smoke |
| FR-003 PDF Navigation | PDF toolbar actions, page input slot, go-to logic; first/last page actions use dual shortcuts (`Ctrl+Home`/`Ctrl+End` plus `Meta+Up`/`Meta+Down` for macOS-friendly Cmd+arrow binding) | Manual page-nav checklist; on macOS verify Cmd+Up/Cmd+Down and Ctrl+Home/Ctrl+End where hardware supports them |
| FR-004 PDF Search | find slots, search model integration, result status | Manual search scenarios |
| FR-005 TOC Navigation | `QPdfBookmarkModel`, TOC tab activation/jump | Manual bookmark navigation tests |
| FR-006 Print | `printCurrentPdf()` options + raster/native paths | Manual print matrix (mode/range/printer) |
| FR-007 Clipboard | `copyCurrentSelection()` sanitization flow | Manual copy/paste checks |
| FR-008 Help/About | help/about slots + menu actions | Manual dialog content checks |
| FR-009 Live Reload | watcher + debounce callbacks | Manual file-change reload scenarios |
| FR-010 Native App Handoff | tree context menu slot + `openPathInDefaultApp()` (`QDesktopServices::openUrl`) | Manual default-app open tests for supported and unsupported preview types |
| FR-011 CLI Launcher Installer | Tools menu action + `installCommandLineTool()` + launcher path/content helpers | Manual installer flow (file creation, executable bit/shim behavior, PATH guidance messaging) |
| NFR-001 Platform Support | CI workflow matrix + packaging scripts | CI run status and release artifacts |
| NFR-004 Maintainability | Header/function doc blocks + `docs/` set | Review checklist |
