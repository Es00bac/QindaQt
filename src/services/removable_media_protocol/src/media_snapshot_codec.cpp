// SPDX-License-Identifier: LGPL-3.0-or-later
#include "media_codec_p.h"
namespace QindaQt::RemovableMedia {
using namespace CodecPrivate;
EncodeResult encodeSnapshot(const Snapshot &value) {
  if (const auto valid = validateSnapshot(value); !valid.accepted()) return invalid(valid);
  Writer w(MessageKind::Snapshot, kMaxSnapshotBytes);
  w.number(value.protocolVersion); writeLineage(w, value.lineage);
  w.enumeration(value.availability); w.number(static_cast<quint32>(value.rows.size()));
  for (const auto &row : value.rows) writeRow(w, row);
  w.boolean(value.pending.has_value());
  if (value.pending) {
    w.text(value.pending->operationId); writeRequest(w, value.pending->request);
    w.enumeration(value.pending->phase);
  }
  writeDiagnostic(w, value.diagnostic); return w.finish();
}
CodecResult decodeSnapshot(QByteArrayView bytes, Snapshot &destination) {
  Reader r(bytes, kMaxSnapshotBytes); Snapshot value;
  quint32 count = 0;
  if (!(r.header(MessageKind::Snapshot) && r.number(value.protocolVersion)
      && readLineage(r, value.lineage) && r.enumeration(value.availability)
      && r.count(count, kMaxVolumes))) return r.result();
  value.rows.reserve(static_cast<qsizetype>(count));
  for (quint32 i = 0; i < count; ++i) {
    VolumeRow row;
    if (!readRow(r, row)) return r.result();
    value.rows.append(std::move(row));
  }
  bool pending = false;
  if (!r.boolean(pending)) return r.result();
  if (pending) {
    PendingOperation operation;
    if (!(r.text(operation.operationId, kMaxIdentifierBytes)
        && readRequest(r, operation.request) && r.enumeration(operation.phase))) return r.result();
    value.pending = std::move(operation);
  }
  if (!readDiagnostic(r, value.diagnostic)) return r.result();
  return publish(r, std::move(value), destination, validateSnapshot);
}
} // namespace QindaQt::RemovableMedia
