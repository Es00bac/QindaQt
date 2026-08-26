// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridgroupstackingpolicy.h"

#include "qindaqt/hybrid/windowtopology.h"

#include <QMap>
#include <QPointF>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

class KWinChromeManager;
class ManagedWindowRegistry;

// Applies the pure active-page block plan through KWin's public stacking API,
// then reparents each scene chrome item to that block's topmost member.
// Everything runs synchronously on KWin's compositor thread.
class KWinHybridGroupStacking final
{
public:
    KWinHybridGroupStacking(ManagedWindowRegistry &registry,
                            KWinChromeManager &chrome);

    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   QString *error = nullptr);
    [[nodiscard]] bool raiseContainer(const QString &containerId,
                                      QString *error = nullptr);
    // Answers against KWin's live stack rather than chrome-plan geometry.
    // excludedWindowId is used only while dragging that exact source window.
    [[nodiscard]] bool chromeExposedAt(
        const QString &containerId,
        const QPointF &position,
        const QString &excludedWindowId = {}) const;
    [[nodiscard]] qsizetype publishedGroupCount() const noexcept;
    void clear() noexcept;

private:
    ManagedWindowRegistry &m_registry;
    KWinChromeManager &m_chrome;
    QMap<QString, QStringList> m_membersBottomToTop;
    QMap<QString, QStringList> m_transientsBottomToTop;
    QMap<QString, QString> m_activationRepresentatives;
};

} // namespace QindaQt::Compositor::KWinIntegration

inline qsizetype
QindaQt::Compositor::KWinIntegration::KWinHybridGroupStacking::publishedGroupCount()
    const noexcept
{
    return m_membersBottomToTop.size();
}
