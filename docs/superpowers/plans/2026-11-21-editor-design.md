# Editor Design Implementation Plan

**Goal:** Make Strata's source/preview workspace clearer, readable in both themes, and safer when files change externally.

**Architecture:** Keep Qt widgets and the existing document/view separation. DocumentView owns Markdown modes, preview presentation and document warnings; MainWindow owns actions, find controls and status fields; Highlighter owns theme-aware syntax colors.

**Tech Stack:** C++20, Qt 6.4+, CMake, Qt Test. No new dependencies.

**Spec:** The six design improvements approved in the conversation: theme contrast; compact navigation; readable preview; Source/Split/Preview controls; persistent external-change warnings; progressive find/replace.

## Constraints and review focus

- Preserve native controls, the slate palette, user editor fonts, keyboard access and file-conflict protections.
- Keep the unrelated shortcut-customization checkout untouched; work from origin/main.
- Verify theme changes update existing documents and current-line highlighting.
- Verify every Markdown mode, extra splits, legacy session restoration, and finding text from Preview.
- Verify warnings survive cursor movement/tab switches and cancelled reload/save-copy actions.
- Verify collapsed/expanded find controls and keyboard focus fit a 640px window.
- Preview formatting must never change source, undo history or export content; remote resources stay blocked.

## Tasks

- [x] Add regression tests for syntax contrast and live theme changes, Markdown modes/session restoration, progressive find/replace and persistent conflict warnings.
- [x] Implement theme-specific syntax/focus colors, compact toolbar/sidebar, readable preview and view controls in the existing editor/UI files.
- [x] Separate persistent status fields from transient messages; add document-scoped warning actions without bypassing save/reload validation.
- [x] Run CMake/CTest, inspect dark/light and narrow-window screenshots, run Jev review, and fix justified findings.
- [x] Update the user guide, commit only this work, push the isolated branch, and open a PR with validation and limitations.

## Validation

- CMake Debug build and all 35 Qt Test cases pass on Debian with Qt 6.11.2.
- The six new regression flows pass at QT_SCALE_FACTOR=2.
- Native X11/Xvfb screenshots reviewed in dark/light themes at 1180×780 and 640×420 logical pixels, captured at 1.2× scale.
- Jev follow-up reports improved correctness and reliability with no score regressions; remaining low-priority structural signals do not justify expanding this UI change.
- Windows, Wayland and screen-reader checks remain outside local validation.
