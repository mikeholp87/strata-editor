# Strata Editor Implementation Plan

**Goal:** Build a reliable Qt desktop editor covering the minimum release baseline in the supplied master specification.
**Architecture:** Independently owned QTextDocuments carry file identity and serialization metadata. Editor views share those documents. The main window composes dock panels and registered QAction commands.
**Tech Stack:** C++20, Qt 6 Widgets, Concurrent, PrintSupport, Test; CMake.
**Spec:** User-supplied Strata Editor master build prompt, 2026-11-21.

## Constraints and review focus
Native, Linux first, portable core, no accounts, telemetry, Python/Node runtime, or automatic project command execution. Product display name is a CMake cache option. Prioritize data integrity over advanced feature count.
Review Unicode/BOM round trips, external changes before save, cancelled close, rich-text resources, and recovery of unsaved buffers.

## Task 1: Document foundation
Files: src/core/document.{h,cpp}, tests/test_strata.cpp, CMakeLists.txt.
Implement decode/encode with strict UTF-8 and BOM-tagged UTF-16, newline metadata, binary rejection, atomic saves and hash conflict detection. Keep document storage independent of views. Test round trips, external conflict, invalid data, and shared undo.
Acceptance: CMake build and Qt Test passes; changed disk content is never silently overwritten.

## Task 2: Editing workspace
Files: src/editor/codeeditor.{h,cpp}, src/editor/highlighter.{h,cpp}, src/ui/documentview.{h,cpp}, src/ui/mainwindow.{h,cpp}, src/main.cpp.
Implement line numbers, auto-indent, comment/line commands, tabs, shared splits, centralized commands, find/replace, status, explorer, quick open and palette.
Acceptance: GUI tests drive edit/save, search/replace, split synchronization and document closing.

## Task 3: Writing and persistence
Files: src/core/persistence.{h,cpp}, src/editor/safedocument.{h,cpp}, main window and document view.
Implement HTML mode, formatting, debounced Markdown preview, PDF/HTML export, themes, settings, recent files, sessions and recovery snapshots. Never convert Markdown source via preview.
Acceptance: rich-text formatting survives save/reopen; session and recovery tests verify actual content and paths.

## Task 4: Tools and delivery
Files: src/ui/projectsearch.{h,cpp}, src/ui/gitpanel.{h,cpp}, packaging, CI, documentation.
Add cancellable asynchronous project search, asynchronous Git status/diff/stage/unstage/commit, external terminal action, install target and CPack DEB/TGZ, Linux/Windows CI, license and user docs.
Acceptance: real temporary-project search and Git process checks; build, CTest, launch smoke, installation and package checks.

## Deferred milestones
Tree-sitter, LSP, embedded PTY/ConPTY terminal, multi-cursor/column selection, folding, spellcheck, transactional project replacement, shortcut import/export, streaming large-file viewer, AppImage and Windows installer verification. Do not advertise these as implemented.

## Verification approach
No existing code/tests. Add behavioral Qt tests alongside each coherent implementation, then run full CTest. Use offscreen and Xvfb for GUI checks. Record measured benchmark results only; no comparative speed claims without hyperfine.
