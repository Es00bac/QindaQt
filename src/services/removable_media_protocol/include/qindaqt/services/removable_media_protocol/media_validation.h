// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_types.h>
namespace QindaQt::RemovableMedia {
enum class ValueError {
  None, UnsupportedVersion, InvalidEnum, InvalidText, InvalidIdentifier,
  InvalidOwner, InvalidLineage, InvalidAttachment, InvalidMountRoot,
  LimitExceeded, DuplicateIdentity, InconsistentValue
};
struct ValidationResult final {
  ValueError error = ValueError::None;
  [[nodiscard]] bool accepted() const noexcept { return error == ValueError::None; }
};
// Reentrant structural checks only, without filesystem/bus access. These
// checks neither admit an action nor attest that reported mount/safety is true.
[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &value);
[[nodiscard]] ValidationResult validateActionRequest(const ActionRequest &value);
[[nodiscard]] ValidationResult validateActionAdmission(const ActionAdmission &value);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &value);
} // namespace QindaQt::RemovableMedia
