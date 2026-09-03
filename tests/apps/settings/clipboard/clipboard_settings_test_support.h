// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_clipboard/clipboard_settings_model.h>
#include <qindaqt/services/clipboard_client/clipboard_transport.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <utility>

namespace QindaQt::Apps::SettingsClipboard::TestSupport {
using Services::SettingsProtocol::SettingsWireStatus;
using Services::SettingsProtocol::WireContract;

class FakeSettingsTransport final
    : public Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    struct SnapshotRequest { quint64 token; QString owner; };
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
                         const QStringList &) override
    {
        snapshots.append({token, owner});
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

class FakeClipboardTransport final
    : public Services::Clipboard::ClipboardTransport {
    Q_OBJECT
public:
    struct Submitted {
        QString owner;
        quint64 token;
        Services::Clipboard::OperationRequest request;
    };

    void start() override { started = true; }
    void stop() override { started = false; }
    void fetchSnapshot(const QString &owner, quint64 token) override
    {
        fetchOwner = owner;
        fetchToken = token;
    }
    void submitOperation(const QString &owner, quint64 token,
                         const Services::Clipboard::OperationRequest &request) override
    {
        operations.append({owner, token, request});
    }

    QString fetchOwner;
    quint64 fetchToken = 0;
    QList<Submitted> operations;
    bool started = false;
};

inline QVariantMap settingsSnapshotWire(QString epoch, quint64 revision,
                                        bool enabled,
                                        QString source = QStringLiteral("user-overrides"))
{
    const QString key = QString::fromLatin1(ClipboardHistorySettingsKey);
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, std::move(source)}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

inline QVariantMap settingsSnapshotWireValue(QString epoch, quint64 revision,
                                             QVariant value, QString source)
{
    const QString key = QString::fromLatin1(ClipboardHistorySettingsKey);
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{key, std::move(value)}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, std::move(source)}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

inline QVariantMap settingsCommitWire(SettingsWireStatus status,
                                      quint64 before, quint64 after,
                                      bool enabled, QString epoch,
                                      QString message = {})
{
    const QString key = QString::fromLatin1(ClipboardHistorySettingsKey);
    const QStringList changed = status == SettingsWireStatus::Applied
        ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), std::move(message)}};
}

inline Services::Clipboard::Snapshot clipboardSnapshot(
    quint64 epoch, quint32 generation, quint64 revision, int count,
    bool historyEnabled = true, bool privacyAllowed = true)
{
    QList<Services::ClipboardModel::ClipboardEntryDescriptor> descriptors;
    for (int index = 0; index < count; ++index) {
        Services::ClipboardModel::ClipboardEntryDescriptor descriptor;
        descriptor.id = {.generation = generation,
                         .serial = quint32(index + 1)};
        descriptor.admittedTick = quint64(index + 1);
        descriptor.lastUsedTick = quint64(index + 1);
        descriptor.fingerprint = QByteArray(32, char(index + 1));
        descriptor.formats = {{QStringLiteral("text/plain"), 4}};
        descriptors.append(descriptor);
    }
    const auto encoded = Services::ClipboardModel::encodeDescriptorList(descriptors);
    Q_ASSERT(encoded.accepted());
    return {.epoch = epoch,
            .generation = generation,
            .revision = revision,
            .historyEnabled = historyEnabled,
            .privacyAllowed = privacyAllowed,
            .descriptorList = encoded.bytes};
}

} // namespace QindaQt::Apps::SettingsClipboard::TestSupport
