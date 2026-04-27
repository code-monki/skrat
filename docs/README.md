# skrat System Documentation

This folder contains core engineering documentation for the `skrat` desktop viewer, including PDF/text/image behavior, HTML/Markdown Preview-vs-Text mode, SVG preview-vs-source mode, unified in-document find, and the CLI launcher.

## Documents

- `SRS.md` - Software Requirements Specification (includes acceptance criteria)
- `HLA.md` - High-Level Architecture
- `DD.md` - Detailed Design
- `RTM.md` - Requirements Traceability Matrix
- `TEST_PLAN.md` - Test Strategy and Execution Plan

**Generated API reference (Doxygen):** after configuring Doxygen at the repository root, HTML output is written to `docs/api/html/` (see root `Doxyfile`). Regenerate when public C++ APIs or doc comments change materially.

## Usage

- Treat `SRS.md` as the requirements baseline for release scope.
- Treat `SRS.md` Product Scope Guardrails as the source of truth for what is intentionally out of scope.
- Keep `RTM.md` synchronized when requirements or implementation/test coverage changes.
- Update `TEST_PLAN.md` with each release's regression focus areas.
