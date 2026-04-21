# Software Requirements Specification (SRS)

## 1. Introduction

### 1.1 Purpose
Define functional and non-functional requirements for `skrat`, a read-only desktop viewer for PDFs and text files.

### 1.2 Scope
`skrat` provides:
- File browsing via a tree view
- PDF preview, navigation, search, TOC browsing, and print workflows
- Text preview for common textual formats
- Basic help/about documentation in-app

### 1.3 Definitions
- **TOC**: Table of contents (PDF bookmarks/outline)
- **Raster print**: In-app rendering of PDF pages to images before print
- **Vector print handoff**: Opening PDF in system viewer for native print controls

## 2. Functional Requirements

### FR-001 File Navigation
- The system shall display a file tree rooted at a selected directory.
- The system shall allow changing root via Open Folder action.

### FR-002 Preview Modes
- The system shall preview PDF files in a PDF viewer.
- The system shall preview supported text files in read-only text view.
- The system shall show an explanatory placeholder for unsupported/unavailable files.

### FR-003 PDF Navigation
- The system shall support first/prev/next/last page actions.
- The system shall provide a page input field for direct jump with validation.
- The system shall provide Go To action for page/line contexts.

### FR-004 PDF Search
- The system shall allow searching within active PDF documents.
- The system shall navigate between search results (next/previous).
- The system shall auto-jump to the first match on new query results.

### FR-005 TOC Navigation
- The system shall show a TOC tab for PDF bookmark navigation.
- The system shall disable TOC tab when no PDF is active.
- The system shall jump to selected bookmark location/page when activated.

### FR-006 Print
- The system shall provide print options for:
  - Rasterized in-app printing at selectable DPI (300/600)
  - Native viewer handoff for vector-quality print workflows
- Raster print shall support page-range selection via print dialog.

### FR-007 Clipboard Behavior
- The system shall support copy from text/PDF contexts.
- The system shall sanitize copied HTML to remove background style artifacts.

### FR-008 Help and About
- The system shall provide an About dialog with version, maintainer, license summary, and project link.
- The system shall provide a Help/Shortcuts dialog with usage guidance and shortcut list.

### FR-009 Live Reload
- The system shall watch currently previewed file for on-disk changes and reload with debounce.

## 3. Non-Functional Requirements

### NFR-001 Platform Support
- macOS, Linux, and Windows builds shall be supported via CI workflows.

### NFR-002 Performance
- UI interactions (navigation/search actions) should be responsive for typical document sizes.

### NFR-003 Reliability
- Invalid operations (missing files, out-of-range page input, unsupported types) shall fail gracefully with clear user feedback.

### NFR-004 Maintainability
- Code should contain in-file API/function documentation for key modules.
- System docs in `docs/` shall be maintained with release updates.

## 4. Acceptance Criteria

### AC-001 File/Preview
- Given a selected directory, when user selects a supported PDF/text file, then corresponding preview appears.
- Given unsupported file, then placeholder explains unsupported type.

### AC-002 PDF Navigation
- Given an active PDF with N pages, when navigation controls are used, then current page changes correctly and page indicators update.
- Given page input outside [1..N], when submitted, then warning dialog appears and page does not jump.

### AC-003 Search
- Given an active PDF with matches, when query is entered, then first result is auto-focused.
- Given repeated next/previous actions, then navigation cycles across available matches.

### AC-004 TOC
- Given active PDF with bookmarks, TOC tab is enabled and displays entries.
- Given non-PDF active preview, TOC tab is disabled.
- Given bookmark activation, viewer jumps to expected destination.

### AC-005 Print
- Given raster print mode, print dialog supports page range and only selected pages print.
- Given native/vector mode, system viewer opens selected PDF for native print controls.

### AC-006 Help/About
- Help dialog displays usage + shortcuts.
- About dialog displays version, maintainer, license summary, and Qt licensing notice.

### AC-007 Documentation
- `docs/` includes SRS, HLA, DD, RTM, and Test Plan documents.
