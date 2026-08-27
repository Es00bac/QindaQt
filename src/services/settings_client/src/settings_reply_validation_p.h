// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::SettingsClient::Private {

struct CommitReplyContext final {
    QString epoch;
    quint32 settingsSchemaVersion = 0;
    quint64 baseRevision = 0;
    QString key;
};

[[nodiscard]] std::optional<quint64> exactUnsigned64(const QVariant &value);
[[nodiscard]] std::optional<SettingsProtocol::SettingsWireStatus> wireStatus(
    const QVariantMap &wire);
[[nodiscard]] std::optional<QVariantMap> boundedValueMap(const QVariant &wireValue);
[[nodiscard]] std::optional<QVariantMap> boundedSourceMap(const QVariant &wireValue);
[[nodiscard]] std::optional<QStringList> boundedChangedKeys(const QVariant &wireValue);
[[nodiscard]] std::optional<QString> boundedWireMessage(const QVariant &wireValue);
[[nodiscard]] std::optional<CommitOutcome> validatedCommitReply(
    const QVariantMap &wire, const CommitReplyContext &context);
[[nodiscard]] bool hasExactSnapshotFields(const QVariantMap &wire);
[[nodiscard]] bool validEpoch(const QString &epoch);
[[nodiscard]] bool validVersions(const QVariantMap &wire, quint32 *settingsSchemaVersion);

} // namespace QindaQt::Services::SettingsClient::Private
