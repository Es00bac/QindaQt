// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_applet_controller.h"
#include <QtGui/QGuiApplication>
#include <limits>
#include <QtGui/QScreen>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

namespace QindaQt::Shell::AudioApplet {
void AudioAppletController::initializePopupGeometry()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!application)
        return;
    const auto changed = [this] {
        m_popupGeometryRevision = m_popupGeometryRevision == std::numeric_limits<int>::max()
            ? 0 : m_popupGeometryRevision + 1;
        Q_EMIT popupGeometryChanged();
    };
    const auto observe = [this, changed](QScreen *screen) {
        // QObject disconnects these borrowed-screen notifications on removal/
        // destruction. Queries re-resolve current screens; no pointer is kept.
        connect(screen, &QScreen::availableGeometryChanged, this, changed);
        connect(screen, &QScreen::geometryChanged, this, changed);
        changed();
    };
    connect(application, &QGuiApplication::screenAdded, this, observe);
    connect(application, &QGuiApplication::screenRemoved, this, changed);
    for (auto *screen : QGuiApplication::screens())
        observe(screen);
}
QSize AudioAppletController::popupAvailableSize(QQuickItem *anchor,
                                                const QString &screenName) const
{
    if (!anchor || !anchor->window() || screenName.isEmpty())
        return {};
    QScreen *selected = nullptr;
    for (auto *screen : QGuiApplication::screens()) {
        if (screen->name() != screenName)
            continue;
        // Do not guess among duplicate output names, or consult primary.
        if (selected)
            return {};
        selected = screen;
    }
    if (!selected)
        return {};
    return selected->availableGeometry().size();
}
} // namespace QindaQt::Shell::AudioApplet
