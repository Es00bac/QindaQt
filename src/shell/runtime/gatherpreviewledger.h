// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Shell {

// Keeps a Gather snapshot for as long as its window is still visible. Task
// generations fence activation, but may advance while the same window stays
// open; using them as a preview-cache key made thumbnails flash and caused
// repeated ScreenShot2 calls on a busy desktop (ADR-0243).
class GatherPreviewLedger final {
public:
    // Returns true when the visible identity set changed. The caller cancels
    // pending captures before submitting newly needed requests.
    bool reconcile(const QSet<QString> &visible)
    {
        if (m_visible == visible)
            return false;
        m_visible = visible;
        for (auto it = m_urls.begin(); it != m_urls.end();) {
            if (!visible.contains(it.key()))
                it = m_urls.erase(it);
            else
                ++it;
        }
        m_requested.clear();
        for (auto it = m_urls.cbegin(); it != m_urls.cend(); ++it)
            m_requested.insert(it.key());
        return true;
    }

    bool markRequested(const QString &windowId)
    {
        if (!m_visible.contains(windowId) || m_requested.contains(windowId))
            return false;
        m_requested.insert(windowId);
        return true;
    }

    bool accept(const QString &windowId, const QString &url)
    {
        if (!m_visible.contains(windowId) || !m_requested.contains(windowId))
            return false;
        m_urls.insert(windowId, url);
        return true;
    }

    void clear()
    {
        m_visible.clear();
        m_requested.clear();
        m_urls.clear();
    }

    [[nodiscard]] const QVariantMap &urls() const noexcept { return m_urls; }

private:
    QSet<QString> m_visible;
    QSet<QString> m_requested;
    QVariantMap m_urls;
};

} // namespace QindaQt::Shell
