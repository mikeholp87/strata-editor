# Strata Editor

A native C++20 / Qt 6 editor for text, source code, Markdown and formatted HTML. This is a working **0.1.0 preview**, with a tested editing baseline and an explicit roadmap toward a Kate replacement.

![Strata Editor showing Markdown source and preview](docs/screenshot.png)

- Multiple draggable tabs, shared-document splits and independent cursors
- Atomic saves, UTF-8/BOM and UTF-16/BOM support, newline preservation and external-change conflict checks
- Line numbers, undo/redo, indentation, bracket pairing, comments and baseline syntax highlighting
- Find/replace with case, word and regex options; asynchronous project search
- Markdown source with debounced preview, HTML/PDF export
- Dedicated visual HTML editor with undoable formatting, lists, links and tables
- Project explorer, command palette, recent files/projects, recovery snapshots and workspace restoration
- Dark/light themes, editor preferences, optional Git operations and external terminal

No telemetry, accounts, cloud service, or Python/Node runtime. Git and the terminal are optional system tools.

## Build and run

Requires Qt 6.4+ development packages, CMake 3.22+, a C++20 compiler and Ninja (optional). Verified locally with Qt 6.11.2 on Debian x86_64.

```sh
# Debian/Ubuntu build dependencies, if needed:
sudo apt install build-essential cmake ninja-build qt6-base-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/strata-editor
./build/strata-editor README.md
```

Display name is configurable: `-DSTRATA_PRODUCT_NAME="Your Editor"`.

Install with `cmake --install build --prefix ~/.local`. Create system-Qt Linux packages with `cpack --config build/CPackConfig.cmake`. See [building and installation](docs/BUILDING.md), [user guide](docs/USER_GUIDE.md), [feature boundaries](docs/FEATURES.md), [architecture](docs/ARCHITECTURE.md), [implementation plan](docs/IMPLEMENTATION_PLAN.md), and [roadmap](docs/ROADMAP.md).

## Preview limitations

Files above 8 MiB are refused until a large-file viewer exists. Syntax rules are lexical, not Tree-sitter. LSP, embedded terminal emulation, folding, multiple cursors, spellcheck, project replacement and shortcut customization are later milestones. Visual HTML uses Qt’s supported HTML subset and may normalize unsupported markup. This is not yet a complete Kate replacement. Windows CI is provided but has not been executed in this environment.

Original source is [MIT](LICENSE); dynamically linked dependencies retain their own [license terms](THIRD_PARTY.md).
