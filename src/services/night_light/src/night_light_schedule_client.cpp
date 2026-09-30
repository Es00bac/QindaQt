// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_client.h>

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <QUuid>

#include <memory>
#include <limits>
#include <QTimeZone>

namespace QindaQt::Services::NightLight {
namespace {
const QString kService = QStringLiteral("org.qindaqt.NightLight");
const QString kPath = QStringLiteral("/org/qindaqt/NightLight");
const QString kInterface = QStringLiteral("org.qindaqt.NightLight.Schedule1");
const QString kSignal = QStringLiteral("ScheduleFrame");
const QString kBus = QStringLiteral("org.freedesktop.DBus");
const QString kBusPath = QStringLiteral("/org/freedesktop/DBus");
const QString kBusInterface = QStringLiteral("org.freedesktop.DBus");
constexpr int kTimeoutMs = 2'000;

bool exact(const QVariantMap &map, const char *key, int type, QVariant *out)
{
    const auto it = map.constFind(QString::fromLatin1(key));
    if (it == map.cend() || it->metaType().id() != type) return false;
    *out = *it;
    return true;
}

std::optional<NightLightScheduleState> decode(quint64 revision,
                                               const QVariantMap &map)
{
    if (map.size() != 21) return std::nullopt;
    NightLightScheduleState state;
    QVariant v;
    if (!exact(map, "active", QMetaType::Bool, &v)) return std::nullopt;
    state.settings.output.active = v.toBool();
    if (!exact(map, "mode", QMetaType::QString, &v)) return std::nullopt;
    const auto mode = modeFromConfigToken(v.toString());
    if (!mode) return std::nullopt;
    state.settings.output.mode = *mode;
    if (!exact(map, "dayTemperatureKelvin", QMetaType::Int, &v)) return std::nullopt;
    state.settings.output.dayTemperatureKelvin = v.toInt();
    if (!exact(map, "nightTemperatureKelvin", QMetaType::Int, &v)) return std::nullopt;
    state.settings.output.nightTemperatureKelvin = v.toInt();
    if (!exact(map, "disabledOutputs", QMetaType::QStringList, &v)) return std::nullopt;
    state.settings.output.disabledOutputs = v.toStringList();
    if (!exact(map, "disabledOutputUuids", QMetaType::QStringList, &v)) return std::nullopt;
    state.disabledOutputUuids = v.toStringList();
    if (state.disabledOutputUuids.size() > 32) return std::nullopt;
    for (qsizetype i = 0; i < state.disabledOutputUuids.size(); ++i) {
        const QString &uuid = state.disabledOutputUuids.at(i);
        if (uuid.isEmpty() || uuid.toUtf8().size() > 128
            || state.disabledOutputUuids.indexOf(uuid) != i) return std::nullopt;
    }
    if (!exact(map, "outputIdentityAvailable", QMetaType::Bool, &v)) return std::nullopt;
    state.outputIdentityAvailable = v.toBool();
    if (state.outputIdentityAvailable
        ? state.disabledOutputUuids.size() != state.settings.output.disabledOutputs.size()
        : !state.disabledOutputUuids.isEmpty()) return std::nullopt;
    if (!exact(map, "scheduleSource", QMetaType::QString, &v)) return std::nullopt;
    const auto source = sourceFromConfigToken(v.toString());
    if (!source) return std::nullopt;
    state.settings.schedule.source = *source;
    if (!exact(map, "automaticLocation", QMetaType::Bool, &v)) return std::nullopt;
    state.settings.schedule.automaticLocation = v.toBool();
    if (!exact(map, "latitudeDegrees", QMetaType::Double, &v)) return std::nullopt;
    state.settings.schedule.latitudeDegrees = v.toDouble();
    if (!exact(map, "longitudeDegrees", QMetaType::Double, &v)) return std::nullopt;
    state.settings.schedule.longitudeDegrees = v.toDouble();
    if (!exact(map, "sunriseStart", QMetaType::QString, &v)) return std::nullopt;
    state.settings.schedule.sunriseStart = QTime::fromString(v.toString(), Qt::ISODateWithMs);
    if (!state.settings.schedule.sunriseStart.isValid())
        state.settings.schedule.sunriseStart = QTime::fromString(v.toString(), QStringLiteral("HH:mm:ss"));
    if (!exact(map, "sunsetStart", QMetaType::QString, &v)) return std::nullopt;
    state.settings.schedule.sunsetStart = QTime::fromString(v.toString(), QStringLiteral("HH:mm:ss"));
    if (!exact(map, "transitionSeconds", QMetaType::Int, &v)) return std::nullopt;
    state.settings.schedule.transitionSeconds = v.toInt();
    if (!isValidOutput(state.settings.output) || !isValidSchedule(state.settings.schedule))
        return std::nullopt;
    if (!exact(map, "available", QMetaType::Bool, &v)) return std::nullopt;
    state.schedule.available = v.toBool();
    if (!exact(map, "daylight", QMetaType::Bool, &v)) return std::nullopt;
    state.schedule.daylight = v.toBool();
    auto readTime = [&map](const char *key, QDateTime *out) {
        QVariant item;
        if (!exact(map, key, QMetaType::ULongLong, &item)) return false;
        const quint64 msecs = item.toULongLong();
        if (msecs > quint64(std::numeric_limits<qint64>::max())) return false;
        *out = msecs == 0 ? QDateTime() : QDateTime::fromMSecsSinceEpoch(qint64(msecs), QTimeZone::UTC);
        return msecs == 0 || out->isValid();
    };
    if (!readTime("previousStartMs", &state.schedule.previous.start)
        || !readTime("previousEndMs", &state.schedule.previous.end)
        || !readTime("nextStartMs", &state.schedule.next.start)
        || !readTime("nextEndMs", &state.schedule.next.end)) return std::nullopt;
    const bool needsTransitions = state.settings.output.active
        && state.settings.output.mode == Mode::DarkLight;
    if (state.schedule.available && needsTransitions) {
        if (!state.schedule.previous.start.isValid() || !state.schedule.previous.end.isValid()
            || !state.schedule.next.start.isValid() || !state.schedule.next.end.isValid()
            || state.schedule.previous.start >= state.schedule.previous.end
            || state.schedule.next.start >= state.schedule.next.end) return std::nullopt;
    } else if (!state.schedule.available
               && (state.schedule.previous.start.isValid() || state.schedule.next.start.isValid())) {
        return std::nullopt;
    }
    if (!exact(map, "diagnostic", QMetaType::QString, &v)) return std::nullopt;
    state.schedule.diagnostic = v.toString();
    state.revision = revision;
    return state;
}
} // namespace

class QtNightLightScheduleClient::Private final {
public:
    explicit Private(const QDBusConnection &bus) : connection(bus) {}
    QDBusConnection connection;
    std::unique_ptr<QDBusServiceWatcher> serviceWatcher;
    QTimer receiptTimeout;
    QString owner;
    QByteArray nonce;
    quint64 cookie = 0;
    quint64 revision = 0;
    quint64 generation = 0;
    bool started = false;
    std::optional<NightLightScheduleState> state;
};

QtNightLightScheduleClient::QtNightLightScheduleClient(
    const QDBusConnection &connection, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(connection))
{
    d->receiptTimeout.setSingleShot(true);
    d->receiptTimeout.setInterval(kTimeoutMs);
    connect(&d->receiptTimeout, &QTimer::timeout, this, [this] {
        clearState(QStringLiteral("schedule service did not confirm its subscription"));
    });
    d->serviceWatcher = std::make_unique<QDBusServiceWatcher>(
        kService, d->connection,
        QDBusServiceWatcher::WatchForRegistration
            | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(d->serviceWatcher.get(), &QDBusServiceWatcher::serviceRegistered,
            this, &QtNightLightScheduleClient::resolveOwner);
    connect(d->serviceWatcher.get(), &QDBusServiceWatcher::serviceUnregistered,
            this, [this] { bindOwner({}); });
}

QtNightLightScheduleClient::~QtNightLightScheduleClient() { stop(); }
const std::optional<NightLightScheduleState> &QtNightLightScheduleClient::state() const noexcept
{ return d->state; }

void QtNightLightScheduleClient::start()
{
    if (d->started) return;
    d->started = true;
    resolveOwner();
    QDBusMessage activate = QDBusMessage::createMethodCall(
        kBus, kBusPath, kBusInterface, QStringLiteral("StartServiceByName"));
    activate.setArguments({QVariant(kService), QVariant(uint(0))});
    d->connection.asyncCall(activate, kTimeoutMs);
}

void QtNightLightScheduleClient::stop()
{
    if (!d->started) return;
    d->started = false;
    ++d->generation;
    d->receiptTimeout.stop();
    if (!d->owner.isEmpty() && d->cookie != 0) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            d->owner, kPath, kInterface, QStringLiteral("Unsubscribe"));
        call.setArguments({QVariant(d->nonce), QVariant::fromValue(qulonglong(d->cookie))});
        d->connection.asyncCall(call, kTimeoutMs);
    }
    bindOwner({});
}

void QtNightLightScheduleClient::resolveOwner()
{
    if (!d->started) return;
    const quint64 generation = ++d->generation;
    QDBusMessage call = QDBusMessage::createMethodCall(
        kBus, kBusPath, kBusInterface, QStringLiteral("GetNameOwner"));
    call.setArguments({QVariant(kService)});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call, kTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        QDBusPendingReply<QString> reply = *finished;
        finished->deleteLater();
        if (!d->started || generation != d->generation) return;
        bindOwner(reply.isError() ? QString{} : reply.value());
    });
}

void QtNightLightScheduleClient::bindOwner(const QString &owner)
{
    if (owner == d->owner) return;
    if (!d->owner.isEmpty()) {
        d->connection.disconnect(d->owner, kPath, kInterface, kSignal, this,
            SLOT(receiveFrame(QByteArray,qulonglong,qulonglong,QVariantMap)));
        if (d->cookie != 0) {
            QDBusMessage call = QDBusMessage::createMethodCall(
                d->owner, kPath, kInterface, QStringLiteral("Unsubscribe"));
            call.setArguments({QVariant(d->nonce), QVariant::fromValue(qulonglong(d->cookie))});
            d->connection.asyncCall(call, kTimeoutMs);
        }
    }
    d->owner.clear();
    d->nonce.clear();
    d->cookie = 0;
    d->revision = 0;
    ++d->generation;
    d->receiptTimeout.stop();
    if (d->state) {
        d->state.reset();
        Q_EMIT unavailable(owner.isEmpty() ? QStringLiteral("schedule service disappeared")
                                           : QStringLiteral("schedule service owner changed"));
    }
    if (!d->started || owner.isEmpty()) return;
    d->owner = owner;
    d->nonce = QUuid::createUuid().toRfc4122();
    if (!d->connection.connect(d->owner, kPath, kInterface, kSignal, this,
            SLOT(receiveFrame(QByteArray,qulonglong,qulonglong,QVariantMap)))) {
        const QString reason = QStringLiteral("cannot subscribe to the exact schedule-service owner");
        d->owner.clear();
        Q_EMIT unavailable(reason);
        return;
    }
    subscribe();
}

void QtNightLightScheduleClient::subscribe()
{
    if (!d->started || d->owner.isEmpty() || d->nonce.size() != 16) return;
    d->cookie = 0;
    d->revision = 0;
    QDBusMessage call = QDBusMessage::createMethodCall(
        d->owner, kPath, kInterface, QStringLiteral("Subscribe"));
    call.setArguments({QVariant(d->nonce)});
    const quint64 generation = d->generation;
    d->receiptTimeout.start();
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(call, kTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        QDBusPendingReply<> reply = *finished;
        finished->deleteLater();
        if (!d->started || generation != d->generation || !reply.isError()) return;
        d->receiptTimeout.stop();
        clearState(QStringLiteral("schedule service refused subscription"));
    });
}

void QtNightLightScheduleClient::receiveFrame(const QByteArray &nonce,
                                               qulonglong cookie,
                                               qulonglong revision,
                                               const QVariantMap &values)
{
    if (!d->started || d->owner.isEmpty() || nonce != d->nonce || cookie == 0
        || revision == 0 || (d->cookie != 0 && cookie != d->cookie)
        || revision <= d->revision) return;
    const auto frame = decode(revision, values);
    if (!frame) {
        clearState(QStringLiteral("schedule service sent an invalid frame"));
        return;
    }
    d->cookie = cookie;
    d->revision = revision;
    d->receiptTimeout.stop();
    d->state = *frame;
    Q_EMIT stateChanged(*d->state);
}

void QtNightLightScheduleClient::clearState(QString reason)
{
    d->receiptTimeout.stop();
    if (d->state) d->state.reset();
    Q_EMIT unavailable(reason);
}

} // namespace QindaQt::Services::NightLight
