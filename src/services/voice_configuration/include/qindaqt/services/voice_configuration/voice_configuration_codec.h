// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "voice_configuration_types.h"
#include <QtCore/QVariantMap>
namespace QindaQt::Services::VoiceConfiguration {
[[nodiscard]] bool validKey(const QString &key);
[[nodiscard]] bool decodeSnapshot(const QVariantMap &map, Snapshot &destination);
[[nodiscard]] bool decodeResult(const QVariantMap &map, Result &destination);
}
