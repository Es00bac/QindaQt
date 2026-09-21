// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Session::DesktopControls {

// One installed screensaver, discovered from the `.desktop` entry its package
// shipped (ADR-0226). `token` is the program name and the value persisted in
// the Settings1 key `power.screensaver`; keeping the token and the program
// name identical is what lets values persisted before discovery existed keep
// working.
struct ScreensaverCatalogEntry final {
    QString token;
    QString name;
    QString comment;
    QString iconName;
    // AGENT-CONTRACT: the launch arguments are fixed per token and never read
    // from the desktop entry or the persisted preference: `--screensaver`
    // (which every QindaQt saver documents as covering every connected
    // output) plus the flag that stops what an unattended screen must not do
    // (telemetry, sound). A saver with no house-known flag runs with
    // `--screensaver` alone.
    QStringList arguments;
    // True only when the locker's wallpaper plugin ships a scene that imports
    // this saver's QML module (ADR-0216); the greeter cannot draw a saver
    // that has no QQuickItem, however well the idle path runs it.
    bool showsOnLockScreen = false;

    friend bool operator==(const ScreensaverCatalogEntry &,
                           const ScreensaverCatalogEntry &) = default;
};

// Seam over "which savers are installed". Production scans the system desktop
// entries; tests inject a fixed list.
class ScreensaverCatalog {
public:
    virtual ~ScreensaverCatalog() = default;

    [[nodiscard]] virtual QList<ScreensaverCatalogEntry> entries() const = 0;

    // Convenience lookup over entries(); not virtual so every implementation
    // shares the same token equality rule.
    [[nodiscard]] std::optional<ScreensaverCatalogEntry>
    entry(const QString &token) const;
};

// Discovers installed savers from their `.desktop` entries instead of a
// hard-coded list, so a newly packaged saver appears without a code change.
//
// AGENT-CONTRACT: an entry is a QindaQt screensaver when all of these hold
// (the Screensaver repository owns following them):
//   1. Type=Application and not Hidden=true;
//   2. it attests its purpose: "screensaver" appears, case-insensitively, in
//      Keywords, GenericName, Comment, or one of its action names;
//   3. it proves the launch contract: one of its desktop actions runs the
//      entry's own program with `--screensaver` or `--all-screens`.
//
// AGENT-GUARD: only system application directories are scanned, never the
// user-writable ~/.local/share/applications. A persisted token resolves to a
// program only through this catalog, so persistence can name a
// package-installed saver but never a user-planted one; the user's own
// directory is how a stray file would otherwise become an idle-launched
// process.
class DesktopEntryScreensaverCatalog final : public ScreensaverCatalog {
public:
    // Production: every XDG system applications directory, in priority order.
    DesktopEntryScreensaverCatalog();
    // Test seam: explicit directories instead of the real search path.
    explicit DesktopEntryScreensaverCatalog(QStringList applicationDirectories);

    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override;

    // The two pieces of house knowledge discovery cannot infer: the privacy
    // flag a known saver needs beyond --screensaver, and which savers the
    // locker's wallpaper plugin ships a scene for. A saver absent from both
    // tables still appears and runs; it simply keeps its own defaults and
    // leaves the lock wallpaper alone.
    [[nodiscard]] static QStringList launchArguments(const QString &token);
    [[nodiscard]] static bool shipsLockScreenScene(const QString &token);

    [[nodiscard]] static QStringList systemApplicationDirectories();

private:
    QStringList m_directories;
};

} // namespace QindaQt::Session::DesktopControls
