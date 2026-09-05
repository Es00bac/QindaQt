// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Shell::Icons
{

// AGENT-CONTRACT: Maps an application id or desktop-entry id to the entry's
// `Icon=` name by scanning injected application roots (production: the
// `applications/` directories beneath the freedesktop data locations). The
// desktop-entry documents are parsed through the launcher's public, pure
// `DesktopEntryParser`; this module adds only confined filesystem scanning
// and the app-id mapping rules (docs/wiki/shell/iconography.md).
//
// Scanning is eager at construction, deterministic (roots in injected order,
// entries sorted by relative path within one root, first claim wins an id),
// and bounded (file count, per-file bytes, and tree depth ceilings in
// icon_theme_limits.h). Every read is canonicalized and must remain beneath
// its injected root; symlink escapes and non-regular files are skipped.
// Entries that parse but are `Hidden=true`/`NoDisplay=true` contribute no
// icon. Malformed documents fail closed and are skipped.
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
    // exact id, case-insensitive id, then case-insensitive reverse-DNS tail
    // (`org.kde.dolphin` answers `dolphin`); the scan order breaks ties. A
    // trailing `.desktop` on the app id is ignored. Empty or hostile input
    // resolves to an empty string.
    [[nodiscard]] QString iconNameForAppId(const QString &appId) const;

    // Number of admitted entries; exposed for diagnostics and tests.
    [[nodiscard]] int entryCount() const { return int(m_entries.size()); }

private:
    struct Entry {
        QString id;
        QString iconName;
    };

    void scanRoot(const QString &root);
    void admitFile(const QString &canonicalRoot, const QString &relativePath,
                   const QString &canonicalFile);

    QStringList m_applicationRoots;
    QList<Entry> m_entries;
};

} // namespace QindaQt::Shell::Icons
