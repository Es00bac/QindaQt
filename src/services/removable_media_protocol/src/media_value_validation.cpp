// SPDX-License-Identifier: LGPL-3.0-or-later
#include "media_validation_p.h"
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>

namespace QindaQt::RemovableMedia::ValidationPrivate {
ValueError text(const QString &value, const qsizetype maximum, const bool required) {
  if (value.size() > maximum) return ValueError::LimitExceeded;
  const QByteArray bytes = value.toUtf8();
  if (bytes.size() > maximum) return ValueError::LimitExceeded;
  QStringDecoder decoder(QStringDecoder::Utf8,
      QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom);
  const QString roundTrip = decoder.decode(bytes);
  if ((required && value.isEmpty()) || value.contains(QChar::Null)
      || decoder.hasError() || roundTrip != value) return ValueError::InvalidText;
  return ValueError::None;
}
ValueError identifier(const QString &value) {
  if (value.isEmpty() || value.size() > kMaxIdentifierBytes)
    return ValueError::InvalidIdentifier;
  for (const auto c : value) {
    const auto u = c.unicode();
    if (!((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z')
          || (u >= '0' && u <= '9') || u == '_' || u == '-'))
      return ValueError::InvalidIdentifier;
  }
  return ValueError::None;
}
ValueError lineage(const Lineage &value, const bool allowUnbound) {
  if (allowUnbound && value.owner.isEmpty() && value.epoch.isEmpty()
      && value.revision == 0) return ValueError::None;
  const auto &owner = value.owner;
  if (owner.size() < 4 || owner.size() > kMaxOwnerBytes || !owner.startsWith(':'))
    return ValueError::InvalidOwner;
  bool dotted = false, element = false;
  for (qsizetype i = 1; i < owner.size(); ++i) {
    const auto u = owner.at(i).unicode();
    if (u == '.') {
      if (!element) return ValueError::InvalidOwner;
      dotted = true; element = false;
    } else if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z')
               || (u >= '0' && u <= '9') || u == '_' || u == '-') {
      element = true;
    } else return ValueError::InvalidOwner;
  }
  if (!dotted || !element) return ValueError::InvalidOwner;
  if (identifier(value.epoch) != ValueError::None || value.revision == 0)
    return ValueError::InvalidLineage;
  return ValueError::None;
}
ValueError attachment(const Attachment &value) {
  if (identifier(value.handle) != ValueError::None || value.generation == 0)
    return ValueError::InvalidAttachment;
  return ValueError::None;
}
ValueError diagnostic(const Diagnostic &value) {
  if (!validEnum(value.code, DiagnosticCode::Uncertain)) return ValueError::InvalidEnum;
  return text(value.message, kMaxMessageUtf8Bytes);
}
namespace {
ValueError mountRoot(const QString &value) {
  const auto result = text(value, kMaxMountRootUtf8Bytes, true);
  if (result != ValueError::None) return result;
  if (!value.startsWith('/') || value.startsWith("//")
      || (value.size() > 1 && value.endsWith('/')))
    return ValueError::InvalidMountRoot;
  if (value == QStringLiteral("/")) return ValueError::None;
  for (const auto &part : value.mid(1).split('/')) {
    if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral(".."))
      return ValueError::InvalidMountRoot;
  }
  return ValueError::None;
}
ValueError actionAvailability(const ActionAvailability &value) {
  if (!validEnum(value.reason, DisabledReason::NotAdmitted)) return ValueError::InvalidEnum;
  if (value.enabled != (value.reason == DisabledReason::None))
    return ValueError::InconsistentValue;
  return ValueError::None;
}
} // namespace
ValueError row(const VolumeRow &value) {
  for (const auto &id : {value.driveDisplayId, value.volumeDisplayId})
    if (identifier(id) != ValueError::None) return ValueError::InvalidIdentifier;
  if (const auto error = attachment(value.attachment); error != ValueError::None) return error;
  for (const auto &label : {value.displayName, value.kind})
    if (const auto error = text(label, kMaxLabelUtf8Bytes, true); error != ValueError::None) return error;
  if (!validEnum(value.mountState, MountState::Mounted)
      || !validEnum(value.readOnly, ReadOnlyState::Writable)
      || !validEnum(value.progress, ProgressPhase::Refreshing)
      || !validEnum(value.outcome, OperationStatus::Uncertain)) return ValueError::InvalidEnum;
  if (value.mountRoots.size() > kMaxMountRoots) return ValueError::LimitExceeded;
  QSet<QString> roots;
  for (const auto &root : value.mountRoots) {
    if (const auto error = mountRoot(root); error != ValueError::None) return error;
    if (roots.contains(root)) return ValueError::DuplicateIdentity;
    roots.insert(root);
  }
  if (value.mountState == MountState::Mounted) {
    if (roots.isEmpty() || !roots.contains(value.preferredRoot)) return ValueError::InconsistentValue;
  } else if (!roots.isEmpty() || !value.preferredRoot.isEmpty()) return ValueError::InconsistentValue;
  if (value.locked && (!value.encrypted || value.mountState == MountState::Mounted))
    return ValueError::InconsistentValue;
  for (const auto &available : {value.actions.open, value.actions.mount,
       value.actions.mountReadOnly, value.actions.unmount, value.actions.remove,
       value.actions.showDetails}) {
    if (const auto error = actionAvailability(available); error != ValueError::None) return error;
  }
  return diagnostic(value.diagnostic);
}
} // namespace QindaQt::RemovableMedia::ValidationPrivate
