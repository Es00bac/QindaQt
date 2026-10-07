// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_validation.h>
namespace QindaQt::RemovableMedia::ValidationPrivate {
[[nodiscard]] ValueError text(const QString &value, qsizetype maximum, bool required = false);
[[nodiscard]] ValueError identifier(const QString &value);
[[nodiscard]] ValueError lineage(const Lineage &value, bool allowUnbound = false);
[[nodiscard]] ValueError attachment(const Attachment &value);
[[nodiscard]] ValueError diagnostic(const Diagnostic &value);
[[nodiscard]] ValueError row(const VolumeRow &value);
template<class Enum> bool validEnum(Enum value, Enum maximum) {
  return static_cast<quint32>(value) <= static_cast<quint32>(maximum);
}
} // namespace QindaQt::RemovableMedia::ValidationPrivate
