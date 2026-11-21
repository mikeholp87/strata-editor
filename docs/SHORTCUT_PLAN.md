# Shortcut customization implementation plan

**Goal:** Complete the next roadmap feature: discoverable keyboard shortcut editing, import/export, persistence, and GUI regression coverage.
**Architecture:** Keep QAction commands as the sole command implementation. A small core shortcut configuration module validates complete mappings against command IDs, handles versioned portable JSON and QSettings, and applies mappings transactionally. A modal dialog edits a draft; Cancel never changes actions or settings.
**Stack:** Existing C++20 / Qt Widgets. QKeySequenceEdit uses only APIs available in Qt 6.4. No new runtime dependencies.

## Acceptance tasks

1. Add behavioral tests for duplicate and prefix conflicts, malformed/unknown-version/unknown-command import, explicitly unbound actions, persistence/reload, defaults reset, and atomic export failure. Observe the new test target fail before implementing the module.
2. Implement src/core/shortcuts.{h,cpp}: stable command IDs; defaults and current mapping; canonical QKeySequence PortableText JSON schema version 1; strict 256 KiB import bound; conflict validation; configurable shortcuts require Ctrl/Alt/Meta or function keys so plain typing remains available. Reject invalid mappings as a whole. Exports use QSaveFile.
3. Implement src/ui/shortcutdialog.{h,cpp}: searchable action list with current/default values, a key recorder, clear/reset selected/reset all, import/export draft and explicit Save/Cancel. Conflicts are shown inline and prevent saving/exporting. Imported drafts require Save to apply. Keep the dialog usable at 640x420.
4. Wire Tools → Keyboard shortcuts and the command palette to the same command. Capture defaults at registration; load mappings only after all commands exist. Replace translated rich-format IDs with stable untranslated IDs. Invalid saved maps leave defaults active and display an explanation.
5. Add GUI tests: remap New and type the replacement key, verify a new tab; reject conflict without modifying live actions; cancel a draft; save/restart restores mapping; menu/action/palette use the updated QAction. Run the complete Debug/Release suites and offscreen/Xvfb smoke, format/diff checks. Update ROADMAP, FEATURES, USER_GUIDE and verification documentation.

## Contract and boundaries

Import is a full configuration built from defaults plus supplied overrides; omitted IDs reset to defaults, empty strings unbind. Unknown IDs, unsupported versions, invalid key sequences, exact duplicates and prefix ambiguities fail without mutation. Stable IDs are independent of translated labels. This milestone configures registered application commands; native editor movement and text-widget context-menu shortcuts are not remapped. No automatic push or merge is part of this implementation milestone. Other Next roadmap items stay pending.

## Official references

- https://doc.qt.io/qt-6/qkeysequenceedit.html — recorder and Qt-version availability
- https://doc.qt.io/qt-6/qkeysequence.html — portable strings and partial/exact matches
- https://doc.qt.io/qt-6/qaction.html — shared menu/toolbar/keyboard command actions
