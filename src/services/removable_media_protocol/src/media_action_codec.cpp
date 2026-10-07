// SPDX-License-Identifier: LGPL-3.0-or-later
#include "media_codec_p.h"
namespace QindaQt::RemovableMedia {
using namespace CodecPrivate;
EncodeResult encodeActionRequest(const ActionRequest &value) {
  if (const auto valid = validateActionRequest(value); !valid.accepted()) return invalid(valid);
  Writer w(MessageKind::ActionRequest, kMaxActionEnvelopeBytes);
  writeRequest(w, value); return w.finish();
}
CodecResult decodeActionRequest(QByteArrayView bytes, ActionRequest &destination) {
  Reader r(bytes, kMaxActionEnvelopeBytes); ActionRequest value;
  if (!(r.header(MessageKind::ActionRequest) && readRequest(r, value))) return r.result();
  return publish(r, std::move(value), destination, validateActionRequest);
}
EncodeResult encodeActionAdmission(const ActionAdmission &value) {
  if (const auto valid = validateActionAdmission(value); !valid.accepted()) return invalid(valid);
  Writer w(MessageKind::ActionAdmission, kMaxActionEnvelopeBytes);
  writeRequest(w, value.request); w.enumeration(value.status); w.text(value.operationId);
  writeDiagnostic(w, value.diagnostic); return w.finish();
}
CodecResult decodeActionAdmission(QByteArrayView bytes, ActionAdmission &destination) {
  Reader r(bytes, kMaxActionEnvelopeBytes); ActionAdmission value;
  if (!(r.header(MessageKind::ActionAdmission) && readRequest(r, value.request)
      && r.enumeration(value.status) && r.text(value.operationId, kMaxIdentifierBytes)
      && readDiagnostic(r, value.diagnostic))) return r.result();
  return publish(r, std::move(value), destination, validateActionAdmission);
}
EncodeResult encodeOperationResult(const OperationResult &value) {
  if (const auto valid = validateOperationResult(value); !valid.accepted()) return invalid(valid);
  Writer w(MessageKind::OperationResult, kMaxActionEnvelopeBytes);
  writeRequest(w, value.request); w.text(value.operationId); w.enumeration(value.status);
  writeDiagnostic(w, value.diagnostic); w.boolean(value.confirmingRevision.has_value());
  if (value.confirmingRevision) w.number(*value.confirmingRevision);
  w.enumeration(value.removalMode); return w.finish();
}
CodecResult decodeOperationResult(QByteArrayView bytes, OperationResult &destination) {
  Reader r(bytes, kMaxActionEnvelopeBytes); OperationResult value;
  if (!(r.header(MessageKind::OperationResult) && readRequest(r, value.request)
      && r.text(value.operationId, kMaxIdentifierBytes) && r.enumeration(value.status)
      && readDiagnostic(r, value.diagnostic))) return r.result();
  bool confirmed = false;
  if (!r.boolean(confirmed)) return r.result();
  if (confirmed) {
    quint64 revision = 0;
    if (!r.number(revision)) return r.result();
    value.confirmingRevision = revision;
  }
  if (!r.enumeration(value.removalMode)) return r.result();
  return publish(r, std::move(value), destination, validateOperationResult);
}
} // namespace QindaQt::RemovableMedia
