# Architecture and research

## Environment
Inspected 2026-11-21: empty workspace; Debian forky/sid x86_64, GCC 16.2.0, CMake 4.3.4, Ninja 1.13.2, Qt 6.11.2 Widgets/Test/Concurrent/PrintSupport. A protected empty .git directory is provided by the environment, so this workspace is not currently a usable Git checkout. No external repository is modified.

## Engine decision
Use QPlainTextEdit and QPlainTextDocumentLayout for source documents and QTextEdit for dedicated HTML documents. Qt provides Unicode editing, undo grouping and shared QTextDocument views. QSyntaxHighlighter supplies a block-based baseline; it is not a structural incremental parser. Rich text and source have distinct file modes. Markdown preview never serializes back into source.

KTextEditor provides mature indentation, syntax and editing components through KF6::TextEditor without requiring Plasma itself. It would reduce advanced-editor risk but increases dependency/distribution complexity and does not solve rich-text editing. It is not included in this baseline. Optional backend work must preserve document/view separation.

## Boundaries
Document owns text, path, mode, original bytes digest, encoding and newline metadata; SafeDocument refuses external resources. Views own cursors and scroll positions. MainWindow coordinates commands, sessions and presentation. Search uses a worker with cancellation; Git uses QProcess argument lists. Disk decoding uses QtConcurrent. QSaveFile provides atomic writes with direct-write fallback disabled. Settings have a schema version. Recovery snapshots use atomic JSON writes in QStandardPaths application data.

## Prior art and official references
Reviewed documentation and project overviews, without copying code or visual assets:
- [Qt overview](https://doc.qt.io/qt-6/), [QPlainTextEdit](https://doc.qt.io/qt-6/qplaintextedit.html): block-oriented source editing; custom gutters via visible blocks.
- [QTextEdit](https://doc.qt.io/qt-6/qtextedit.html): rich document model and undoable character/block formatting.
- [QSyntaxHighlighter](https://doc.qt.io/qt-6/qsyntaxhighlighter.html): per-block format rules with state propagation for multiline tokens.
- [Kate](https://kate-editor.org/), [source](https://github.com/KDE/kate), [handbook](https://docs.kde.org/stable_kf6/en/kate/kate/): documents, views, sessions, navigation and plugin-based tools inform separation of responsibilities. Handbook URL currently describes an older release; do not infer current API details from it.
- [KTextEditor](https://api.kde.org/ktexteditor-index.html): embeddable editor component.
- [Helix](https://github.com/helix-editor/helix): modal editing, Tree-sitter and LSP demonstrate separation of parsing, language services and editing state.
- [Zed](https://github.com/zed-industries/zed): performance-oriented editor; its custom rendering architecture is beyond a lightweight Qt baseline.
- [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus): Windows/Scintilla-oriented approach does not match the portable Qt document engine selected here.
- [Tree-sitter](https://tree-sitter.github.io/tree-sitter/): incremental concrete syntax trees. Upgrade needs per-language grammar licenses, byte/UTF-16 position maps, worker revision tokens and parse cancellation.
- [LSP](https://microsoft.github.io/language-server-protocol/): future JSON-RPC transport must frame Content-Length bytes, negotiate position encoding, track request IDs/timeouts and reject stale responses. Workspace edits require preflight and rollback; no servers are downloaded automatically.

## Risks and limits
Qt text layout still occurs on the GUI thread. Files over 8 MiB are refused with an explanatory error until a bounded streaming viewer exists; async decoding does not make layout free. Whole-document snapshots, regex searches and serialization can be costly; ordinary document editing is the baseline. Mixed newlines normalize to the detected predominant convention on save and are disclosed in status. UTF-16 requires BOM; invalid UTF-8 is rejected rather than guessed. Full source preservation for complex HTML is not possible with QTextDocument, so HTML is explicitly opened as visual HTML and serialization limitations are documented.

## Dependency/license choice
Original code: MIT. Dynamically linked Qt modules: LGPL-3.0/GPL/commercial options, with distribution compliance required; no Qt source is copied. Git is an optional external executable (GPL-2.0); terminal is an explicitly launched system application. No Tree-sitter grammar, KDE, Helix, Zed or Notepad++ code is vendored. See THIRD_PARTY.md.
