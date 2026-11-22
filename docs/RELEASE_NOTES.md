# 0.1.0 preview

Initial native editor: text/code tabs, safe saves, rich HTML, Markdown preview, project navigation, shared splits, local recovery/settings, search and optional Git. This is a preview milestone, not completion of all master-prompt phases. See FEATURES.md for exact limits and ROADMAP.md for remaining work. Linux amd64 packages are published through GitHub Releases and the signed central APT repository.

## Next roadmap milestone: shortcut customization

Added a searchable shortcut editor with key recording, clear/reset, persistent QAction mappings and versioned JSON import/export. Changes remain drafts until Save; duplicate and chord-prefix conflicts prevent application. Fixed the shared Ctrl+P assignment by moving Print to Ctrl+Alt+P. Source/rich editor routing now permits custom command keys to override native editing bindings.


## Workspace design and project page

Added Source/Split/Preview controls, a readable Markdown preview column, theme-specific syntax palettes, compact navigation, persistent external-change actions, separate status fields and expandable replacement controls. Finding text from Preview keeps focus in Find; editing commands reveal source. Refreshed the README and project screenshots, documented current feature boundaries, and added the static GitHub Pages project website.

## Document tabs and APT distribution

Refined the document tab strip with clear selected and keyboard-focus states, quiet close icons with hover feedback, bounded widths, middle elision and native overflow scrolling. Accessible tab and close-button names retain the full document name and unsaved state.

The first `v0.1.0` release includes a `.deb` and SHA256 checksums. The package declares its Qt runtime dependencies and installs the binary, application menu entry, icon and license notices under `/usr`. Future Linux updates are available through APT.
