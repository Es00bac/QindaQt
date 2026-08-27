// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::SettingsClient::Private {

[[nodiscard]] std::optional<quint64> exactUnsigned64(const QVariant &value);
[[nodiscard]] std::optional<SettingsProtocol::SettingsWireStatus> wireStatus(
    const QVariantMap &wire);
[[nodiscard]] std::optional<QVariantMap> boundedValueMap(const QVariant &wireValue);
[[nodiscard]] std::optional<QVariantMap> boundedSourceMap(const QVariant &wireValue);
[[nodiscard]] bool validEpoch(const QString &epoch);
[[nodiscard]] bool validVersions(const QVariantMap &wire, quint32 *settingsSchemaVersion);

} // namespace QindaQt::Services::SettingsClient::Private
