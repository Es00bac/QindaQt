# ADR-0218: Use QindaTK and Poppler for the image/PDF viewer

- **Status:** Accepted
- **Date:** 2026-09-19
- **Owners:** First-party applications and session defaults
- **Supersedes:** [ADR-0116](0116-build-bundled-applications-on-stock-qt6.md)
  only for the new Viewer application; its existing applications are unchanged.
  Also supersedes [ADR-0124](0124-add-qindaqt-bliss-luna-option-set.md)
  decision8's login-time MIME seeding.

## Context

The desktop needs an image/PDF viewer and useful file associations. The product
owner explicitly chose QindaTK, C++, and GPLv3-compatible reuse rather than a new
PDF renderer. QindaTK already supplies application chrome and document viewports;
QindaMPV already provides the desktop's media player. Poppler has a maintained
[Qt 6 C++ frontend](https://poppler.freedesktop.org/api/qt6/).

## Decision

- Ship `qindaqt-viewer`, desktop ID `org.qindaqt.Viewer.desktop`, under
  GPL-3.0-or-later. Its presentation uses the installed QindaTK toolkit.
- Keep document loading and rendering in the application's C++ backend. Link
  Poppler's `poppler-qt6` frontend privately for PDF parsing/rendering and use
  Qt image readers for supported image formats. Do not vendor a rendering engine
  or expose Poppler types to the shell, Settings, or QML.
- QindaTK and Poppler are explicit build/runtime dependencies of Viewer.
  `QINDAQT_BUILD_VIEWER` defaults to `ON`; disabling it is an explicit reduced
  developer build. Full desktop packages must keep it enabled.
  QindaTK's LGPL-3.0-or-later and the installed Poppler frontend's
  GPL-2.0-or-later license permit this GPL-3.0-or-later application. Upstream
  engines retain their own licenses and update cycles.
- Package desktop-scoped defaults in `applications/qindaqt-mimeapps.list`,
  following the [freedesktop MIME Applications specification](https://specifications.freedesktop.org/mime-apps/latest-single/).
  Files use QindaQt File Manager; plain text uses Text Editor; images and PDFs
  use Viewer; audio and video use `org.qindaqt.QQMpv.desktop` (QindaMPV).
  Browser/mail fallbacks refer to installed conventional applications.
- User and administrator associations retain their standard precedence.
  Login must not copy these defaults into user preferences. The Default
  Applications route offers a separate PDF category, displays inherited
  defaults, and writes only the category the user changes.
- Reuse the public application catalog with an explicit MIME-handler scan
  mode. `NoDisplay` hides an application from menus without invalidating its
  file associations; `Hidden` remains a deletion marker in every mode.
  Ordinary launcher consumers retain their existing visibility policy and
  higher-priority desktop entries continue to mask lower-priority copies.

## Consequences

The stock-Qt presentation rule continues to apply to Calendar, File Manager,
Terminal, and Text Editor. Viewer is an explicit QindaTK application, not a
second desktop theme authority. Its library dependencies stay local to the
application, with no new daemon or desktop-global file-access service.

The package must include the QindaTK QML runtime, Qt image-format plugins and
Poppler Qt 6. QindaMPV is a separate package; install it after the desktop's
AppShell libraries to avoid a build dependency cycle. Associations do not
pretend that a missing application is installed.

QindaTK currently has a local source repository, so hosted CI explicitly
disables Viewer until it can provision that dependency. Those jobs do not
qualify Viewer; installed-dependency native builds run its rendering, UI and
relocated-install gates. Missing QindaTK must fail an enabled Viewer configure.

The [Viewer contract](../apps/viewer.md) and
[Default applications contract](../apps/default-applications.md) own supported
formats, interaction details and verification. Image/PDF editing, annotations,
and a new rendering engine are outside this decision.
