// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/qtypes.h>

#include <functional>

namespace QindaQt::Shell::Icons
{

// AGENT-CONTRACT (ADR-0392): bounded, expiring memo of "does this icon resolve in the
// theme chain", for presentation hints that are asked about on every task-list
// publication (docs/wiki/shell/iconography.md, "IconAvailabilityCache").
//
// Why a separate type and not a cache inside IconThemeLocator: the locator's
// contract is that it never remembers a result, so a file created, removed or
// swapped for an escaping symlink is seen by the very next lookup
// (tst_shell_icons_locator filesystemChangesRemainFresh). That freshness is a
// confinement property and the image provider depends on it. This type sits in
// front of hasIcon() ONLY. It never returns a path and is never consulted by
// the provider, so the pixels a surface loads are still resolved and confined
// fresh. What is remembered is a yes/no used to pick between the real icon
// and a typed fallback glyph, which can be a few seconds old without harm.
//
// Verdicts expire: a positive verdict after kAvailabilityPositiveTtlMs (then
// one metadata check of the remembered file keeps it or forces a fresh
// lookup), a negative verdict after kAvailabilityNegativeTtlMs. A negative
// verdict is deliberately short-lived: the compositor writes the PE icon of a
// Wine program shortly AFTER its window appears (ADR-0230), and the row must
// pick it up on a later publication without waiting for a theme change.
// invalidate() drops everything; the owner calls it, or replaces this object,
// when the icon theme or roots change.
//
// Keys are the bounded request tuple (name, size, scale, symbolic) after the
// locator's own clamping, and a name the locator refuses is answered without
// being retained, so memory stays bounded regardless of caller input. At most
// kMaxAvailabilityEntries are held; when full, expired verdicts go first and
// then the memo restarts empty (re-filling is the same cost as a cold start).
//
// Threading: like IconThemeLocator, an instance is confined to the thread that
// owns it. The memo is mutable under const methods for that reason.
class IconAvailabilityCache
{
public:
    // Monotonic milliseconds. Injectable so expiry is testable without
    // sleeping; the default is std::chrono::steady_clock.
    using Clock = std::function<qint64()>;

    struct Statistics {
        quint64 queries = 0;       // every hasIcon() call
        quint64 memoHits = 0;      // answered from a live verdict, no disk
        quint64 revalidations = 0; // expired positive kept after one stat
        quint64 diskLookups = 0;   // full IconThemeLocator lookups
        int entries = 0;           // verdicts currently remembered
    };

    IconAvailabilityCache(QStringList iconRoots, QStringList themeNames,
                          Clock clock = {});

    // Same validation, chain order and confinement as
    // IconThemeLocator::hasIcon, answered from a live verdict when one exists.
    [[nodiscard]] bool hasIcon(const QString &iconName, int size, double scale = 1.0,
                               bool symbolic = false) const;

    // Forgets every verdict; the next query consults the disk again.
    void invalidate();

    [[nodiscard]] Statistics statistics() const;

private:
    struct Verdict {
        bool available = false;
        QString path;        // remembered file of a positive verdict
        qint64 expiresAtMs = 0;
    };

    [[nodiscard]] static QString keyFor(const QString &iconName, int size, double scale,
                                        bool symbolic);
    void makeRoom(qint64 now) const;

    IconThemeLocator m_locator;
    Clock m_clock;
    mutable QHash<QString, Verdict> m_verdicts;
    mutable Statistics m_statistics;
};

} // namespace QindaQt::Shell::Icons
