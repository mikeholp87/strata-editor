# Security policy

Strata Editor is a preview release and is not yet supported for production security-sensitive workflows.

## Reporting a vulnerability

Please report suspected vulnerabilities privately through GitHub:
<https://github.com/mikeholp87/strata-editor/security/advisories/new>

Do not open a public issue for a vulnerability, and do not attach sensitive files or secrets to any issue. Include the version (`strata-editor --version`), your platform, and the smallest file or steps that reproduce the problem. You can expect an acknowledgement within a week.

## What the application does and does not do

- Document resource loading is blocked, child processes receive argument lists (never shell strings), and there are no AI or network services.
- Opening a second file while the editor is running passes absolute paths through a small request file in the per-user data directory; requests older than a minute are discarded.
- Keep your system Qt and Git updated. Dependency distribution terms are in [THIRD_PARTY.md](THIRD_PARTY.md).
