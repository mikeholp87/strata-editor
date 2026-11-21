# Roadmap

0.1 preview: functional minimum editing, writing, navigation and persistence baseline; build/test/package validation on Linux.

Next: broaden GUI safety tests, shortcut editor/import/export, project file operations, complete quick-open traversal, search-in-selection, ignore rules, tab/split polish and source/visual HTML choice. Improve whole-document operations and profile layout/typing latency.

0.2: streaming read-only large-file viewer and measured 1/10/100 MiB benchmarks; transactional reviewed project replacement; spellchecking with optional licensed dictionaries; Markdown conformance and richer navigation.

0.3: optional Tree-sitter with audited grammar licenses and true incremental worker parsing; tested folding and multi-cursor behavior. LSP JSON-RPC transport, framing, negotiated UTF positions, cancellation and revision tracking; clangd integration first, transactional workspace edits before rename.

Tools/release: verified PTY component on Linux, ConPTY on Windows, without claiming a QProcess output view is a terminal; AppImage/Flatpak and Windows installer, signing and license compliance. CI must pass on Windows before advertising platform verification. AI remains disabled and unimplemented until explicitly configured in a future extension.

All advanced items require behavior tests before being marked shipped.
