// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_service.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_protocol/display_types.h>

#include <QDBusContext>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>

#include <memory>

namespace QindaQt::Services::NightLight {
namespace {
const QString kService = QStringLiteral("org.qindaqt.NightLight");
const QString kPath = QStringLiteral("/org/qindaqt/NightLight");
const QString kInterface = QStringLiteral("org.qindaqt.NightLight.Schedule1");
const QStringList kKeys{
    QStringLiteral("display.nightLight.active"),
    QStringLiteral("display.nightLight.mode"),
    QStringLiteral("display.nightLight.dayTemperatureKelvin"),
    QStringLiteral("display.nightLight.nightTemperatureKelvin"),
    QStringLiteral("display.nightLight.scheduleSource"),
    QStringLiteral("display.nightLight.automaticLocation"),
    QStringLiteral("display.nightLight.latitudeDegrees"),
    QStringLiteral("display.nightLight.longitudeDegrees"),
    QStringLiteral("display.nightLight.sunriseStart"),
    QStringLiteral("display.nightLight.sunsetStart"),
    QStringLiteral("display.nightLight.transitionSeconds"),
    QStringLiteral("display.nightLight.disabledOutputs")};

QString valueString(const QVariantMap &values, const QString &key, const QString &fallback)
{ const auto it = values.constFind(key); return it == values.cend() ? fallback : it->toString(); }
int valueInt(const QVariantMap &values, const QString &key, int fallback)
{ const auto it = values.constFind(key); return it == values.cend() ? fallback : it->toInt(); }
bool valueBool(const QVariantMap &values, const QString &key, bool fallback)
{ const auto it = values.constFind(key); return it == values.cend() ? fallback : it->toBool(); }
double valueDouble(const QVariantMap &values, const QString &key, double fallback)
{ const auto it = values.constFind(key); return it == values.cend() ? fallback : it->toDouble(); }

class ScheduleObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.NightLight.Schedule1")
public:
    explicit ScheduleObject(const QDBusConnection &bus) : m_bus(bus) {}
    void setFrame(NightLightSettings settings, ScheduleFrame schedule, bool ready,
                  bool identityReady, QString identityDiagnostic,
                  QStringList runtimeOutputUuids, quint64 revision)
    {
        m_settings = std::move(settings);
        m_schedule = std::move(schedule);
        m_ready = ready;
        m_identityReady = identityReady;
        m_identityDiagnostic = std::move(identityDiagnostic);
        m_runtimeOutputUuids = std::move(runtimeOutputUuids);
        m_revision = revision;
    }

public Q_SLOTS:
    void Subscribe(const QByteArray &nonce)
    {
        if (!calledFromDBus()) return;
        const QString caller = message().service();
        if (!caller.startsWith(QLatin1Char(':')) || nonce.size() != 16) return;
        quint64 cookie = 0;
        do { cookie = QRandomGenerator::global()->generate64(); }
        while (cookie == 0 || m_subscriptions.contains(cookie));
        m_subscriptions.insert(cookie, {caller, nonce});
        sendFrame(cookie, nonce, m_revision);
    }
    void Unsubscribe(const QByteArray &nonce, qulonglong cookie)
    {
        if (!calledFromDBus()) return;
        const QString caller = message().service();
        const auto it = m_subscriptions.find(cookie);
        if (it != m_subscriptions.end() && it->owner == caller && it->nonce == nonce)
            m_subscriptions.erase(it);
    }

public:
    void ownerLost(const QString &owner)
    {
        for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
            if (it->owner == owner) it = m_subscriptions.erase(it); else ++it;
        }
    }
    void publish()
    {
        for (auto it = m_subscriptions.cbegin(); it != m_subscriptions.cend(); ++it)
            sendFrame(it.key(), it->nonce, m_revision);
    }
private:
    struct Subscription { QString owner; QByteArray nonce; };
    QVariantMap frame() const
    {
        QVariantMap values;
        values.insert(QStringLiteral("active"), m_settings.output.active);
        values.insert(QStringLiteral("mode"), modeToConfigToken(m_settings.output.mode));
        values.insert(QStringLiteral("dayTemperatureKelvin"), qint32(m_settings.output.dayTemperatureKelvin));
        values.insert(QStringLiteral("nightTemperatureKelvin"), qint32(m_settings.output.nightTemperatureKelvin));
        values.insert(QStringLiteral("disabledOutputs"), m_settings.output.disabledOutputs);
        values.insert(QStringLiteral("disabledOutputUuids"), m_runtimeOutputUuids);
        values.insert(QStringLiteral("outputIdentityAvailable"), m_identityReady);
        values.insert(QStringLiteral("scheduleSource"), sourceToConfigToken(m_settings.schedule.source));
        values.insert(QStringLiteral("automaticLocation"), m_settings.schedule.automaticLocation);
        values.insert(QStringLiteral("latitudeDegrees"), m_settings.schedule.latitudeDegrees);
        values.insert(QStringLiteral("longitudeDegrees"), m_settings.schedule.longitudeDegrees);
        values.insert(QStringLiteral("sunriseStart"), m_settings.schedule.sunriseStart.toString(QStringLiteral("HH:mm:ss")));
        values.insert(QStringLiteral("sunsetStart"), m_settings.schedule.sunsetStart.toString(QStringLiteral("HH:mm:ss")));
        values.insert(QStringLiteral("transitionSeconds"), qint32(m_settings.schedule.transitionSeconds));
        values.insert(QStringLiteral("available"), m_ready && m_schedule.available && m_identityReady);
        values.insert(QStringLiteral("daylight"), m_ready && m_schedule.daylight);
        const auto millis = [](const QDateTime &date) -> qulonglong {
            return date.isValid() ? qulonglong(date.toMSecsSinceEpoch()) : 0;
        };
        values.insert(QStringLiteral("previousStartMs"), millis(m_schedule.previous.start));
        values.insert(QStringLiteral("previousEndMs"), millis(m_schedule.previous.end));
        values.insert(QStringLiteral("nextStartMs"), millis(m_schedule.next.start));
        values.insert(QStringLiteral("nextEndMs"), millis(m_schedule.next.end));
        const QString diagnostic = !m_ready
            ? QStringLiteral("Settings1 preferences are unavailable")
            : !m_identityReady ? m_identityDiagnostic : m_schedule.diagnostic;
        values.insert(QStringLiteral("diagnostic"), diagnostic);
        return values;
    }
    void sendFrame(quint64 cookie, const QByteArray &nonce, quint64 revision) const
    {
        const auto it = m_subscriptions.constFind(cookie);
        if (it == m_subscriptions.cend() || it->nonce != nonce) return;
        QDBusMessage signal = QDBusMessage::createTargetedSignal(
            it->owner, kPath, kInterface, QStringLiteral("ScheduleFrame"));
        signal.setArguments({QVariant(nonce), QVariant::fromValue(qulonglong(cookie)),
                             QVariant::fromValue(qulonglong(revision)), QVariant(frame())});
        m_bus.send(signal);
    }
    QDBusConnection m_bus;
    NightLightSettings m_settings;
    ScheduleFrame m_schedule;
    bool m_ready = false;
    bool m_identityReady = true;
    QString m_identityDiagnostic;
    QStringList m_runtimeOutputUuids;
    quint64 m_revision = 1;
    QHash<quint64, Subscription> m_subscriptions;
};
} // namespace

const QStringList &nightLightSettingKeys() { return kKeys; }

class NightLightScheduleService::Private final {
public:
    Private(QDBusConnection bus,
            QindaQt::Services::SettingsClient::SettingsClient &client,
            QindaQt::DisplayClient::Client &displayClient,
            NightLightScheduleService &service)
        : connection(std::move(bus)), settings(client), display(displayClient),
          object(connection) { Q_UNUSED(service); }
    QDBusConnection connection;
    QindaQt::Services::SettingsClient::SettingsClient &settings;
    QindaQt::DisplayClient::Client &display;
    ScheduleObject object;
    QTimer refreshTimer;
    NightLightSettings values;
    ScheduleFrame schedule;
    quint64 revision = 1;
    bool ready = false;
    bool identityReady = true;
    QString identityDiagnostic;
    QStringList runtimeOutputUuids;
    bool started = false;
};

NightLightScheduleService::NightLightScheduleService(
    const QDBusConnection &connection,
    QindaQt::Services::SettingsClient::SettingsClient &settings,
    QindaQt::DisplayClient::Client &display,
    QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(connection, settings, display, *this))
{
    connect(&settings, &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
            this, [this] { refreshSettings(); });
    connect(&settings, &QindaQt::Services::SettingsClient::SettingsClient::stateChanged,
            this, [this] {
        if (d->settings.state() != QindaQt::Services::SettingsClient::ClientState::Ready
            && d->ready) {
            d->ready = false;
            d->schedule = {};
            ++d->revision;
            publish();
        }
    });
    connect(&display, &QindaQt::DisplayClient::Client::snapshotChanged,
            this, [this] { refreshSettings(); });
    connect(&display, &QindaQt::DisplayClient::Client::stateChanged,
            this, [this] { refreshSettings(); });
    d->refreshTimer.setInterval(60'000);
    connect(&d->refreshTimer, &QTimer::timeout, this, [this] {
        ++d->revision;
        publish();
    });
    connect(d->connection.interface(), &QDBusConnectionInterface::serviceOwnerChanged,
            this, [this](const QString &name, const QString &, const QString &) {
        if (name.startsWith(QLatin1Char(':'))) d->object.ownerLost(name);
    });
}

NightLightScheduleService::~NightLightScheduleService() { stop(); }

ScheduleServiceStart NightLightScheduleService::start(QString *error)
{
    if (d->started) return ScheduleServiceStart::Started;
    if (!d->connection.isConnected()) {
        if (error) *error = QStringLiteral("constructing session bus is disconnected");
        return ScheduleServiceStart::Failed;
    }
    if (!d->connection.registerService(kService))
        return d->connection.lastError().name() == QLatin1String("org.freedesktop.DBus.Error.NameHasOwner")
            ? ScheduleServiceStart::NameAlreadyOwned : ScheduleServiceStart::Failed;
    if (!d->connection.registerObject(kPath, &d->object, QDBusConnection::ExportAllSlots)) {
        d->connection.unregisterService(kService);
        if (error) *error = d->connection.lastError().message();
        return ScheduleServiceStart::Failed;
    }
    d->started = true;
    d->refreshTimer.start();
    refreshSettings();
    return ScheduleServiceStart::Started;
}

void NightLightScheduleService::stop()
{
    if (!d->started) return;
    d->started = false;
    d->refreshTimer.stop();
    d->connection.unregisterObject(kPath);
    d->connection.unregisterService(kService);
}

void NightLightScheduleService::refreshSettings()
{
    const auto &snapshot = d->settings.snapshot();
    if (d->settings.state() != QindaQt::Services::SettingsClient::ClientState::Ready
        || !snapshot) {
        return;
    }
    const QVariantMap &map = snapshot->values;
    NightLightSettings values;
    values.output.active = valueBool(map, kKeys.at(0), false);
    const auto mode = modeFromConfigToken(valueString(map, kKeys.at(1), QStringLiteral("DarkLight")));
    const auto source = sourceFromConfigToken(valueString(map, kKeys.at(4), QStringLiteral("Location")));
    if (!mode || !source) {
        d->ready = false;
        d->schedule = {};
        ++d->revision;
        publish();
        return;
    }
    values.output.mode = *mode;
    values.output.dayTemperatureKelvin = valueInt(map, kKeys.at(2), kDefaultDayTemperatureKelvin);
    values.output.nightTemperatureKelvin = valueInt(map, kKeys.at(3), kDefaultNightTemperatureKelvin);
    values.output.disabledOutputs = map.value(kKeys.at(11)).toStringList();
    values.schedule.source = *source;
    values.schedule.automaticLocation = valueBool(map, kKeys.at(5), true);
    values.schedule.latitudeDegrees = valueDouble(map, kKeys.at(6), 0.0);
    values.schedule.longitudeDegrees = valueDouble(map, kKeys.at(7), 0.0);
    values.schedule.sunriseStart = QTime::fromString(valueString(map, kKeys.at(8), QStringLiteral("06:00:00")), QStringLiteral("HH:mm:ss"));
    values.schedule.sunsetStart = QTime::fromString(valueString(map, kKeys.at(9), QStringLiteral("18:00:00")), QStringLiteral("HH:mm:ss"));
    values.schedule.transitionSeconds = valueInt(map, kKeys.at(10), kDefaultTransitionSeconds);
    if (!isValidOutput(values.output) || !isValidSchedule(values.schedule)) {
        d->ready = false;
        d->schedule = {};
        ++d->revision;
        publish();
        return;
    }
    d->values = values;
    d->ready = true;
    d->schedule = calculateSchedule(d->values, QDateTime::currentDateTime());
    ++d->revision;
    publish();
}

void NightLightScheduleService::publish()
{
    if (!d->started) return;
    if (d->ready) d->schedule = calculateSchedule(d->values, QDateTime::currentDateTime());
    d->identityReady = true;
    d->identityDiagnostic.clear();
    d->runtimeOutputUuids.clear();
    if (!d->values.output.disabledOutputs.isEmpty()) {
        const auto snapshot = d->display.snapshot();
        if (d->display.state() != QindaQt::DisplayClient::ClientState::Ready
            || !snapshot || !snapshot->wireValid) {
            d->identityReady = false;
            d->identityDiagnostic = QStringLiteral("Display1 output identities are unavailable");
        } else {
            QSet<QString> runtimeUuids;
            for (const QString &stableId : d->values.output.disabledOutputs) {
                const QindaQt::Display::Output *match = nullptr;
                for (const QindaQt::Display::Output &output : snapshot->outputs) {
                    if (output.stableId == stableId) {
                        if (match != nullptr) {
                            match = nullptr;
                            break;
                        }
                        match = &output;
                    }
                }
                if (match == nullptr || !match->wireValid
                    || match->runtimeCompositorUuid.isEmpty()
                    || runtimeUuids.contains(match->runtimeCompositorUuid)) {
                    d->identityReady = false;
                    d->identityDiagnostic = QStringLiteral("A disabled output no longer has one unambiguous compositor identity");
                    d->runtimeOutputUuids.clear();
                    break;
                }
                runtimeUuids.insert(match->runtimeCompositorUuid);
                d->runtimeOutputUuids.append(match->runtimeCompositorUuid);
            }
        }
    }
    d->object.setFrame(d->values, d->schedule, d->ready, d->identityReady,
                       d->identityDiagnostic, d->runtimeOutputUuids, d->revision);
    d->object.publish();
}

} // namespace QindaQt::Services::NightLight

#include "night_light_schedule_service.moc"
