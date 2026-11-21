# Local verification — 2026-11-21

Environment: Debian forky/sid x86_64; GCC 16.2.0, CMake 4.3.4, Ninja 1.13.2, Qt 6.11.2. No development Qt packages needed installation. clang-format 23.1.5 was installed under /tmp for formatting only.

## Results

- Debug and Release CMake/Ninja builds succeeded. Final compilation with -Wall -Wextra -Wpedantic had no source warnings.
- CTest: 1 executable, 29 Qt Test cases including data rows and lifecycle cases; 29 passed, 0 failed, 0 skipped.
- Tests cover exact byte round trips (UTF-8/BOM, both UTF-16 BOM endiannesses, emoji/RTL, NBSP/U+2028, empty file), truncated/binary refusal, external-save conflicts, read-only save refusal, shared split undo, line commands, rich-text save/reopen, Markdown source/resource safety, persistence corruption and schema handling, literal project search/cancel, unsaved-close cancellation, settings reload, split state, actual workspace restoration, asynchronous open/edit/save, GUI find/replace/undo, optional Git operations in a temporary repository, PDF output, source-safe exports and oversized snapshot retention.
- Xvfb launch and screenshot succeeded; docs/screenshot.png was inspected. Markdown scroll reset was corrected after inspecting the first capture. Screenshot shows the real app, not a mockup.
- CMake staged installation to /tmp/strata-install succeeded; installed executable reported 0.1.0.
- CPack generated dist/strata-editor-0.1.0-Linux.deb and .tar.gz. dpkg-deb metadata and archive contents were inspected. The local DEB requires this system’s Qt >=6.11.2; rebuild on the intended distribution for its own compatible dependency set. No system-wide install was performed.
- clang-format dry-run passed on source, tests and benchmark code.
- Measurements are recorded in benchmarks/README.md, docs/benchmarks.json and docs/startup-lifecycle.json. Lifecycle includes an intentional timer; no pure startup, idle CPU or real scrolling-latency claim is made.

## Review and fixes

A combined code-quality/reuse/efficiency/test/comment review found Unicode normalization through Qt toPlainText, unsafe PDF source targeting, wrong-pane commands, mismatched recovery limits and unsafe declined-backup replacement. These were fixed and protected with behavioral regressions. The reviewer rechecked the fixes, reran CTest and found no new material issue within that scope. Fixed regexes are cached and redundant language rehighlighting is avoided. No proof tests were removed or weakened.

Jev Review was attempted but returned `JEV_API_KEY is not set`; no scalar scores were available. Code review: skipped (ce-code-review unavailable) — its required Git base cannot be resolved in the protected empty .git workspace. The file-set review above and manual code scan provided local fallback coverage, not a completed CE review receipt. No commit, push or release publication was attempted during the build milestone. Git was subsequently initialized and the existing GitHub history fetched at the user’s explicit request.

## Coverage limits

Windows CI is authored but unexecuted here. Neither Wayland nor physical printing, accessibility technology, installed-system package lifecycle, AppImage/installer delivery, or a CommonMark conformance corpus was verified. CMake reports missing CUPS development libraries; PDF generation passes, but physical printing needs platform validation. LSP/Tree-sitter/embedded terminal/multicursor/folding/spellcheck and streaming large files are not implemented. Recovery over 64 MiB total fails visibly and retains the previous snapshot. U+2029 source files are refused rather than silently normalized. Large synchronous layout/save/serialization and regex operations remain bounded by the document cap, but need further responsiveness work before production release.


## Roadmap milestone: configurable shortcuts

Implemented on `feat/shortcut-customization`: command shortcut editor, key recording, draft clear/reset, duplicate/prefix conflict detection, versioned portable JSON import/export, and QSettings persistence. Defaults and stable command IDs remain shared with QAction menus/toolbars/palette. The previously ambiguous Print/Quick Open Ctrl+P default is resolved: Print is Ctrl+Alt+P.

Evidence: the new shortcut test target first failed because the configuration module did not exist. Integration then exposed the existing Ctrl+P default conflict. Regressions for empty-selection clipboard retention and rich HTML copy were observed failing before the handlers were corrected. A bounded read-only review covered the new module, UI and editor-event routing. Jev Review again returned `JEV_API_KEY is not set`; no scalar review score is claimed.

Final validation: Debug and Release builds succeeded without source warnings. Both CTest executables pass in both configurations: 23 shortcut cases plus 29 baseline cases, 52 total including data rows/lifecycle cases. Release smoke launch passed. clang-format dry-run and git diff --check passed. Xvfb captures were inspected at the default size and 640×420 logical pixels; controls remain usable. Screenshot: docs/shortcuts.png. Tests use temporary settings and data directories; they do not change the running editor’s preferences. Windows/native file-dialog behavior and OS global shortcut conflicts were not verified. Native text navigation/context-menu bindings are not globally remapped. The source and rich editors give assigned application shortcuts precedence; ordinary typing remains on the native path.


## Workspace design and project documentation

The merged workspace includes theme-specific syntax palettes, Source/Split/Preview modes with session persistence, a readable preview column, persistent external-change warnings, separate status fields and progressive Find/Replace. A review regression for Find-field focus when revealing source was fixed with real keyboard-event coverage.

Current local Release validation on Debian / Qt 6.11.2: both CTest executables pass, with 35 editor checks and 23 shortcut checks (58 including data rows/lifecycle checks). GitHub Actions completed successfully on the merged `834a509` baseline for Linux and Windows. This is build/test validation, not manual Windows desktop or printing validation.

The project page uses the existing Tasky/Luma visual family, current native application captures, source-build instructions and explicit preview limitations. Browser checks cover desktop, 375px and 320px widths; light/dark image selection; visible keyboard focus; reduced motion; and expanded Windows build instructions. Axe 4.12.1 reports zero violations and zero incomplete rules in the checked light/dark mobile and desktop states. Static checks cover local links, anchors, image sizes, metadata JSON and README file links. The page contains no runtime JavaScript, third-party analytics or external font requests.
