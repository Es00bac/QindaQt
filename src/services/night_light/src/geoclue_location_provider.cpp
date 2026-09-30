// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/night_light/automatic_location_provider.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QTimeZone>

#include <cmath>
#include <functional>

namespace QindaQt::Services::NightLight {
namespace {
const QString kService = QStringLiteral("org.freedesktop.GeoClue2");
const QString kManagerPath = QStringLiteral("/org/freedesktop/GeoClue2/Manager");
const QString kManagerInterface = QStringLiteral("org.freedesktop.GeoClue2.Manager");
const QString kClientInterface = QStringLiteral("org.freedesktop.GeoClue2.Client");
const QString kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kLocationInterface = QStringLiteral("org.freedesktop.GeoClue2.Location");
constexpr quint32 kCityAccuracy = 4;
constexpr qint64 kMaxFixAgeSeconds = 15 * 60;
constexpr qint64 kMaxFutureSkewSeconds = 60;

bool permissionError(const QString &name)
{
    return name.contains(QStringLiteral("AccessDenied"))
        || name.contains(QStringLiteral("NotAuthorized"));
}
}

GeoClueLocationProvider::GeoClueLocationProvider(
    const QDBusConnection &systemBus, QObject *parent)
    : AutomaticLocationProvider(parent), m_bus(systemBus)
{
    if (!m_bus.isConnected() || !m_bus.interface()) return;
    connect(m_bus.interface(), &QDBusConnectionInterface::serviceOwnerChanged,
            this, [this](const QString &name, const QString &, const QString &newOwner) {
        if (name != kService || !m_requested) return;
        if (m_owner.isEmpty()) {
            if (!newOwner.isEmpty()) request();
            return;
        }
        if (newOwner == m_owner) return;
        if (m_signalConnected) {
            m_bus.disconnect(m_owner, m_clientPath, kClientInterface,
                             QStringLiteral("LocationUpdated"), this,
                             SLOT(locationUpdated(QDBusObjectPath,QDBusObjectPath)));
        }
        ++m_generation;
        m_owner.clear();
        m_clientPath.clear();
        m_signalConnected = false;
        m_fix.reset();
        m_state = AutomaticLocationState::Unavailable;
        m_diagnostic = QStringLiteral("GeoClue service owner changed; waiting for authorization");
        Q_EMIT changed();
        if (!newOwner.isEmpty()) request();
    });
}

GeoClueLocationProvider::~GeoClueLocationProvider() { stop(); }

void GeoClueLocationProvider::request()
{
    if (m_state == AutomaticLocationState::Pending) return;
    if (!m_bus.isConnected()) {
        m_state = AutomaticLocationState::Unavailable;
        m_diagnostic = QStringLiteral("GeoClue system bus is unavailable");
        Q_EMIT changed();
        return;
    }
    if (m_state == AutomaticLocationState::Available && m_fix
        && m_fix->observedAt.secsTo(QDateTime::currentDateTimeUtc()) < kMaxFixAgeSeconds)
        return;
    stopRemoteClient();
    ++m_generation;
    m_requested = true;
    if (m_signalConnected && !m_owner.isEmpty()) {
        m_bus.disconnect(m_owner, m_clientPath, kClientInterface,
                         QStringLiteral("LocationUpdated"), this,
                         SLOT(locationUpdated(QDBusObjectPath,QDBusObjectPath)));
    }
    m_signalConnected = false;
    m_fix.reset();
    m_owner.clear();
    m_clientPath.clear();
    m_state = AutomaticLocationState::Pending;
    m_diagnostic = QStringLiteral("Waiting for GeoClue authorization");
    Q_EMIT changed();
    acquireOwner(m_generation);
}

void GeoClueLocationProvider::stop()
{
    stopRemoteClient();
    ++m_generation;
    m_requested = false;
    if (m_signalConnected && !m_owner.isEmpty()) {
        m_bus.disconnect(m_owner, m_clientPath, kClientInterface,
                         QStringLiteral("LocationUpdated"), this,
                         SLOT(locationUpdated(QDBusObjectPath,QDBusObjectPath)));
    }
    m_signalConnected = false;
    m_owner.clear();
    m_clientPath.clear();
    m_fix.reset();
    m_state = AutomaticLocationState::Stopped;
    m_diagnostic.clear();
    Q_EMIT changed();
}

AutomaticLocationState GeoClueLocationProvider::state() const { return m_state; }
std::optional<AutomaticLocationFix> GeoClueLocationProvider::fix() const
{
    if (!m_fix || m_fix->observedAt.secsTo(QDateTime::currentDateTimeUtc()) > kMaxFixAgeSeconds)
        return std::nullopt;
    return m_fix;
}
QString GeoClueLocationProvider::diagnostic() const { return m_diagnostic; }

void GeoClueLocationProvider::acquireOwner(quint64 generation)
{
    if (generation != m_generation) return;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    message << kService;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        if (reply.isError() || !reply.value().startsWith(QLatin1Char(':'))) {
            fail(generation, false, QStringLiteral("GeoClue service is unavailable"));
            return;
        }
        m_owner = reply.value();
        createClient(generation);
    });
}

void GeoClueLocationProvider::createClient(quint64 generation)
{
    const auto message = QDBusMessage::createMethodCall(
        m_owner, kManagerPath, kManagerInterface, QStringLiteral("CreateClient"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        if (reply.isError() || reply.value().path().isEmpty()) {
            const QString error = reply.isError() ? reply.error().name() : QString{};
            fail(generation, permissionError(error),
                 permissionError(error) ? QStringLiteral("GeoClue permission was denied")
                                        : QStringLiteral("GeoClue refused location access"));
            return;
        }
        m_clientPath = reply.value().path();
        setProperty(generation, QStringLiteral("DesktopId"),
                    QStringLiteral("org.qindaqt.NightLight"), [this, generation] {
            setProperty(generation, QStringLiteral("RequestedAccuracyLevel"),
                        QVariant::fromValue(kCityAccuracy),
                        [this, generation] { startClient(generation); });
        });
    });
}

void GeoClueLocationProvider::setProperty(
    quint64 generation, const QString &property, const QVariant &value,
    std::function<void()> next)
{
    if (generation != m_generation || m_owner.isEmpty()) return;
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_owner, m_clientPath, kPropertiesInterface, QStringLiteral("Set"));
    message << kClientInterface << property << QVariant::fromValue(QDBusVariant(value));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, next = std::move(next)]() mutable {
        QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        if (reply.isError()) {
            const bool denied = permissionError(reply.error().name());
            fail(generation, denied, denied
                ? QStringLiteral("GeoClue permission was denied")
                : QStringLiteral("GeoClue client configuration failed"));
            return;
        }
        next();
    });
}

void GeoClueLocationProvider::startClient(quint64 generation)
{
    if (generation != m_generation || m_owner.isEmpty()) return;
    m_signalConnected = m_bus.connect(
        m_owner, m_clientPath, kClientInterface, QStringLiteral("LocationUpdated"),
        this, SLOT(locationUpdated(QDBusObjectPath,QDBusObjectPath)));
    if (!m_signalConnected) {
        fail(generation, false, QStringLiteral("GeoClue location updates are unavailable"));
        return;
    }
    const auto message = QDBusMessage::createMethodCall(
        m_owner, m_clientPath, kClientInterface, QStringLiteral("Start"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        if (reply.isError()) {
            const bool denied = permissionError(reply.error().name());
            fail(generation, denied, denied
                ? QStringLiteral("GeoClue permission was denied")
                : QStringLiteral("GeoClue could not start location updates"));
        }
    });
}

void GeoClueLocationProvider::locationUpdated(
    const QDBusObjectPath &, const QDBusObjectPath &newPath)
{
    if ((m_state != AutomaticLocationState::Pending
         && m_state != AutomaticLocationState::Available)
        || newPath.path().isEmpty() || !newPath.path().startsWith(QLatin1Char('/'))) return;
    readLocation(m_generation, newPath.path());
}

void GeoClueLocationProvider::readLocation(quint64 generation, const QString &path)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_owner, path, kPropertiesInterface, QStringLiteral("GetAll"));
    message << kLocationInterface;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation
            || (m_state != AutomaticLocationState::Pending
                && m_state != AutomaticLocationState::Available)) return;
        if (reply.isError()) {
            fail(generation, false, QStringLiteral("GeoClue location data is unavailable"));
            return;
        }
        const QVariantMap values = reply.value();
        const auto coordinate = [&values](const QString &name) -> std::optional<double> {
            if (!values.contains(name)) return std::nullopt;
            QVariant value = values.value(name);
            if (value.metaType() == QMetaType::fromType<QDBusVariant>())
                value = value.value<QDBusVariant>().variant();
            // GeoClue defines both coordinate properties as D-Bus doubles. Reject
            // missing or mistyped data instead of letting QVariant default it to 0.
            if (value.metaType() != QMetaType::fromType<double>()) return std::nullopt;
            bool ok = false;
            const double result = value.toDouble(&ok);
            return ok ? std::optional<double>(result) : std::nullopt;
        };
        const auto latitudeValue = coordinate(QStringLiteral("Latitude"));
        const auto longitudeValue = coordinate(QStringLiteral("Longitude"));
        if (!latitudeValue || !longitudeValue) {
            fail(generation, false,
                 QStringLiteral("GeoClue returned incomplete or invalid coordinates"));
            return;
        }
        const double latitude = *latitudeValue;
        const double longitude = *longitudeValue;
        QVariant timestamp = values.value(QStringLiteral("Timestamp"));
        if (timestamp.metaType() == QMetaType::fromType<QDBusVariant>())
            timestamp = timestamp.value<QDBusVariant>().variant();
        qulonglong seconds = 0;
        qulonglong microseconds = 0;
        if (timestamp.metaType() == QMetaType::fromType<QDBusArgument>()) {
            const QDBusArgument argument = timestamp.value<QDBusArgument>();
            argument.beginStructure();
            argument >> seconds >> microseconds;
            argument.endStructure();
        } else if (timestamp.canConvert<qulonglong>()) {
            seconds = timestamp.toULongLong();
        }
        Q_UNUSED(microseconds);
        const QDateTime observed = QDateTime::fromSecsSinceEpoch(
            qint64(seconds), QTimeZone::UTC);
        const qint64 age = observed.secsTo(QDateTime::currentDateTimeUtc());
        if (!std::isfinite(latitude) || !std::isfinite(longitude)
            || latitude < -90.0 || latitude > 90.0
            || longitude < -180.0 || longitude > 180.0
            || !observed.isValid() || age < -kMaxFutureSkewSeconds
            || age > kMaxFixAgeSeconds) {
            fail(generation, false, QStringLiteral("GeoClue returned a stale or invalid location"));
            return;
        }
        m_fix = AutomaticLocationFix{latitude, longitude, observed};
        m_state = AutomaticLocationState::Available;
        m_diagnostic.clear();
        Q_EMIT changed();
    });
}

void GeoClueLocationProvider::stopRemoteClient()
{
    if (m_owner.isEmpty() || m_clientPath.isEmpty()) return;
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_owner, m_clientPath, kClientInterface, QStringLiteral("Stop"));
    m_bus.asyncCall(message, 2'000);
}

void GeoClueLocationProvider::fail(
    quint64 generation, bool denied, const QString &reason)
{
    if (generation != m_generation) return;
    stopRemoteClient();
    if (m_signalConnected) {
        m_bus.disconnect(m_owner, m_clientPath, kClientInterface,
                         QStringLiteral("LocationUpdated"), this,
                         SLOT(locationUpdated(QDBusObjectPath,QDBusObjectPath)));
    }
    m_signalConnected = false;
    m_fix.reset();
    m_state = denied ? AutomaticLocationState::Denied : AutomaticLocationState::Unavailable;
    m_diagnostic = reason;
    Q_EMIT changed();
}

} // namespace QindaQt::Services::NightLight
