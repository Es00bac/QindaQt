// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_session_apply_status_client.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QVariant>

#include <utility>

namespace QindaQt::Apps::SettingsWindows {

namespace {
constexpr auto ServiceName = "org.qindaqt.WindowManagement1";
constexpr auto ObjectPath = "/org/qindaqt/WindowManagement1";
constexpr auto InterfaceName = "org.qindaqt.WindowManagement1";
constexpr auto MaximumMessageLength = 512;

bool decodePreferenceMap(const QVariant &value, QVariantMap *preferences)
{
    if (preferences == nullptr) {
        return false;
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        *preferences = value.toMap();
        return true;
    }
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::MapType) {
        return false;
    }
    argument.beginMap();
    while (!argument.atEnd()) {
        argument.beginMapEntry();
        QString key;
        QVariant entry;
        argument >> key >> entry;
        argument.endMapEntry();
        preferences->insert(std::move(key), std::move(entry));
    }
    argument.endMap();
    return true;
}

} // namespace

WindowsSessionApplyStatusClient::WindowsSessionApplyStatusClient(QDBusConnection bus,
                                                                 QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
}

void WindowsSessionApplyStatusClient::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(ServiceName), m_bus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                handleOwnerChanged(newOwner);
            });
    if (!m_bus.isConnected() || m_bus.interface() == nullptr) {
        publishUnavailable(QStringLiteral("The session bus is unavailable."));
        return;
    }
    const QDBusReply<QString> owner = m_bus.interface()->serviceOwner(
        QString::fromLatin1(ServiceName));
    if (!owner.isValid()) {
        publishUnavailable(QStringLiteral("Could not inspect the session apply-state owner."));
        return;
    }
    handleOwnerChanged(owner.value());
}

void WindowsSessionApplyStatusClient::handleOwnerChanged(const QString &newOwner)
{
    if (newOwner == m_owner) {
        if (newOwner.isEmpty()) {
            publishUnavailable(QStringLiteral("The QindaQt session apply-state service is not "
                                              "running. Start or restart the session to apply "
                                              "saved window settings."));
        }
        return;
    }
    if (!m_owner.isEmpty()) {
        m_bus.disconnect(m_owner, QString::fromLatin1(ObjectPath),
                         QString::fromLatin1(InterfaceName), QStringLiteral("StateChanged"),
                         this, SLOT(handleStateChanged(QVariantMap)));
    }
    m_owner = newOwner;
    ++m_requestGeneration;
    if (m_owner.isEmpty()) {
        publishUnavailable(QStringLiteral("The QindaQt session apply-state service is not "
                                          "running. Start or restart the session to apply "
                                          "saved window settings."));
        return;
    }
    if (!m_bus.connect(m_owner, QString::fromLatin1(ObjectPath),
                       QString::fromLatin1(InterfaceName), QStringLiteral("StateChanged"),
                       this, SLOT(handleStateChanged(QVariantMap)))) {
        publishUnavailable(QStringLiteral("Could not watch the session apply-state service."));
        return;
    }
    requestState();
}

void WindowsSessionApplyStatusClient::requestState()
{
    if (m_owner.isEmpty()) {
        return;
    }
    const QString owner = m_owner;
    const quint64 generation = ++m_requestGeneration;
    const QDBusMessage request = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(ObjectPath), QString::fromLatin1(InterfaceName),
        QStringLiteral("GetState"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 3'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, generation](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (owner != m_owner || generation != m_requestGeneration) {
                    return;
                }
                if (reply.isError()) {
                    publishUnavailable(QStringLiteral("Could not read the session apply state: %1")
                                           .arg(reply.error().message().left(
                                               MaximumMessageLength)));
                    return;
                }
                SessionApplyStatus status;
                QString decodeError;
                if (!decodeState(reply.value(), &status, &decodeError)) {
                    publishUnavailable(QStringLiteral(
                        "The session returned an invalid window apply state: %1")
                                           .arg(decodeError));
                    return;
                }
                publishStatus(std::move(status));
            });
}

void WindowsSessionApplyStatusClient::retryApply()
{
    if (m_owner.isEmpty()) {
        publishUnavailable(QStringLiteral("The QindaQt session apply-state service is not "
                                          "running. Start or restart the session to apply "
                                          "saved window settings."));
        return;
    }
    const QString owner = m_owner;
    const quint64 generation = ++m_requestGeneration;
    const QDBusMessage request = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(ObjectPath), QString::fromLatin1(InterfaceName),
        QStringLiteral("RetryApply"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 3'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, generation](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();
                if (owner != m_owner || generation != m_requestGeneration) {
                    return;
                }
                if (reply.isError()) {
                    publishUnavailable(QStringLiteral("The session could not retry the saved "
                                                      "window settings: %1")
                                           .arg(reply.error().message().left(
                                               MaximumMessageLength)));
                    return;
                }
                requestState();
            });
}

void WindowsSessionApplyStatusClient::handleStateChanged(const QVariantMap &wire)
{
    Q_UNUSED(wire);
    // Treat the signal as invalidation and fetch from the current unique owner;
    // a queued signal from the former owner must never restore stale state.
    requestState();
}

void WindowsSessionApplyStatusClient::publishStatus(SessionApplyStatus status)
{
    m_status = std::move(status);
    Q_EMIT statusChanged(m_status);
}

void WindowsSessionApplyStatusClient::publishUnavailable(const QString &message)
{
    SessionApplyStatus status;
    status.message = message.left(MaximumMessageLength);
    publishStatus(std::move(status));
}

bool WindowsSessionApplyStatusClient::decodeState(const QVariantMap &wire,
                                                  SessionApplyStatus *status,
                                                  QString *error) const
{
    const auto requireType = [&wire, error](const QString &key, QMetaType::Type expected) {
        const QMetaType actual = wire.value(key).metaType();
        if (actual.id() == expected) {
            return true;
        }
        if (error != nullptr) {
            *error = QStringLiteral("%1 has type %2; expected %3")
                         .arg(key, QString::fromLatin1(actual.name()),
                              QString::fromLatin1(QMetaType(expected).name()));
        }
        return false;
    };
    if (status == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("no destination status was provided");
        }
        return false;
    }
    if (!requireType(QStringLiteral("wireVersion"), QMetaType::UInt)
        || !requireType(QStringLiteral("phase"), QMetaType::QString)
        || !requireType(QStringLiteral("settingsOwner"), QMetaType::QString)
        || !requireType(QStringLiteral("settingsEpoch"), QMetaType::QString)
        || !requireType(QStringLiteral("settingsRevision"), QMetaType::ULongLong)
        || !requireType(QStringLiteral("kwinOwner"), QMetaType::QString)
        || !requireType(QStringLiteral("message"), QMetaType::QString)) {
        return false;
    }
    QVariantMap preferences;
    if (!decodePreferenceMap(wire.value(QStringLiteral("preferences")), &preferences)) {
        if (error != nullptr) {
            *error = QStringLiteral("preferences is not a D-Bus string-to-variant map");
        }
        return false;
    }
    if (wire.value(QStringLiteral("wireVersion")).toUInt() != 1) {
        if (error != nullptr) {
            *error = QStringLiteral("unsupported wireVersion %1")
                         .arg(wire.value(QStringLiteral("wireVersion")).toUInt());
        }
        return false;
    }
    const QString phase = wire.value(QStringLiteral("phase")).toString();
    if (phase == QLatin1String("unavailable")) {
        status->phase = SessionApplyPhase::Unavailable;
    } else if (phase == QLatin1String("applying")) {
        status->phase = SessionApplyPhase::Applying;
    } else if (phase == QLatin1String("applied")) {
        status->phase = SessionApplyPhase::Applied;
    } else if (phase == QLatin1String("failed")) {
        status->phase = SessionApplyPhase::Failed;
    } else {
        if (error != nullptr) {
            *error = QStringLiteral("unknown phase '%1'").arg(phase);
        }
        return false;
    }
    status->serviceAvailable = true;
    if (!preferences.isEmpty()) {
        QString preferencesError;
        status->preferences = WindowsValues::fromVariantMap(preferences, &preferencesError);
        if (!status->preferences.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("invalid preferences map: %1").arg(preferencesError);
            }
            return false;
        }
    } else if (status->phase == SessionApplyPhase::Applied) {
        if (error != nullptr) {
            *error = QStringLiteral("an applied state has no preferences");
        }
        return false;
    }
    if (status->phase == SessionApplyPhase::Applied
        && (wire.value(QStringLiteral("settingsOwner")).toString().isEmpty()
            || wire.value(QStringLiteral("settingsEpoch")).toString().isEmpty()
            || wire.value(QStringLiteral("kwinOwner")).toString().isEmpty())) {
        if (error != nullptr) {
            *error = QStringLiteral("an applied state has no Settings1 or KWin owner");
        }
        return false;
    }
    status->message = wire.value(QStringLiteral("message")).toString().left(
        MaximumMessageLength);
    return true;
}

} // namespace QindaQt::Apps::SettingsWindows
