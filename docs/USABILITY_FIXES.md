# Usability fixes requested in the live review

- Independent project-sidebar font size in Preferences. Ctrl++ and Ctrl+- resize the focused project tree or source editor and persist its size, with limits of 7–48 pt. Commands use the existing shortcut registry and are remappable.
- Each source/rich editor split has an X header when multiple editor panes exist. Closing either pane preserves the shared document and undo history. The last editor view remains; the existing Close extra split views command uses the same removal path. Markdown preview retains its Source/Split/Preview controls.
- Focus mode has a visible Exit focus mode (F11) control, restores previous panel visibility, and persists the normal layout if the app closes while focused.
- Closing the last document leaves a fresh editable Untitled buffer. Close All closes the original documents once and leaves one empty buffer, retaining existing save/discard/cancel prompts.
- Git is functional. Its panel now explains how to begin, identifies the working folder while running, and reports an already-running operation rather than appearing to ignore another click. No unsolicited repository writes occur.

Broader visual redesign, full-project quick open and a clickable Git changes list remain suggestions, outside these concrete fixes.

Validation: Release and ASan/UBSan builds pass all five CTest suites; the new usability suite passes all nine Qt Test rows (including lifecycle) under native X11. Tests verify actual visible close buttons, retained document content/undo/dirty state, keyboard zoom and saved sizes, last-tab/Close All behavior, F11 and button exit, and restored dock layout. Existing Markdown visibility tests now test effective visibility through pane headers; the status test expects the new empty document after final-tab close. Jev Review remains unavailable because JEV_API_KEY is not set. The verified Release build was installed under ~/.local; the open application window is not forcibly restarted.
