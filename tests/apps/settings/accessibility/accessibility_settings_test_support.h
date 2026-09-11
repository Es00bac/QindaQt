// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_accessibility/accessibility_values.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QtTest/QTest>

#include <utility>

namespace QindaQt::Apps::SettingsAccessibility::TestSupport {
using Services::SettingsProtocol::SettingsWireStatus;
using Services::SettingsProtocol::WireContract;

class FakeSettingsTransport final
    : public Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest {
        quint64 token;
        QString owner;
        QString epoch;
        quint64 revision;
        QVariantList operations;
    };

    bool start(QString *) override { started = true; return true; }
    void stop() override { started = false; }
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        snapshots.append({token, owner, keys});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
    bool started = false;
};

inline QVariantMap defaultValues()
{
    return AccessibilityValues{}.toVariantMap();
}

inline QVariantMap snapshotWire(QString epoch, quint64 revision,
                                const QVariantMap &values)
{
    QVariantMap sources;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

inline QVariantMap commitWire(SettingsWireStatus status, quint64 before,
                              quint64 after, const QVariantMap &currentValues,
                              QString epoch, QString message = {})
{
    QStringList changed;
    QVariantMap sources;
    for (auto it = currentValues.constBegin(); it != currentValues.constEnd(); ++it) {
        sources.insert(it.key(), QStringLiteral("user-overrides"));
        if (status == SettingsWireStatus::Applied) {
            changed.append(it.key());
        }
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), currentValues},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), std::move(message)}};
}

// AGENT-GUARD: QTest macros expand `return;`, so these helpers report their
// outcome as a bool and every caller asserts it before touching the model.
[[nodiscard]] inline bool answerSnapshot(FakeSettingsTransport &transport,
                                         const QString &epoch, quint64 revision,
                                         const QVariantMap &values)
{
    if (!QTest::qWaitFor([&transport] { return !transport.snapshots.isEmpty(); },
                         5'000)) {
        return false;
    }
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(epoch, revision, values));
    return true;
}

[[nodiscard]] inline bool establishBaseline(FakeSettingsTransport &transport,
                                            const QString &owner,
                                            const QString &epoch, quint64 revision,
                                            const QVariantMap &values)
{
    Q_EMIT transport.ownerChanged(owner);
    return answerSnapshot(transport, epoch, revision, values);
}

} // namespace QindaQt::Apps::SettingsAccessibility::TestSupport
