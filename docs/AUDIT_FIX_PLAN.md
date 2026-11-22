# Audit remediation plan

The seven confirmed defects in DEBUG_AUDIT.md are already implemented with failing-before/fixed-after regressions. Preserve those fixes and overlapping tab-design work.

1. Bound Git output to 1 MiB, preserve streamed UTF-8, expose asynchronous cancellation, and prevent normal application close from interrupting a running Git operation. Test process output/cancellation with a compiled fixture, plus a real temporary repository.
2. Report invalid/unreadable recovery JSON. Preserve corrupt files under unique names before replacing them; block replacement if preservation fails. Keep three atomic backup generations and recover the newest valid backup when the primary is corrupt. Test corruption, generation order, and write/backup failures.
3. Recheck source conflicts immediately before QSaveFile commit, retain existing atomic-write and permission checks, and test conflict rejection and unchanged original bytes. This reduces the race window; it cannot promise exclusive locking against non-cooperating writers.
4. Remove the desktop entry category warning and update the audit report with exact implemented status and remaining platform/performance certification limits. Existing explicitly deferred roadmap features are separate milestones.
5. Run all Release and ASan/UBSan tests, native X11 regressions and desktop validation. Keep unrelated in-progress tab changes intact. No GitHub push is part of this task.

Status: completed locally. All four Release and ASan/UBSan CTest suites pass; both audit suites pass under native X11; the isolated sanitized smoke launch, Clang analysis of all 18 production translation units, desktop validation and diff/format checks pass. See DEBUG_AUDIT.md for remaining certification and roadmap limits. No commit, push or installation update was performed.
