# Development

Source boundaries: core/document handles bytes, metadata and shared text; persistence handles versioned JSON; editor handles source behavior/highlighting; UI views own cursor state and rendering, window commands coordinate actions; project search owns its worker and Git owns its process. Do not add shell command strings, automatic project execution, network document loading, telemetry or mandatory services.

Build and run CTest after behavior changes. Add regression coverage for encoding/file safety and meaningful GUI flows. Tests run in Qt test-mode paths. No production Python/Node dependency. clang-format uses .clang-format; a formatter is optional. No Gitleaks scan is run by default.

Local verification and packaging details are recorded in VERIFICATION.md. Windows/X11/Wayland matrix and assistive-technology checks remain release work. Versioned settings currently initialize schema 1; JSON with unsupported schema is ignored. Future migration must preserve backups before changing stored content.
