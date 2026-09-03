// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"

#include "qindaqt/services/font_discovery/font_discovery.h"
#include "qindaqt/services/font_preferences/font_preferences_codec.h"
#include "qindaqt/services/font_preferences/font_settings_bootstrap.h"
#include "qindaqt/services/font_preferences/font_settings_bridge.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QUuid>
#include <QVariantMap>

namespace QindaQt::Services::FontDiscovery {

namespace {

using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;
using QindaQt::Services::FontPreferences::FontPreferencesCodec;
using QindaQt::Services::FontPreferences::FontSettingsBootstrap;
using QindaQt::Services::FontPreferences::FontSettingsBridge;

constexpr auto BusService = "org.freedesktop.DBus";
constexpr auto BusPath = "/org/freedesktop/DBus";
constexpr auto BusInterface = "org.freedesktop.DBus";
// Share of the total bootstrap budget reserved for service activation and
// owner resolution; the snapshot read keeps the remainder.
constexpr int ActivationBudgetMilliseconds = 300;
constexpr int OwnerBudgetMilliseconds = 150;

// The F1 fonts.* keys are defined by schema-v2 (data/settings/schema-v2.json);
// a snapshot from any other settings schema generation is not authoritative
// for this composition.
constexpr quint32 ExpectedSettingsSchemaVersion = 2;

void setDiagnostic(QString *output, const QString &message)
{
    if (output != nullptr) {
        *output = message;
    }
}

std::optional<quint32> exactUInt(const QVariantMap &map, const char *field)
{
    const QVariant value = map.value(QLatin1String(field));
    if (value.metaType().id() != QMetaType::UInt) {
        return std::nullopt;
    }
    return value.toUInt();
}

std::optional<quint64> exactULongLong(const QVariantMap &map, const char *field)
{
    const QVariant value = map.value(QLatin1String(field));
    if (value.metaType().id() != QMetaType::ULongLong) {
        return std::nullopt;
    }
    return value.toULongLong();
}

std::optional<QString> exactString(const QVariantMap &map, const char *field)
{
    const QVariant value = map.value(QLatin1String(field));
    if (value.metaType().id() != QMetaType::QString) {
        return std::nullopt;
    }
    return value.toString();
}

bool blockingCall(const QDBusConnection &connection, const QDBusMessage &message,
                  int timeoutMilliseconds, QDBusMessage *reply, QString *diagnostic)
{
    if (timeoutMilliseconds <= 0) {
        setDiagnostic(diagnostic, QStringLiteral("settings bootstrap budget exhausted"));
        return false;
    }
    *reply = connection.call(message, QDBus::Block, timeoutMilliseconds);
    if (reply->type() != QDBusMessage::ReplyMessage) {
        setDiagnostic(diagnostic,
                      QStringLiteral("settings call failed: %1")
                          .arg(reply->errorMessage().left(256)));
        return false;
    }
    return true;
}

// AGENT-CONTRACT: Mirrors the fencing essentials of SettingsClient snapshot
// validation (exact field set, exact-typed envelope fields, exact key scope)
// for the one-shot blocking read; a failure here must leave the composition
// with no snapshot at all.
bool validateSnapshotEnvelope(const QVariantMap &wire, const QStringList &keys,
                              QVariantMap *valuesOut, QString *diagnostic)
{
    const auto fail = [diagnostic](const QString &reason) {
        setDiagnostic(diagnostic, QStringLiteral("settings snapshot rejected: %1").arg(reason));
        return false;
    };
    if (wire.size() != WireContract::SnapshotReplyFieldCount) {
        return fail(QStringLiteral("unexpected field count"));
    }
    const auto status = exactUInt(wire, WireContract::FieldStatus);
    if (!status || *status != quint32(SettingsWireStatus::Applied)) {
        return fail(QStringLiteral("status is not a confirmed Applied"));
    }
    const auto wireSchema = exactUInt(wire, WireContract::FieldWireSchemaVersion);
    if (!wireSchema || *wireSchema != WireContract::WireSchemaVersion) {
        return fail(QStringLiteral("wire schema version mismatch"));
    }
    const auto settingsSchema = exactUInt(wire, WireContract::FieldSettingsSchemaVersion);
    if (!settingsSchema || *settingsSchema != ExpectedSettingsSchemaVersion) {
        return fail(QStringLiteral("settings schema version mismatch"));
    }
    if (!exactULongLong(wire, WireContract::FieldRevision)) {
        return fail(QStringLiteral("revision is not an exact unsigned64"));
    }
    const auto epoch = exactString(wire, WireContract::FieldEpoch);
    if (!epoch || epoch->isEmpty() || epoch->toUtf8().size() > WireContract::MaximumEpochBytes) {
        return fail(QStringLiteral("epoch is missing or over-bound"));
    }
    const auto message = exactString(wire, WireContract::FieldMessage);
    if (!message || message->toUtf8().size() > WireContract::MaximumMessageBytes) {
        return fail(QStringLiteral("message is missing or over-bound"));
    }
    const auto values = SettingsProtocol::decodeBoundedVariantMap(
        wire.value(QLatin1String(WireContract::FieldValues)), WireContract::MaximumMapEntries);
    const auto sources = SettingsProtocol::decodeBoundedVariantMap(
        wire.value(QLatin1String(WireContract::FieldSourceLayers)),
        WireContract::MaximumMapEntries);
    if (!values || !sources || values->size() != keys.size() || sources->size() != keys.size()) {
        return fail(QStringLiteral("value/source scope does not match the requested keys"));
    }
    for (const QString &key : keys) {
        if (!values->contains(key) || !sources->contains(key)) {
            return fail(QStringLiteral("value/source scope does not match the requested keys"));
        }
        if (sources->value(key).metaType().id() != QMetaType::QString) {
            return fail(QStringLiteral("source layer is not a string"));
        }
    }
    *valuesOut = *values;
    return true;
}

} // namespace

std::optional<FontPreferences> FontSessionBootstrap::readConfirmedPreferences(
    const QDBusConnection &connection, int timeoutMilliseconds, QString *diagnostic)
{
    if (timeoutMilliseconds <= 0) {
        setDiagnostic(diagnostic, QStringLiteral("bootstrap timeout must be positive"));
        return std::nullopt;
    }
    if (!connection.isConnected()) {
        setDiagnostic(diagnostic, QStringLiteral("settings session bus is not connected"));
        return std::nullopt;
    }

    QElapsedTimer deadline;
    deadline.start();
    const auto remaining = [&]() {
        return timeoutMilliseconds - static_cast<int>(deadline.elapsed());
    };

    // Activation failure is tolerated here: the owner lookup below fails
    // closed when the service never appears.
    QDBusMessage activation = QDBusMessage::createMethodCall(
        QString::fromLatin1(BusService), QString::fromLatin1(BusPath),
        QString::fromLatin1(BusInterface), QStringLiteral("StartServiceByName"));
    activation << QString::fromLatin1(WireContract::ServiceName) << quint32(0);
    connection.call(activation, QDBus::Block,
                    qMin(remaining(), ActivationBudgetMilliseconds));

    QDBusMessage ownerCall = QDBusMessage::createMethodCall(
        QString::fromLatin1(BusService), QString::fromLatin1(BusPath),
        QString::fromLatin1(BusInterface), QStringLiteral("GetNameOwner"));
    ownerCall << QString::fromLatin1(WireContract::ServiceName);
    QDBusMessage ownerReply;
    if (!blockingCall(connection, ownerCall, qMin(remaining(), OwnerBudgetMilliseconds),
                      &ownerReply, diagnostic)) {
        return std::nullopt;
    }
    if (ownerReply.arguments().size() != 1
        || ownerReply.arguments().constFirst().metaType().id() != QMetaType::QString) {
        setDiagnostic(diagnostic, QStringLiteral("settings owner reply is malformed"));
        return std::nullopt;
    }
    const QString owner = ownerReply.arguments().constFirst().toString();
    if (!owner.startsWith(QLatin1Char(':'))) {
        setDiagnostic(diagnostic, QStringLiteral("settings service owner is not a unique name"));
        return std::nullopt;
    }

    const QStringList keys = FontSettingsBridge::scopedKeys();
    QDBusMessage snapshotCall = QDBusMessage::createMethodCall(
        owner, QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::GetSnapshotMethod));
    snapshotCall << keys;
    QDBusMessage snapshotReply;
    if (!blockingCall(connection, snapshotCall, remaining(), &snapshotReply, diagnostic)) {
        return std::nullopt;
    }
    if (snapshotReply.arguments().size() != 1) {
        setDiagnostic(diagnostic, QStringLiteral("settings snapshot reply has wrong arity"));
        return std::nullopt;
    }
    const auto wire = SettingsProtocol::decodeBoundedVariantMap(
        snapshotReply.arguments().constFirst(), WireContract::SnapshotReplyFieldCount);
    if (!wire) {
        setDiagnostic(diagnostic, QStringLiteral("settings snapshot envelope is malformed"));
        return std::nullopt;
    }
    QVariantMap values;
    if (!validateSnapshotEnvelope(*wire, keys, &values, diagnostic)) {
        return std::nullopt;
    }

    QString decodeError;
    auto preferences = FontPreferencesCodec::fromSettingsMap(values, &decodeError);
    if (!preferences) {
        setDiagnostic(diagnostic,
                      decodeError.isEmpty()
                          ? QStringLiteral("confirmed font preferences failed to decode")
                          : decodeError.left(512));
        return std::nullopt;
    }
    return preferences;
}

bool FontSessionBootstrap::applyFromSessionSettings(QString *diagnostic)
{
    // AGENT-GUARD: Never autolaunch a session bus from a pre-application
    // read; an unset address means there is no preference source, not that
    // one should be spawned.
    if (qgetenv("DBUS_SESSION_BUS_ADDRESS").isEmpty()) {
        setDiagnostic(diagnostic,
                      QStringLiteral("no session bus address; font preference source absent"));
        return false;
    }
    const QString connectionName = QStringLiteral("qindaqt-font-session-bootstrap-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::Id128));
    const QDBusConnection connection =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    if (!connection.isConnected()) {
        setDiagnostic(diagnostic,
                      QStringLiteral("settings session bus unavailable: %1")
                          .arg(connection.lastError().message().left(256)));
        QDBusConnection::disconnectFromBus(connectionName);
        return false;
    }
    const auto preferences =
        readConfirmedPreferences(connection, DefaultBootstrapTimeoutMilliseconds, diagnostic);
    QDBusConnection::disconnectFromBus(connectionName);
    if (!preferences) {
        return false;
    }

    // AGENT-CONTRACT (review finding P1-1): this is the production discovery
    // composition -- the provider is invoked with productionDefault(), the
    // only request shape allowed to resolve the default fontconfig
    // configuration.
    const FontDiscoveryProvider provider(FontDiscoveryRequest::productionDefault());
    const FontDiscoveryResult discovery = provider.discover();
    if (!discovery.available) {
        setDiagnostic(diagnostic,
                      QStringLiteral("live font discovery unavailable: %1")
                          .arg(discovery.diagnostic.left(256)));
        return false;
    }
    if (!FontSettingsBootstrap::confirmedFamilyResolves(*preferences, discovery.facts)) {
        setDiagnostic(diagnostic,
                      QStringLiteral("confirmed family does not resolve in the live font catalog"));
        return false;
    }
    return FontSettingsBootstrap::applyPreferences(*preferences, diagnostic);
}

} // namespace QindaQt::Services::FontDiscovery
