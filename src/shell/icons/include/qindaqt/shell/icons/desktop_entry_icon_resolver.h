// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Shell::Icons
{

// AGENT-CONTRACT: Maps an application id or desktop-entry id to the entry's
// `Icon=` name and `Name=` display name by scanning injected application roots
// (production: the `applications/` directories beneath the freedesktop data
// locations). The desktop-entry documents are parsed through the launcher's
// public, pure `DesktopEntryParser`; this module adds only confined
// filesystem scanning and the app-id mapping rules
// (docs/wiki/shell/iconography.md).
//
// Scanning is eager at construction, deterministic (roots in injected order,
// entries sorted by relative path within one root, first claim wins an id),
// and bounded (file count, per-file bytes, and tree depth ceilings in
// icon_theme_limits.h). Every read is canonicalized and must remain beneath
// its injected root; symlink escapes and non-regular files are skipped.
// Entries that parse but are `Hidden=true`/`NoDisplay=true` contribute
// neither icon nor name. Malformed documents fail closed and are skipped. An
// entry whose `Icon=` value is refused still contributes its name.
//
// Threading: immutable after construction; every query is const and safe to
// call from any thread. No network access and no filesystem writes.
class DesktopEntryIconResolver
{
public:
    explicit DesktopEntryIconResolver(QStringList applicationRoots);

    // Exact desktop-entry id lookup (the relative path beneath the root with
    // `/` mapped to `-` and the `.desktop` suffix removed).
    [[nodiscard]] QString iconNameForDesktopId(const QString &desktopEntryId) const;

    // Compositor-facing lookup for a window's app id. Resolution order is
    // exact id, case-insensitive id, case-insensitive `StartupWMClass`, then
    // case-insensitive reverse-DNS tail (`org.kde.dolphin` answers `dolphin`);
    // the scan order breaks ties. A trailing `.desktop` on the app id is
    // ignored. Empty or hostile input resolves to an empty string.
    [[nodiscard]] QString iconNameForAppId(const QString &appId) const;

    // The matching entry's `Name=` under the same rules as iconNameForAppId;
    // empty when no visible entry matches.
    [[nodiscard]] QString displayNameForAppId(const QString &appId) const;

    // Presentation name for a compositor window (task list rows, the active
    // application indicator): the entry name for `appId`, else the entry name
    // for `reportedName` (the compositor's resource class: the Wayland app id
    // or the X11 WM_CLASS), else prettifiedApplicationId() of the reported
    // name, or of `appId` when nothing was reported. Non-empty whenever either
    // input is non-empty.
    [[nodiscard]] QString applicationDisplayName(const QString &appId,
                                                 const QString &reportedName) const;

    // Pure fallback: a reverse-DNS id (at least two dot-separated segments,
    // each starting with a letter and made of letters, digits, `_`, or `-`)
    // becomes its last segment (`org.qindaqt.Terminal` -> `Terminal`); any
    // other value is kept whole. A `.desktop` suffix is dropped and a leading
    // lowercase letter is capitalized (`firefox` -> `Firefox`).
    [[nodiscard]] static QString prettifiedApplicationId(const QString &applicationId);

    // Number of entries admitted with an icon; exposed for diagnostics and
    // tests.
    [[nodiscard]] int entryCount() const { return int(m_entries.size()); }

private:
    struct Entry {
        QString id;
        QString value;
        QString startupWmClass;
    };

    [[nodiscard]] static const Entry *matchAppId(const QList<Entry> &entries,
                                                 const QString &appId);
    [[nodiscard]] bool full() const;
    void scanRoot(const QString &root);
    void admitFile(const QString &canonicalRoot, const QString &relativePath,
                   const QString &canonicalFile);

    QStringList m_applicationRoots;
    // Icon claims keep their original admission rule: only entries whose
    // `Icon=` value is acceptable claim an id here.
    QList<Entry> m_entries;
    // Name claims: every visible entry that parses claims its id here.
    QList<Entry> m_names;
};

} // namespace QindaQt::Shell::Icons
