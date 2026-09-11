// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_time_schedule_monitor.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>

namespace QindaQt::Services::NightLight {

namespace {
const QString kScheduleServiceName = QStringLiteral("org.kde.NightTime");
} // namespace

NightTimeScheduleMonitor::NightTimeScheduleMonitor(QObject *parent)
    : QObject(parent)
{
}

class QtNightTimeScheduleMonitor::Private {
public:
    explicit Private(const QDBusConnection &bus) : connection(bus) {}

    QDBusConnection connection;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    bool available = false;
    bool started = false;

    void setAvailable(bool value)
    {
        if (available == value) {
            return;
        }
        available = value;
    }
};

QtNightTimeScheduleMonitor::QtNightTimeScheduleMonitor(
    const QDBusConnection &connection, QObject *parent)
    : NightTimeScheduleMonitor(parent),
      d(std::make_unique<Private>(connection))
{
}

QtNightTimeScheduleMonitor::~QtNightTimeScheduleMonitor()
{
    stop();
}

void QtNightTimeScheduleMonitor::start()
{
    if (d->started) {
        return;
    }
    d->started = true;

    // AGENT-NOTE: The owner check is asynchronous: a blocking call cannot be
    // used here (peer-reply delivery is not guaranteed for blocked waits),
    // and D-Bus activation must not be triggered by this monitor, so there is
    // no StartService call either: an unstarted daemon means unavailable
    // schedule truth, by design.
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameHasOwner"));
    call.setArguments({QVariant(kScheduleServiceName)});
    auto *watcher =
        new QDBusPendingCallWatcher(d->connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *finished) {
                finished->deleteLater();
                QDBusPendingReply<bool> reply = *finished;
                if (reply.isError()) {
                    return;
                }
                const bool owned = reply.value();
                if (owned == d->available) {
                    return;
                }
                d->available = owned;
                Q_EMIT scheduleAvailabilityChanged(owned);
            });

    d->watcher = std::make_unique<QDBusServiceWatcher>(
        kScheduleServiceName, d->connection,
        QDBusServiceWatcher::WatchForRegistration
            | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceRegistered, this,
            [this]() {
                d->setAvailable(true);
                Q_EMIT scheduleAvailabilityChanged(true);
            });
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceUnregistered,
            this, [this]() {
                d->setAvailable(false);
                Q_EMIT scheduleAvailabilityChanged(false);
            });
}

void QtNightTimeScheduleMonitor::stop()
{
    if (!d->started) {
        return;
    }
    d->started = false;
    d->watcher.reset();
    if (d->available) {
        d->setAvailable(false);
        Q_EMIT scheduleAvailabilityChanged(false);
    }
}

bool QtNightTimeScheduleMonitor::scheduleAvailable() const
{
    return d->available;
}

} // namespace QindaQt::Services::NightLight
