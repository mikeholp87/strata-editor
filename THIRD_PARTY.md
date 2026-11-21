# Dependency inventory

| Component | Use | License / distribution |
|---|---|---|
| Qt Core, Gui, Widgets, Concurrent, PrintSupport | Required dynamic runtime | LGPL-3.0-only or GPL/commercial options; see [Qt licensing](https://www.qt.io/licensing/open-source-lgpl-obligations) and installed Qt license files |
| Qt Test | Tests only | Qt license terms |
| Qt Markdown parser | Through QTextDocument | Qt ships MD4C; preserve Qt third-party notices with bundled Qt |
| Git executable | Optional, installed separately | GPL-2.0; no Git code is linked or bundled |
| System terminal | Optional, installed separately | Provider-specific; not bundled |
| CMake / Ninja / C++ compiler | Build tools | Not application runtime dependencies |

No KDE, Helix, Zed, Scintilla or Tree-sitter source is copied. Original application source and icon are MIT. A standalone Windows bundle must include the licenses and third-party notices from the exact Qt distribution and permit Qt library replacement under the applicable LGPL terms. Source packages here do not bundle Qt. CPack DEB depends on the system Qt packages.

Development used clang-format 23.1.5 in /tmp only. It is not a runtime or build requirement.
