# ADR-0096: Package bundled applications with Portage

- Status: Accepted
- Date: 2026-09-07

## Context

Copying development executables into `/usr/bin` preserved build-directory
RUNPATHs and left Portage unaware of their files and dependencies. The user
requires normal Gentoo installation and dependency management.

## Decision

Maintain `gui-apps/qindaqt-apps` in the repository's Gentoo packaging directory.
A versioned ebuild fetches one immutable source commit, uses the Gentoo CMake
eclass and toolchain, and builds File Manager, Text Editor and Terminal plus
their public QML runtime plugins. CMake installs the FileManager, TextEditor
and Terminal components into Portage's image directory; Portage merges and owns
the files. Never copy raw build executables into the system prefix.

One package owns the three applications and their shared AppShell, Controls and
Tokens runtime files, avoiding duplicate ownership between application packages.
Declare Qt slot dependencies, the supported qtermwidget release, syntax
highlighting, fontconfig and file-opening helpers. Configure-only desktop
provider dependencies remain explicit until the top-level project supports an
apps-only configure. This package does not install the compositor or services.

## Consequences

Portage provides dependency resolution, file ownership, upgrades, uninstallation,
merge logs and optional reusable binary packages. Runtime paths must resolve under the
installed prefix and contain no source/build paths. Verify a normal interactive
Terminal prompt and keyboard output as well as its child process; process startup
alone does not prove a working terminal.

Calendar remains a separate candidate until its owner hands off an accepted
commit and an explicit package component. Documentation and packaging live with
the source; machine-specific keyword acceptance remains in `/etc/portage`.

See [Gentoo app installation](../development/gentoo-apps.md).
