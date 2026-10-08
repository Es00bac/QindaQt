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
    if (!anchor || !anchor->window())
        return {};
    // The anchor's actual window selects the output even when names are
    // empty or duplicated. No primary-screen or virtual-union inference.
    auto *selected = anchor->window()->screen();
    if (!selected || !QGuiApplication::screens().contains(selected)
        || (!screenName.isEmpty() && selected->name() != screenName))
        return {};
    const auto size = selected->availableGeometry().size();
    return size.width() > 0 && size.height() > 0 ? size : QSize{};
}
} // namespace QindaQt::Shell::AudioApplet
