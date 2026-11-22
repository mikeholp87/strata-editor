# Strata Editor debug audit — 2026-11-21

## Scope and baseline

Local audit of the native Qt editor at baseline `9ab343a`, on branch `fix/debug-audit`. Applied the requested debugging-and-error-recovery workflow: reproduce, localize, reduce, fix, add regressions, verify. Reviewed document bytes and atomic saves, recovery/session/settings, source and rich editing, Markdown resources, shared views, command/shortcut routing, asynchronous search/Git, CLI startup and packaging/CI configuration.

The existing Release suite passed before changes. Defect regressions live in `tests/test_audit.cpp`; failures were observed before their fixes. Follow-up hardening cases live in `tests/test_hardening.cpp` with a compiled Git-process fixture. Separate tab-design/resource/accessibility edits arrived during the audit; those were preserved and are not audit changes. They continued evolving, so this report describes the tested audit build rather than certifying subsequent edits. No commit, push or installation update was performed by this audit run.

## Confirmed defects and fixes

| Severity | Defect and reproduction | Root cause and fix |
|---|---|---|
| P1 | A POSIX named pipe blocked document opening; a bounded pre-fix probe timed out in `Document::read` → `QFile::open`/`open64`. | The loader opened special filesystem objects as ordinary files. Reject existing non-regular files before opening and during disk-conflict checks, including a regular file replaced by a pipe. Regression: `fifoCannotBlockOpen`. |
| P1 | A failed read could be accepted as empty text, or an unreadable empty-file hash could be treated as unchanged. Linux `/proc/self/mem` provides a deterministic read-error fixture at offset zero. | `readAll()` return bytes were used without checking QFile errors. Both load and conflict checks now check I/O status and bound reads to 8 MiB + 1 byte, including files that grow during a read. Regression: `readErrorsAreReported`. |
| P1 | Changing line endings, then undoing a text edit removed the unsaved tab indicator and could skip Save All, recovery, reload/close warnings. | QFile serialization metadata was represented only by QTextDocument’s text modification flag. Track the saved newline convention independently and use `Document::isModified()` throughout the UI and recovery paths. Regression: `newlineChangeSurvivesTextUndo`, including Save All, exact CRLF bytes, cancelled close and reverting to the saved convention. |
| P1 | Saving an unpaired surrogate or NUL succeeded, silently replacing text or creating a document the strict loader could not reopen. | Encoding results were not validated. Validate Unicode/NUL before opening the atomic output; reject invalid content while preserving original bytes and the dirty state. Regressions: `invalidEditorTextCannotCorruptSave` for UTF-8, UTF-16 and NUL. |
| P1 | A literal U+FEFF immediately after a file’s encoding BOM disappeared during an unchanged save. Reproduced for UTF-8, UTF-16 LE and BE. | Manual BOM removal was followed by a decoder that also skipped an initial BOM character. Decode the remaining content with ConvertInitialBom so only the actual encoding marker is removed. Regressions: `bomContentRoundTrip`. |
| P2 | Custom brackets/indentation/newline keys, menu edits, replace and rich-format commands could alter read-only views. | QTextCursor-based handlers bypassed the native widget’s read-only check. Add checks at custom input and action boundaries, including the active split. Regressions: `readOnlyCustomKeys`, `readOnlyMenuCommands`, `readOnlyRichFormatting`. |
| P2 | Typing a closing bracket over selected text could move past an existing bracket without replacing the selection. | Pair-skipping checked the next character without checking for a selection. Skip an existing closer only for an empty selection. Regression: `closingBracketReplacesSelection`, including Undo. |

Two other suspected cases—line commands at a block start and directory input—already behaved correctly. Their tests passed before changes; no speculative line-selection fix was made.

## Follow-up hardening implemented

- Git drains stdout and stderr while displaying at most 1 MiB per operation, with a truncation notice and persistent UTF-8 decoders. Status, diff and history can be cancelled asynchronously (terminate, then kill after one second). Stage/unstage/commit are allowed to finish; normal window close is blocked while any Git process runs. Git is resolved from PATH before changing directory, optional status locks and fsmonitor are disabled, and history does not invoke signature verification. Tests use an output-flood/fragmented-Unicode/delayed-process fixture and a real temporary Git repository.
- Checked JSON reads distinguish missing, malformed, unsupported, oversized and unreadable data. Recovery records are validated. Startup reports and preserves corrupt primaries under unique names; if preservation fails, automatic replacement is disabled for the session. The newest valid nonempty backup is offered if the primary is missing or invalid. A valid empty primary after clean close does not reopen historical buffers.
- Three distinct nonempty recovery generations are retained with atomic writes. Failed backup writes prevent primary replacement. The separate declined-recovery backup remains. Startup does not start the snapshot timer until the restoration prompts finish, preventing empty snapshots while a recovery decision is pending. Tests cover exact preserved bytes, malformed records, backup order, blocked backup destinations, refusal to overwrite an invalid primary, and actual UI restoration.
- Document saves recheck the original digest after serialization/writing and immediately before atomic commit. External conflict tests continue to verify rejected saves preserve disk bytes. This narrows the race window without claiming exclusive locking or a filesystem compare-and-swap.
- The desktop entry uses a single main menu category, removing the validator hint.

## Verification

- Release build and all four CTest executables pass after fixes (4.48 seconds in the final local run).
- The new audit executable passes all 18 Qt Test cases (including data rows and test lifecycle), with no skips on this Linux host.
- The hardening executable passes all 12 Qt Test cases (including lifecycle), with no skips on this Linux host. Its first UI-restoration attempt exposed an incorrect test dialog response; the test now clicks the actual Yes button and asserts recovered content and modification state.
- All 18 audit and 12 hardening cases also pass under Xvfb with the XCB platform, generic platform theme and Fusion style. These settings isolate the test from the host desktop’s portal/theme integration.
- Clean Debug build with AddressSanitizer and UndefinedBehaviorSanitizer: all four CTest executables pass (6.37 seconds in the final local run). Leak detection is enabled; no project-specific suppressions or weakened tests are used. A separately isolated sanitized application smoke launch also exits successfully with no sanitizer diagnostics. One earlier parallel rebuild was externally terminated (exit 143) without a compiler diagnostic; it was rerun to completion before final validation.
- Clang static analysis of all 18 production translation units completed with zero diagnostics. Detailed local output: `/tmp/strata-audit-static.txt`.
- Desktop-file validation succeeds without diagnostics.
- Jev Review was attempted, but the server returned `JEV_API_KEY is not set`; no scalar score is claimed.

Commands:

```sh
cmake --build build-release --parallel 2
QT_QPA_PLATFORMTHEME=generic QT_STYLE_OVERRIDE=Fusion ctest --test-dir build-release --output-on-failure --timeout 45
cmake -S . -B /tmp/strata-debug-audit -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build /tmp/strata-debug-audit --parallel 2
QT_QPA_PLATFORMTHEME=generic QT_STYLE_OVERRIDE=Fusion ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir /tmp/strata-debug-audit --output-on-failure --timeout 45
```

## Remaining limits and follow-up

- This is local Linux verification with Qt 6.11.2. Windows, physical printing, native Wayland window-manager behavior, and minimum-supported-Qt builds were not verified. CMake reports unavailable CUPS development libraries; PDF tests are covered by the suite.
- The 8 MiB document limit, synchronous layout/save/serialization and regex operations remain. Pathological regex/HTML workloads, network filesystem stalls and a streaming large-file viewer need separate performance/cancellation work. No comparative performance claim is made.
- External-change checks are optimistic; atomic replacement prevents partial writes but does not provide an exclusive lock against non-cooperating writers. ACLs, extended attributes and ownership preservation are not certified by the current mode-bit tests.
- Git cancellation controls the direct read-only Git process; arbitrary subprocess trees, hooks and forced OS shutdown are not certified. Mutation commands intentionally have no cancel action. Their hooks may still delay application exit.
- Recovery keeps three generations plus the separate declined backup, with a 64 MiB limit per JSON file. Configurable retention and automatic cleanup of preserved corrupt files remain future work. JSON serialization/backup writes remain synchronous.
- LSP, Tree-sitter, embedded terminal, multiple cursors and other explicitly deferred roadmap features were not represented as implemented or tested.
