// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_state_port.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>
#include <QtCore/QTimeZone>

#include <memory>
#include <optional>

namespace QindaQt::Services::NightLight {

namespace {

const QString kServiceName = QStringLiteral("org.kde.KWin.NightLight");
const QString kObjectPath = QStringLiteral("/org/kde/KWin/NightLight");
const QString kInterfaceName = QStringLiteral("org.kde.KWin.NightLight");
const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");

// AGENT-GUARD: Every frame field is validated before publication. KWin is
// trusted for live output, not for hostile input: a frame with an unknown
// mode integer, a temperature outside the documented window, a wrong property
// type, or missing properties is rejected whole, and the last complete status
// stays published with a degraded() report (ADR-0136). D-Bus decoding rules:
// type `u` decodes as uint, type `t` decodes as qulonglong epoch seconds.
bool readBoolProperty(const QVariantMap &properties, const QString &name,
                      bool &target)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd()) {
        return false;
    }
    if (it->metaType().id() != QMetaType::Bool) {
        return false;
    }
    target = it->toBool();
    return true;
}

bool readUIntProperty(const QVariantMap &properties, const QString &name,
                      quint32 &target)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd()) {
        return false;
    }
    if (it->metaType().id() != QMetaType::UInt) {
        return false;
    }
    target = it->toUInt();
    return true;
}

bool readTimestampProperty(const QVariantMap &properties, const QString &name,
                           QDateTime &target)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd()) {
        return false;
    }
    if (it->metaType().id() != QMetaType::ULongLong) {
        return false;
    }
    const quint64 seconds = it->toULongLong();
    if (seconds == 0) {
        target = QDateTime();
        return true;
    }
    target = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds),
                                           QTimeZone::UTC);
    return target.isValid();
}

std::optional<NightLightStatus> statusFromProperties(
    const QVariantMap &properties)
{
    NightLightStatus status;
    quint32 modeValue = 0;
    quint32 currentTemperature = 0;
    quint32 targetTemperature = 0;
    quint32 previousDuration = 0;
    quint32 scheduledDuration = 0;

    if (!readBoolProperty(properties, QStringLiteral("available"),
                          status.available)
        || !readBoolProperty(properties, QStringLiteral("enabled"),
                             status.enabled)
        || !readBoolProperty(properties, QStringLiteral("running"),
                             status.running)
        || !readBoolProperty(properties, QStringLiteral("inhibited"),
                             status.inhibited)
        || !readBoolProperty(properties, QStringLiteral("daylight"),
                             status.daylight)
        || !readUIntProperty(properties, QStringLiteral("mode"), modeValue)
        || !readUIntProperty(properties, QStringLiteral("currentTemperature"),
                             currentTemperature)
        || !readUIntProperty(properties, QStringLiteral("targetTemperature"),
                             targetTemperature)
        || !readUIntProperty(properties,
                             QStringLiteral("previousTransitionDuration"),
                             previousDuration)
        || !readUIntProperty(properties,
                             QStringLiteral("scheduledTransitionDuration"),
                             scheduledDuration)
        || !readTimestampProperty(
               properties, QStringLiteral("previousTransitionDateTime"),
               status.previousTransition.dateTime)
        || !readTimestampProperty(
               properties, QStringLiteral("scheduledTransitionDateTime"),
               status.scheduledTransition.dateTime)) {
        return std::nullopt;
    }

    const auto mode = modeFromBusValue(modeValue);
    if (!mode.has_value()) {
        return std::nullopt;
    }
    status.mode = *mode;

    // Temperatures inside a transition stay within the documented window
    // (1000 K through the 6500 K neutral point); anything else is hostile.
    if (currentTemperature < kTemperatureFloorKelvin
        || currentTemperature > kTemperatureCeilingKelvin
        || targetTemperature < kTemperatureFloorKelvin
        || targetTemperature > kTemperatureCeilingKelvin) {
        return std::nullopt;
    }
    status.currentTemperatureKelvin = static_cast<int>(currentTemperature);
    status.targetTemperatureKelvin = static_cast<int>(targetTemperature);

    status.previousTransition.durationMilliseconds = previousDuration;
    status.scheduledTransition.durationMilliseconds = scheduledDuration;
    return status;
}

} // namespace

class QtNightLightStatePort::Private {
public:
    explicit Private(const QDBusConnection &bus) : connection(bus) {}

    QDBusConnection connection;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    NightLightStatus lastStatus;
    bool publishedAny = false;
    bool started = false;
};

NightLightStatePort::NightLightStatePort(QObject *parent)
    : QObject(parent)
{
}

QtNightLightStatePort::QtNightLightStatePort(const QDBusConnection &connection,
                                             QObject *parent)
    : NightLightStatePort(parent),
      d(std::make_unique<Private>(connection))
{
}

QtNightLightStatePort::~QtNightLightStatePort()
{
    stop();
}

void QtNightLightStatePort::publishStatus(const NightLightStatus &status)
{
    if (d->publishedAny && d->lastStatus == status) {
        return;
    }
    d->lastStatus = status;
    d->publishedAny = true;
    Q_EMIT statusChanged(d->lastStatus);
}

// AGENT-GUARD: Every bus read is asynchronous. QDBus::Block against a peer
// connection never completes reliably in this environment (only
// daemon-generated replies return), so a synchronous GetAll would hang until
// timeout. The refreshes are queued out of the bus dispatch and complete
// through pending-call watchers.
void QtNightLightStatePort::beginRefresh(bool serviceAppearance)
{
    if (!d->started) {
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        kServiceName, kObjectPath, kPropertiesInterface,
        QStringLiteral("GetAll"));
    call.setArguments({QVariant(kInterfaceName)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call),
                                                this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, serviceAppearance](QDBusPendingCallWatcher *finished) {
                finishRefresh(finished, serviceAppearance);
            });
}

void QtNightLightStatePort::finishRefresh(QDBusPendingCallWatcher *watcher,
                                          bool serviceAppearance)
{
    watcher->deleteLater();
    QDBusPendingReply<QVariantMap> reply = *watcher;
    if (reply.isError()) {
        if (serviceAppearance) {
            // AGENT-GUARD: The service appeared but its first frame is
            // unusable or the read failed. Publish the explicit unavailable
            // frame rather than leaving the previous status, so consumers
            // cannot mistake a hostile or vanished producer for a working
            // one.
            publishStatus(NightLightStatus{});
        }
        Q_EMIT degraded(reply.error().message().isEmpty()
                            ? QStringLiteral("night light read failed")
                            : reply.error().message());
        return;
    }
    const std::optional<NightLightStatus> status = statusFromProperties(
        reply.value());
    if (!status.has_value()) {
        if (serviceAppearance) {
            publishStatus(NightLightStatus{});
        }
        Q_EMIT degraded(QStringLiteral("hostile night light properties"));
        return;
    }
    publishStatus(*status);
}

void QtNightLightStatePort::refreshChangeHint()
{
    beginRefresh(false);
}

void QtNightLightStatePort::refreshServiceFrame()
{
    beginRefresh(true);
}

void QtNightLightStatePort::handlePropertiesChanged(
    const QString &interfaceName, const QVariantMap &, const QStringList &)
{
    if (interfaceName != kInterfaceName) {
        return;
    }
    // PropertiesChanged is an invalidation hint: the authoritative frame is
    // always the full re-read that follows, queued out of the dispatch.
    beginRefresh(false);
}

void QtNightLightStatePort::start()
{
    if (d->started) {
        return;
    }
    d->started = true;

    d->watcher = std::make_unique<QDBusServiceWatcher>(
        kServiceName, d->connection,
        QDBusServiceWatcher::WatchForRegistration
            | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceRegistered, this,
            [this]() {
                // Queued: this handler runs inside the connection's message
                // dispatch.
                QMetaObject::invokeMethod(this,
                                          &QtNightLightStatePort::
                                              refreshServiceFrame,
                                          Qt::QueuedConnection);
            });
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceUnregistered,
            this, [this]() {
                publishStatus(NightLightStatus{});
            });

    // PropertiesChanged is treated as an invalidation hint only; the frame
    // that follows is always re-read through GetAll, so a hostile partial
    // notification can never publish on its own.
    d->connection.connect(kServiceName, kObjectPath, kPropertiesInterface,
                          QStringLiteral("PropertiesChanged"), this,
                          SLOT(handlePropertiesChanged(QString, QVariantMap,
                                                       QStringList)));

    // The service may already be up; refresh once immediately (asynchronously,
    // like every other read).
    beginRefresh(true);
}

void QtNightLightStatePort::stop()
{
    if (!d->started) {
        return;
    }
    d->started = false;
    d->watcher.reset();
    d->connection.disconnect(kServiceName, kObjectPath, kPropertiesInterface,
                             QStringLiteral("PropertiesChanged"), this,
                             SLOT(handlePropertiesChanged(QString, QVariantMap,
                                                          QStringList)));
}

void QtNightLightStatePort::preview(int temperatureKelvin)
{
    if (temperatureKelvin < kTemperatureFloorKelvin
        || temperatureKelvin > kTemperatureCeilingKelvin) {
        Q_EMIT degraded(QStringLiteral("preview temperature out of range"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        kServiceName, kObjectPath, kInterfaceName, QStringLiteral("preview"));
    call.setArguments(
        {QVariant::fromValue(static_cast<quint32>(temperatureKelvin))});
    d->connection.asyncCall(call);
}

void QtNightLightStatePort::stopPreview()
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        kServiceName, kObjectPath, kInterfaceName,
        QStringLiteral("stopPreview"));
    d->connection.asyncCall(call);
}

} // namespace QindaQt::Services::NightLight
