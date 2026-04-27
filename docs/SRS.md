# Software Requirements Specification (SRS)

## 1. Introduction

### 1.1 Purpose
Define functional and non-functional requirements for `skrat`, a read-only desktop viewer for PDFs, rendered/source HTML/Markdown, common text files, and common images.

### 1.2 Scope
`skrat` provides:
- File browsing via a tree view
- PDF preview, navigation, PDF search, TOC browsing, and print workflows
- In-document search for plaintext and rendered HTML/Markdown previews, and for SVG shown as XML source
- HTML/Markdown preview with a user-selectable rendered-vs-source mode
- Text preview for common textual formats
- Image preview for common raster formats and basic SVG handling (raster preview and optional XML **source** view)
- File-tree context action to open selected files in the OS default app
- In-app installer for a command-line launcher utility (`skrat`)
- Basic help/about documentation in-app

### 1.3 Product Scope Guardrails
- `skrat` is a **read-only triage viewer**, not a document editor.
- In scope: fast preview, navigation, search, TOC/thumbnails, print, copy sanitation, and native-app handoff.
- Out of scope: authoring-grade document workflows that modify or enrich document semantics.
- Out-of-scope examples include (non-exhaustive): fillable form editing/submission workflows, annotation/markup authoring, digital-signature authoring flows, and redaction editing workflows.
- When those capabilities are needed, the expected path is handoff through **Open in Default App** or **Open With…**.

### 1.4 Definitions
- **TOC**: Table of contents (PDF bookmarks/outline)
- **Raster print**: In-app rendering of PDF pages to images before print
- **Vector print handoff**: Opening PDF in system viewer for native print controls

## 2. Functional Requirements

### FR-001 File Navigation
- The system shall display a file tree rooted at a selected directory.
- The system shall allow changing root via Open Folder action.

### FR-002 Preview Modes
- The system shall preview PDF files in a PDF viewer.
- The system shall preview `.html`/`.htm` and `.md`/`.markdown` files in either rendered Preview mode or raw Text mode.
- The system shall default `.html`/`.htm` and `.md`/`.markdown` files to rendered Preview mode.
- The system shall persist the user’s HTML/Markdown Preview/Text mode selection across launches.
- The system shall preview supported text files in read-only text view.
- The system shall preview supported image files (`gif`, `png`, `jpg`/`jpeg`, `tif`/`tiff`, `webp`, basic `svg`) in read-only image view, except that `svg` may be shown as read-only **source** (XML) in the text view per user mode.
- The system shall allow the user to choose **SVG preview** (rasterized) or **SVG source** (text) for `.svg` files; the choice shall persist across launches.
- The system shall show an explanatory placeholder for unsupported/unavailable files.

### FR-017 Rendered Preview Link Handling
- In rendered HTML/Markdown Preview mode, clicking `http`/`https` links shall open the URL in the system default browser.
- In rendered HTML/Markdown Preview mode, local file links shall resolve against the current document base path and be handled in-app when the target file exists.

### FR-018 HTML quick-view layout limits and fallback
- The rendered in-app HTML/Markdown Preview mode is a lightweight quick view and shall not be treated as a full browser engine.
- The system shall document that modern CSS layout fidelity, especially `flex`/flexbox and `grid`, may be incomplete or inaccurate in in-app rendered Preview mode.
- The system shall provide and document external-browser fallback via file-tree context actions (**Open in Default App** and **Open With…**) for full layout fidelity.

### FR-010 Native App Handoff
- The system shall provide a file-tree context menu action to open the selected file/folder in the system default app.
- The system shall keep preview behavior unchanged when opening in `skrat`.

### FR-011 CLI Launcher Installer
- The system shall provide an in-app action to install a user-level `skrat` command-line launcher wrapper.
- The installer shall write to a user-writable location and report whether that location is on the current `PATH`.

### FR-012 Open With App Chooser
- The system shall provide an **Open With…** context action for files.
- The chooser shall list known handlers for the selected file type, ordered by local usage preference.
- The chooser shall include an **Other…** option that lets the user select an application executable/bundle path manually.

### FR-013 Open With Preference Persistence
- The system shall persist per-filetype app usage metadata locally (at minimum: last used timestamp and launch count).
- The chooser shall sort candidates primarily by last-used/frequency preference, then by platform-discovered order.
- Launching a file through a selected app shall update persisted usage metadata.

### FR-014 Cross-Platform Fallback Behavior
- On platforms where full handler enumeration is unavailable, the system shall still present best-effort candidates plus **Other…**.
- If no candidate list is available, **Other…** shall remain available so users can choose an app path explicitly.

### FR-003 PDF Navigation
- The system shall support first/prev/next/last page actions.
- The system shall provide a page input field for direct jump with validation.
- The system shall provide Go To action for page/line contexts.

### FR-004 In-document search
- The system shall allow searching within the active PDF using the PDF text layer (when loaded).
- The system shall allow searching within read-only plain-text previews and within rendered HTML/Markdown Preview (`QTextBrowser`), including match counts and next/previous navigation for the active query.
- Searching within SVG applies when SVG is displayed as XML source text.
- The system shall navigate between search results (next/previous).
- The system shall auto-select the first match when new query results apply (PDF via search model; text/rich via document scan).

### FR-005 TOC Navigation
- The system shall show a TOC tab for PDF bookmark navigation.
- The system shall disable TOC tab when no PDF is active.
- The system shall jump to selected bookmark location/page when activated.

### FR-015 PDF Thumbnails Navigation
- The system shall show a Thumbnails tab while a PDF is active.
- The Thumbnails tab shall display per-page thumbnail entries for the active PDF.
- Activating a thumbnail shall navigate the viewer to the selected page.
- The system shall disable Thumbnails tab when no PDF is active.

### FR-016 UI Theme and Font Preferences
- The system shall provide a chrome-only UI theme preference with modes: System, Light, Dark, and Warm Sepia.
- The system shall provide chrome-only UI font preferences (family and point size).
- Theme/font preferences shall persist locally and apply at startup.
- Theme/font preferences shall not alter PDF/text/image document rendering content.

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
- Reference API documentation may be generated with Doxygen from the repository `Doxyfile` into `docs/api/` for HTML/XML consumers.

## 4. Acceptance Criteria

### AC-001 File/Preview
- Given a selected directory, when user selects a supported PDF/text file, then corresponding preview appears.
- Given a selected `.html`/`.htm` or `.md`/`.markdown` file, default rendered Preview appears.
- Given user toggles HTML/Markdown mode to Text, raw source appears for that file.
- Given app restart, previously selected HTML/Markdown mode is restored.
- Given a selected `.svg` file, preview follows persisted SVG Preview/Source preference (raster vs XML text).
- Given unsupported file, then placeholder explains unsupported type.
- Given supported image file (non-SVG), then image preview appears in the preview pane.

### AC-002 PDF Navigation
- Given an active PDF with N pages, when navigation controls are used, then current page changes correctly and page indicators update.
- Given page input outside [1..N], when submitted, then warning dialog appears and page does not jump.

### AC-003 Search
- Given an active PDF with matches, when query is entered, then first result is selected and highlighted per PDF search behavior.
- Given plaintext or rendered HTML Preview with matching text, when query is entered, then matches are enumerated and first match is selected without moving keyboard focus away from the find field while typing.
- Given SVG **source** mode with matching text in the XML, when query is entered, then in-document search behaves like plaintext search.
- Given repeated next/previous actions for the active searchable preview, then navigation cycles across available matches.

### AC-004 TOC
- Given active PDF with bookmarks, TOC tab is enabled and displays entries.
- Given non-PDF active preview, TOC tab is disabled.
- Given bookmark activation, viewer jumps to expected destination.

### AC-013 PDF thumbnails
- Given active PDF, Thumbnails tab is enabled and renders one thumbnail item per page.
- Given thumbnail activation, viewer jumps to that page and page UI state updates accordingly.
- Given non-PDF active preview, Thumbnails tab is disabled.

### AC-014 UI theme/font preferences
- Given user selects a theme/font in Theme Settings and applies it, chrome colors/fonts update immediately.
- Given app restart, the previously selected theme/font preferences are restored.
- Given theme/font changes, document content rendering semantics remain unchanged.

### AC-005 Print
- Given raster print mode, print dialog supports page range and only selected pages print.
- Given native/vector mode, system viewer opens selected PDF for native print controls.

### AC-006 Help/About
- Help dialog displays usage + shortcuts.
- About dialog displays version, maintainer, license summary, and Qt licensing notice.

### AC-007 Documentation
- `docs/` includes SRS, HLA, DD, RTM, and Test Plan documents.
- Release updates align system docs with observable product behavior (preview modes, find scope, CLI usage).

### AC-008 Native app handoff
- Given a selected file in tree view, when user chooses **Open in Default App**, then the OS-associated app opens that file.

### AC-009 CLI launcher installation
- Given user invokes **Tools → Install Command-Line Tool…**, launcher file is created in the platform-specific user location with executable behavior.
- Given installer path is not on `PATH`, the user is shown guidance to add it.

### AC-010 Open With chooser
- Given a selected file, when user chooses **Open With…**, a chooser dialog/menu is shown with known handlers and **Other…**.
- Given user selects a listed app, the file opens in that app and usage metadata updates.

### AC-011 Other fallback
- Given user chooses **Other…**, a file dialog allows selecting an app executable/bundle path.
- Given selected app launches successfully, usage metadata updates and future chooser ordering reflects that choice.

### AC-012 Best-effort platform enumeration
- Given platform association enumeration is partial/unavailable, user can still open the file via **Other…** without blocking errors.

### AC-015 Rendered Preview links
- Given rendered HTML/Markdown Preview mode and user clicks an external `http`/`https` link, then URL opens in system browser.
- Given rendered HTML/Markdown Preview mode and user clicks a local file link, then skrat resolves and previews that target in-app when available.

### AC-016 HTML quick-view layout limits and fallback
- Given an HTML file that depends on flexbox/grid styling, documentation states in-app rendered Preview is a quick view and may not match a full browser.
- Given user needs full fidelity for such files, docs direct user to file-tree context actions to open in an external browser.
