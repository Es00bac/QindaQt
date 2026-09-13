// SPDX-License-Identifier: GPL-3.0-or-later
#include "qt_customize_output_provider.h"

#include <QGuiApplication>
#include <QScreen>
#include <QThread>

namespace QindaQt::Apps::SettingsCustomize {

QtCustomizeOutputProvider::QtCustomizeOutputProvider(
    QGuiApplication &application, QObject *parent)
    : CustomizeOutputProvider(parent)
    , m_application(application)
{
    for (QScreen *screen : m_application.screens()) {
        attachScreen(screen);
    }
    connect(&m_application, &QGuiApplication::screenAdded, this,
            [this](QScreen *screen) {
                attachScreen(screen);
                advanceRevision();
            });
    connect(&m_application, &QGuiApplication::screenRemoved, this,
            [this](QScreen *) { advanceRevision(); });
    connect(&m_application, &QGuiApplication::primaryScreenChanged, this,
            [this](QScreen *) { advanceRevision(); });
}

void QtCustomizeOutputProvider::attachScreen(QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    const auto changed = [this] { advanceRevision(); };
    m_screenConnections.append(
        connect(screen, &QScreen::geometryChanged, this, changed));
    m_screenConnections.append(
        connect(screen, &QScreen::physicalDotsPerInchChanged, this, changed));
    m_screenConnections.append(
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, changed));
    m_screenConnections.append(
        connect(screen, &QScreen::orientationChanged, this, changed));
}

void QtCustomizeOutputProvider::advanceRevision()
{
    ++m_revision;
    Q_EMIT snapshotChanged();
}

CustomizeOutputSnapshot QtCustomizeOutputProvider::snapshot() const
{
    if (m_application.thread() != QThread::currentThread()) {
        return {{},
                {},
                m_revision,
                QStringLiteral("Display inventory must be read on the GUI thread")};
    }

    CustomizeOutputSnapshot result;
    result.revision = m_revision;
    const QList<QScreen *> screens = m_application.screens();
    result.outputs.reserve(screens.size());
    for (const QScreen *screen : screens) {
        if (screen == nullptr) {
            result.error = QStringLiteral("Qt reported an invalid display entry");
            return result;
        }
        result.outputs.push_back(
            {screen->name(), screen->geometry(), screen->devicePixelRatio()});
    }
    if (const QScreen *primary = m_application.primaryScreen();
        primary != nullptr) {
        result.primaryOutputIds.append(primary->name());
    }
    return result;
}

} // namespace QindaQt::Apps::SettingsCustomize
