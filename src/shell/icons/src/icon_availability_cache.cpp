// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_availability_cache.h>

#include <qindaqt/shell/icons/icon_theme_limits.h>

#include <QFileInfo>

#include <chrono>

namespace QindaQt::Shell::Icons
{
namespace
{

[[nodiscard]] qint64 steadyMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

IconAvailabilityCache::IconAvailabilityCache(QStringList iconRoots, QStringList themeNames,
                                             Clock clock)
    : m_locator(std::move(iconRoots), std::move(themeNames))
    , m_clock(clock ? std::move(clock) : Clock(steadyMilliseconds))
{
}

QString IconAvailabilityCache::keyFor(const QString &iconName, int size, double scale,
                                      bool symbolic)
{
    // The unit separator cannot occur in an acceptable icon name, so the
    // fields cannot run together into one key.
    return iconName + QLatin1Char('\x1f') + QString::number(size) + QLatin1Char('\x1f')
        + QString::number(scale, 'f', 2) + (symbolic ? QStringLiteral("\x1fs") : QString());
}

bool IconAvailabilityCache::hasIcon(const QString &iconName, int size, double scale,
                                    bool symbolic) const
{
    ++m_statistics.queries;
    // AGENT-GUARD: a name the locator refuses is answered here, before any key
    // exists, so hostile input retains no bytes in the memo.
    if (!IconThemeLocator::isAcceptableIconName(iconName)) {
        return false;
    }
    const int normalizedSize = IconThemeLocator::normalizedSize(size);
    const double normalizedScale = IconThemeLocator::normalizedScale(scale);
    const QString key = keyFor(iconName, normalizedSize, normalizedScale, symbolic);
    const qint64 now = m_clock();

    const auto found = m_verdicts.find(key);
    if (found != m_verdicts.end()) {
        Verdict &verdict = found.value();
        if (now < verdict.expiresAtMs) {
            ++m_statistics.memoHits;
            return verdict.available;
        }
        // An expired positive verdict costs one metadata check, not a walk of
        // every theme directory. A file that vanished or stopped being a
        // regular file falls through to a full lookup.
        if (verdict.available && QFileInfo(verdict.path).isFile()) {
            verdict.expiresAtMs = now + kAvailabilityPositiveTtlMs;
            ++m_statistics.revalidations;
            return true;
        }
        m_verdicts.erase(found);
    }

    ++m_statistics.diskLookups;
    // locate() rather than hasIcon(): the remembered path is what lets an
    // expired positive verdict be revalidated with a single stat. It is never
    // returned to a caller.
    const QString path = m_locator.locate(iconName, normalizedSize, normalizedScale, symbolic);
    const bool available = !path.isEmpty();
    makeRoom(now);
    m_verdicts.insert(key,
                      {available, path,
                       now + (available ? kAvailabilityPositiveTtlMs
                                        : kAvailabilityNegativeTtlMs)});
    return available;
}

void IconAvailabilityCache::makeRoom(qint64 now) const
{
    if (m_verdicts.size() < kMaxAvailabilityEntries) {
        return;
    }
    for (auto it = m_verdicts.begin(); it != m_verdicts.end();) {
        it = now >= it->expiresAtMs ? m_verdicts.erase(it) : std::next(it);
    }
    if (m_verdicts.size() >= kMaxAvailabilityEntries) {
        m_verdicts.clear();
    }
}

void IconAvailabilityCache::invalidate()
{
    m_verdicts.clear();
}

IconAvailabilityCache::Statistics IconAvailabilityCache::statistics() const
{
    Statistics statistics = m_statistics;
    statistics.entries = int(m_verdicts.size());
    return statistics;
}

} // namespace QindaQt::Shell::Icons
