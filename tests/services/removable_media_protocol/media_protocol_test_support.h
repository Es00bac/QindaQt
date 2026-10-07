// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QtCore/QtEndian>
namespace MediaTest {
using namespace QindaQt::RemovableMedia;
inline ActionRequest request() {
  ActionRequest v; v.requestId = QStringLiteral("request-1");
  v.lineage = {QStringLiteral(":1.23"), QStringLiteral("epoch-1"), 4};
  v.attachment = {QStringLiteral("attach-a"), 2}; v.action = Action::Mount;
  return v;
}
inline VolumeRow row(QString suffix = QStringLiteral("a")) {
  VolumeRow v; v.driveDisplayId = QStringLiteral("drive-1");
  v.volumeDisplayId = QStringLiteral("volume-") + suffix;
  v.displayName = QStringLiteral("USB storage"); v.kind = QStringLiteral("EXFAT");
  v.sizeBytes = 123456; v.attachment = {QStringLiteral("attach-") + suffix, 2};
  v.mountState = MountState::Unmounted; return v;
}
inline Snapshot snapshot() {
  Snapshot v; v.lineage = request().lineage; v.availability = Availability::Ready;
  v.rows = {row(), row(QStringLiteral("b"))}; return v;
}
inline ActionAdmission admission() {
  ActionAdmission v; v.request = request(); v.status = AdmissionStatus::Accepted;
  v.operationId = QStringLiteral("operation-1"); return v;
}
inline OperationResult result() {
  OperationResult v; v.request = request(); v.operationId = QStringLiteral("operation-1");
  v.status = OperationStatus::Applied; v.confirmingRevision = 5; return v;
}
inline void put32(QByteArray &bytes, qsizetype offset, quint32 value) {
  qToLittleEndian(value, bytes.data() + offset);
}
// Mutation locators read only trusted test-generated framing. They record
// public ABI offsets, never include the production private codec/validators.
struct Cursor {
  const QByteArray &bytes;
  qsizetype position = 0;
  qsizetype text() {
    const auto length = qFromLittleEndian<quint32>(bytes.constData() + position);
    const qsizetype body = position + 4; position = body + static_cast<qsizetype>(length);
    return body;
  }
};
struct RowOffsets {
  qsizetype drive, volume, label, attachment, rootsCount, firstRoot, mountState,
      readOnly, encrypted, locked, optical, enabled, reason, progress, outcome, diagnostic;
};
inline RowOffsets offsets(Cursor &c) {
  RowOffsets o{};
  o.drive = c.text(); o.volume = c.text(); o.label = c.text(); c.text();
  c.position += 4 + 8; o.attachment = c.text(); c.position += 8;
  o.rootsCount = c.position;
  const auto count = qFromLittleEndian<quint32>(c.bytes.constData() + c.position);
  c.position += 4;
  for (quint32 i = 0; i < count; ++i) {
    const auto body = c.text(); if (i == 0) o.firstRoot = body;
  }
  c.text(); o.mountState = c.position; c.position += 4;
  o.readOnly = c.position; c.position += 4;
  o.encrypted = c.position++; o.locked = c.position++; o.optical = c.position++;
  o.enabled = c.position; o.reason = c.position + 1; c.position += 6 * 5;
  o.progress = c.position; c.position += 4;
  o.outcome = c.position; c.position += 4;
  o.diagnostic = c.position; c.position += 4; c.text(); return o;
}
inline qsizetype snapshotCount(const QByteArray &bytes) {
  Cursor c{bytes, 16}; c.text(); c.text(); return c.position + 8 + 4;
}
inline RowOffsets firstRow(const QByteArray &bytes) {
  Cursor c{bytes, snapshotCount(bytes) + 4}; return offsets(c);
}
inline qsizetype requestAction(const QByteArray &bytes) {
  Cursor c{bytes, 16}; c.text(); c.text(); c.text(); c.position += 8;
  c.text(); return c.position + 8;
}
} // namespace MediaTest
