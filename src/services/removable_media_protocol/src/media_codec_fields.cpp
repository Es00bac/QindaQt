// SPDX-License-Identifier: LGPL-3.0-or-later
#include "media_codec_p.h"
namespace QindaQt::RemovableMedia::CodecPrivate {
void writeLineage(Writer &w, const Lineage &v) {
  w.text(v.owner); w.text(v.epoch); w.number(v.revision);
}
bool readLineage(Reader &r, Lineage &v) {
  return r.text(v.owner, kMaxOwnerBytes) && r.text(v.epoch, kMaxIdentifierBytes)
      && r.number(v.revision);
}
void writeAttachment(Writer &w, const Attachment &v) {
  w.text(v.handle); w.number(v.generation);
}
bool readAttachment(Reader &r, Attachment &v) {
  return r.text(v.handle, kMaxIdentifierBytes) && r.number(v.generation);
}
void writeDiagnostic(Writer &w, const Diagnostic &v) {
  w.enumeration(v.code); w.text(v.message);
}
bool readDiagnostic(Reader &r, Diagnostic &v) {
  return r.enumeration(v.code) && r.text(v.message, kMaxMessageUtf8Bytes);
}
void writeRequest(Writer &w, const ActionRequest &v) {
  w.number(v.protocolVersion); w.text(v.requestId); writeLineage(w, v.lineage);
  writeAttachment(w, v.attachment); w.enumeration(v.action);
}
bool readRequest(Reader &r, ActionRequest &v) {
  return r.number(v.protocolVersion) && r.text(v.requestId, kMaxIdentifierBytes)
      && readLineage(r, v.lineage) && readAttachment(r, v.attachment)
      && r.enumeration(v.action);
}
namespace {
void writeAvailability(Writer &w, const ActionAvailability &v) {
  w.boolean(v.enabled); w.enumeration(v.reason);
}
bool readAvailability(Reader &r, ActionAvailability &v) {
  return r.boolean(v.enabled) && r.enumeration(v.reason);
}
} // namespace
void writeRow(Writer &w, const VolumeRow &v) {
  w.text(v.driveDisplayId); w.text(v.volumeDisplayId); w.text(v.displayName); w.text(v.kind);
  w.number(v.partitionNumber); w.number(v.sizeBytes); writeAttachment(w, v.attachment);
  w.number(static_cast<quint32>(v.mountRoots.size()));
  for (const auto &root : v.mountRoots) w.text(root);
  w.text(v.preferredRoot); w.enumeration(v.mountState); w.enumeration(v.readOnly);
  w.boolean(v.encrypted); w.boolean(v.locked); w.boolean(v.optical);
  writeAvailability(w, v.actions.open); writeAvailability(w, v.actions.mount);
  writeAvailability(w, v.actions.mountReadOnly); writeAvailability(w, v.actions.unmount);
  writeAvailability(w, v.actions.remove); writeAvailability(w, v.actions.showDetails);
  w.enumeration(v.progress); w.enumeration(v.outcome); writeDiagnostic(w, v.diagnostic);
}
bool readRow(Reader &r, VolumeRow &v) {
  quint32 count = 0;
  if (!(r.text(v.driveDisplayId, kMaxIdentifierBytes)
      && r.text(v.volumeDisplayId, kMaxIdentifierBytes)
      && r.text(v.displayName, kMaxLabelUtf8Bytes) && r.text(v.kind, kMaxLabelUtf8Bytes)
      && r.number(v.partitionNumber) && r.number(v.sizeBytes) && readAttachment(r, v.attachment)
      && r.count(count, kMaxMountRoots))) return false;
  v.mountRoots.reserve(static_cast<qsizetype>(count));
  for (quint32 i = 0; i < count; ++i) {
    QString root;
    if (!r.text(root, kMaxMountRootUtf8Bytes)) return false;
    v.mountRoots.append(std::move(root));
  }
  return r.text(v.preferredRoot, kMaxMountRootUtf8Bytes) && r.enumeration(v.mountState)
      && r.enumeration(v.readOnly) && r.boolean(v.encrypted) && r.boolean(v.locked)
      && r.boolean(v.optical) && readAvailability(r, v.actions.open)
      && readAvailability(r, v.actions.mount) && readAvailability(r, v.actions.mountReadOnly)
      && readAvailability(r, v.actions.unmount) && readAvailability(r, v.actions.remove)
      && readAvailability(r, v.actions.showDetails) && r.enumeration(v.progress)
      && r.enumeration(v.outcome) && readDiagnostic(r, v.diagnostic);
}
} // namespace QindaQt::RemovableMedia::CodecPrivate
