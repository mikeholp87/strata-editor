# Release notes

## Unreleased (audit fixes)

- Opening a file while Strata Editor is already running now opens it in the running window instead of showing an error.
- Saving an HTML file that this editor did not write now asks first, because the visual editor drops scripts, styles and the doctype. Choose **Save a copy…** to keep the original.
- Syntax highlighting no longer colours the rest of a line as a comment after a `//` or `#` inside a string (for example `"https://example.com"`). JSON has no comments, and CSS and HTML use block comments only.
- **Toggle comment** uses the right marker for the language (`--` for SQL, `/* */` for CSS, `<!-- -->` for HTML and Markdown) and leaves empty lines alone.
- Recovery snapshots are skipped when nothing changed, which removes a periodic stall with large unsaved documents. Autocomplete copies only the text around the cursor (128 KiB each side) instead of the whole document.
- Closing a tab releases its file watch, and recovered documents are watched for outside changes.
- The version shown by `--version` and Help → About now comes from the build (`project(... VERSION)`), so it cannot drift.
- `SECURITY.md` points to GitHub private vulnerability reporting.

## 0.1.2

Autocomplete (words from the document plus language keywords, `Ctrl+Space`) and Markdown formatting commands, toolbar and Enter-continuation for lists and quotes.

## 0.1.1

Debian-compatible Qt dependency names in the `.deb`, so one package installs on both Ubuntu 24.04 and Debian.

## 0.1.0 preview

Initial native editor: text/code tabs, safe saves, rich HTML, Markdown preview, project navigation, shared splits, local recovery/settings, search and optional Git. This is a preview milestone, not completion of all master-prompt phases. See FEATURES.md for exact limits and ROADMAP.md for remaining work. Linux amd64 packages are published through GitHub Releases and the signed central APT repository.

## Next roadmap milestone: shortcut customization

Added a searchable shortcut editor with key recording, clear/reset, persistent QAction mappings and versioned JSON import/export. Changes remain drafts until Save; duplicate and chord-prefix conflicts prevent application. Fixed the shared Ctrl+P assignment by moving Print to Ctrl+Alt+P. Source/rich editor routing now permits custom command keys to override native editing bindings.


## Workspace design and project page

Added Source/Split/Preview controls, a readable Markdown preview column, theme-specific syntax palettes, compact navigation, persistent external-change actions, separate status fields and expandable replacement controls. Finding text from Preview keeps focus in Find; editing commands reveal source. Refreshed the README and project screenshots, documented current feature boundaries, and added the static GitHub Pages project website.

## Document tabs and APT distribution

Refined the document tab strip with clear selected and keyboard-focus states, quiet close icons with hover feedback, bounded widths, middle elision and native overflow scrolling. Accessible tab and close-button names retain the full document name and unsaved state.

The first `v0.1.0` release includes a `.deb` and SHA256 checksums. The package declares its Qt runtime dependencies and installs the binary, application menu entry, icon and license notices under `/usr`. Future Linux updates are available through APT.
