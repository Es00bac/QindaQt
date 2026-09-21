// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

namespace QindaQt::Compositor {

// AGENT-CONTRACT: the stateful half of ADR-0230, layered on ADR-0169. Given
// the same inputs the ADR-0169 repair already has (an opaque window class and
// the client's command line, read through KWin's authenticated PID), this
// cache answers two things without ever blocking on unbounded filesystem
// work:
//
//  1. steamNameForClass(): the human title behind a `steam_app_<id>` class,
//     from `appmanifest_<id>.acf` beneath the Steam library roots
//     (`~/.steam/steam/steamapps`, `~/.local/share/Steam/steamapps`, and
//     every root `libraryfolders.vdf` declares). Discovery and per-id names
//     are memoized and revalidated by file mtime+size; misses are NOT cached,
//     so a game installed later appears on the next query. Steam not being
//     installed is the normal case and answers empty cheaply.
//  2. ensureIconForClient(): extracts the PE icon of the executable the
//     command line names into the shared cache root as
//     `<cacheRoot>/<cacheIconNameForApplicationId(basename)>.png`, keyed by
//     (path, mtime, size) in a sidecar signature file. The shell resolves
//     that name through its ordinary confined icon path (ADR-0230).
//
// Threading: NOT thread-safe; confine one instance to a single thread (the
// KWin task-facts publisher's GUI thread in production). Every read is
// bounded by the steamappidentity/peicon ceilings; failures are silent and
// yield "no name" / "no icon".
class WineIdentityCache
{
public:
    // cacheRoot: directory the extracted icons are written to (production:
    // defaultCacheRoot()). homeDir: the user's home for Steam discovery.
    // Both injected so tests never touch the real home or cache.
    WineIdentityCache(QString cacheRoot, QString homeDir);

    // The Steam title for a `steam_app_<id>` class, or empty: not a Steam
    // class, Steam absent, or the manifest unreadable/malformed.
    [[nodiscard]] QString steamNameForClass(const QString &resourceClass);

    // Ensures the cached icon for the executable on `cmdline` (last `.exe`
    // argument, mapped through the prefix from `environBytes`) is present and
    // current in the cache root. Does nothing - and writes nothing - for
    // command lines without a Windows executable path, unmappable paths,
    // unreadable or hostile executables, or icons that fail extraction. A
    // stale icon pair for a DIFFERENT source is removed when extraction
    // fails, so the shell never shows the wrong game.
    void ensureIconForClient(const QByteArray &cmdline, const QByteArray &environBytes);

    // The icon name both sides agree on for an application id
    // (`Battle.net.exe` -> `qindaqt-wine-battle.net`), or empty when the id
    // cannot yield a confined name. AGENT-CONTRACT with
    // QindaQt::Shell::Icons::IconRuntime::wineCacheIconNameForAppId: the two
    // implementations MUST stay byte-identical, pinned by the same test
    // vectors, because the compositor writes the file and the shell looks it
    // up by name (ADR-0230).
    [[nodiscard]] static QString cacheIconNameForApplicationId(
        const QString &applicationId);

    // `<generic cache location>/qindaqt/wine-icons` - the production root the
    // shell adds to its confined icon roots. App-independent on purpose: the
    // compositor (KWin) and the shell have different application names.
    [[nodiscard]] static QString defaultCacheRoot();

    // The icon target size used at extraction (kWineIconTargetSize, peicon.h).
    [[nodiscard]] QString cacheRoot() const { return m_cacheRoot; }

private:
    struct FileSignature {
        QString path;
        qint64 mtimeMs = 0;
        qint64 size = 0;
        bool exists = false;
    };

    struct SteamNameEntry {
        QString name;
        FileSignature source;
    };

    [[nodiscard]] static FileSignature signatureOf(const QString &path);
    [[nodiscard]] static bool sameFile(const FileSignature &a, const FileSignature &b);
    // The current steamapps roots: the two home candidates plus every root
    // libraryfolders.vdf declares beneath them, canonicalized and deduplicated,
    // capped at kMaxSteamLibraryRoots + 2. Rediscovered whenever one of the
    // (at most two) libraryfolders.vdf files changes.
    [[nodiscard]] QStringList steamLibraryRoots();
    [[nodiscard]] bool steamRootsStale() const;
    void rediscoverSteamRoots();

    QString m_cacheRoot;
    QString m_homeDir;
    QStringList m_steamRoots;
    FileSignature m_foldersSignatureA;
    FileSignature m_foldersSignatureB;
    bool m_steamRootsValid = false;
    QHash<quint64, SteamNameEntry> m_steamNames;
};

} // namespace QindaQt::Compositor
