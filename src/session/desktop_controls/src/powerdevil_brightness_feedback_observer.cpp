// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/session/desktop_controls/powerdevil_brightness_feedback_observer.h"

#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

#include <QtGlobal>

#include <utility>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr auto ScreenBrightnessService = "org.kde.ScreenBrightness";
constexpr auto ScreenBrightnessPath = "/org/kde/ScreenBrightness";
constexpr auto ScreenBrightnessInterface = "org.kde.ScreenBrightness";
constexpr auto DisplayInterface = "org.kde.ScreenBrightness.Display";
constexpr auto InternalSource = "(internal)";
constexpr auto BrightnessKeyContext = "brightness_key";

QString ownerFor(const QDBusConnection &connection)
{
    if (!connection.isConnected() || connection.interface() == nullptr) {
        return {};
    }
    const QDBusReply<QString> owner = connection.interface()->serviceOwner(
        QString::fromLatin1(ScreenBrightnessService));
    return owner.isValid() ? owner.value() : QString{};
}

QString displayPath(const QString &displayName)
{
    return QString::fromLatin1(ScreenBrightnessPath) + QLatin1Char('/')
        + displayName;
}

} // namespace

PowerDevilBrightnessFeedbackObserver::PowerDevilBrightnessFeedbackObserver(
    QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
}

PowerDevilBrightnessFeedbackObserver::~PowerDevilBrightnessFeedbackObserver()
{
    stop();
}

void PowerDevilBrightnessFeedbackObserver::start()
{
    if (m_serviceWatcher != nullptr) {
        return;
    }
    if (!m_connection.isConnected()) {
        return;
    }

    m_serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(ScreenBrightnessService), m_connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &PowerDevilBrightnessFeedbackObserver::ownerChanged);
    m_signalConnected = m_connection.connect(
        QString::fromLatin1(ScreenBrightnessService),
        QString::fromLatin1(ScreenBrightnessPath),
        QString::fromLatin1(ScreenBrightnessInterface),
        QStringLiteral("BrightnessChanged"), this,
        SLOT(onBrightnessChanged(QString, int, QString, QString)));
    setAvailable(!ownerFor(m_connection).isEmpty());
}

void PowerDevilBrightnessFeedbackObserver::stop()
{
    if (m_serviceWatcher == nullptr && !m_signalConnected) {
        return;
    }
    ++m_generation;
    if (m_signalConnected) {
        m_connection.disconnect(
            QString::fromLatin1(ScreenBrightnessService),
            QString::fromLatin1(ScreenBrightnessPath),
            QString::fromLatin1(ScreenBrightnessInterface),
            QStringLiteral("BrightnessChanged"), this,
            SLOT(onBrightnessChanged(QString, int, QString, QString)));
        m_signalConnected = false;
    }
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
    setAvailable(false);
}

bool PowerDevilBrightnessFeedbackObserver::available() const noexcept
{
    return m_available;
}

void PowerDevilBrightnessFeedbackObserver::ownerChanged(
    const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(oldOwner);
    ++m_generation;
    setAvailable(!newOwner.isEmpty());
}

void PowerDevilBrightnessFeedbackObserver::onBrightnessChanged(
    const QString &displayName, const int brightness,
    const QString &sourceClientName, const QString &sourceClientContext)
{
    if (!m_available || displayName.isEmpty()
        || sourceClientName != QLatin1String(InternalSource)
        || sourceClientContext != QLatin1String(BrightnessKeyContext)) {
        return;
    }
    requestMaximum(displayName, brightness);
}

void PowerDevilBrightnessFeedbackObserver::setAvailable(const bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availabilityChanged(m_available);
}

void PowerDevilBrightnessFeedbackObserver::requestMaximum(
    const QString &displayName, const int brightness)
{
    const quint64 generation = m_generation;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(ScreenBrightnessService), displayPath(displayName),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    call.setArguments({QString::fromLatin1(DisplayInterface),
                       QStringLiteral("MaxBrightness")});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, brightness] {
                const QDBusPendingReply<QDBusVariant> reply = *watcher;
                watcher->deleteLater();
                if (generation != m_generation || !m_available || reply.isError()) {
                    return;
                }
                bool valid = false;
                const int maximum = reply.value().variant().toInt(&valid);
                if (!valid || maximum <= 0 || brightness < 0 || brightness > maximum) {
                    return;
                }
                const int percent = qBound(
                    0, qRound(static_cast<double>(brightness) * 100.0
                              / static_cast<double>(maximum)),
                    100);
                Q_EMIT brightnessFeedbackRequested(percent);
            });
}

} // namespace QindaQt::Session::DesktopControls
