// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QList>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace QindaQt::Tests {

// A Settings1 transport that records requests and lets a test deliver the
// wire replies itself, so a route model is exercised without a bus.
class FakeSettingsTransport final : public QindaQt::Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        ++starts;
        if (!startSucceeds) {
            if (error != nullptr) *error = QStringLiteral("transport unavailable");
            return false;
        }
        if (error != nullptr) error->clear();
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &keys) override
    {
        snapshots.append({token, owner, keys});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch, quint64 revision,
                const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override { ++activations; }

    struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest { quint64 token; QString owner; QString epoch; quint64 revision; QVariantList operations; };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
    int activations = 0;
    int starts = 0;
    bool startSucceeds = true;
};

// The touch scope's defaults: a scoped snapshot answers every requested key,
// so a test names only the values it changes.
inline QVariantMap touchScopeDefaults()
{
    return {{QStringLiteral("input.touch.enabled"), true},
            {QStringLiteral("input.touch.longPressMs"), 500},
            {QStringLiteral("input.touch.onScreenKeyboard"), QStringLiteral("auto")},
            {QStringLiteral("input.touch.edgeLeft"), QStringLiteral("overview")},
            {QStringLiteral("input.touch.edgeTop"), QStringLiteral("notifications")},
            {QStringLiteral("input.touch.edgeRight"), QStringLiteral("none")},
            {QStringLiteral("input.touch.edgeBottom"), QStringLiteral("task-switcher")}};
}

inline QVariantMap withTouchDefaults(const QVariantMap &overrides)
{
    QVariantMap values = touchScopeDefaults();
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        values.insert(it.key(), it.value());
    }
    return values;
}

inline QVariantMap fakeSnapshotWire(quint64 revision, const QVariantMap &values)
{
    using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
    using QindaQt::Services::SettingsProtocol::WireContract;
    QVariantMap layers;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        layers.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

inline QVariantMap fakeCommitWire(QindaQt::Services::SettingsProtocol::SettingsWireStatus status, quint64 before,
                                  quint64 after, const QVariantMap &values)
{
    using QindaQt::Services::SettingsProtocol::WireContract;
    QVariantMap layers;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        layers.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList(values.keys())},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

} // namespace QindaQt::Tests
