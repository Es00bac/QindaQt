// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/network_protocol/network_codec.h>

#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include "network_codec_p.h"

#include <algorithm>
#include <iterator>

namespace QindaQt::Network::CodecPrivate {

DecodeResult readerFailure(const Reader &reader, QString reasonCode) {
  return {.error = reader.error(), .reasonCode = std::move(reasonCode)};
}

void writeRadio(Writer &writer, const Radio &radio) {
  writer.u32(static_cast<quint32>(radio.kind));
  writer.boolean(radio.present);
  writer.boolean(radio.hardwareEnabled);
  writer.boolean(radio.softwareEnabled);
}

bool readRadio(Reader &reader, Radio &radio) {
  quint32 kind = 0;
  if (!reader.u32(kind) || !reader.boolean(radio.present)
      || !reader.boolean(radio.hardwareEnabled)
      || !reader.boolean(radio.softwareEnabled)) {
    return false;
  }
  radio.kind = static_cast<RadioKind>(kind);
  return true;
}

void writeDevice(Writer &writer, const Device &device) {
  writer.text(device.interfaceName);
  writer.u32(static_cast<quint32>(device.kind));
  writer.u32(static_cast<quint32>(device.state));
}

bool readDevice(Reader &reader, Device &device) {
  quint32 kind = 0;
  quint32 state = 0;
  if (!reader.text(device.interfaceName, kMaxInterfaceUtf8Bytes)
      || !reader.u32(kind) || !reader.u32(state)) {
    return false;
  }
  device.kind = static_cast<DeviceKind>(kind);
  device.state = static_cast<DeviceState>(state);
  return true;
}

void writeAccessPoint(Writer &writer, const AccessPoint &point) {
  writer.text(point.deviceInterface);
  writer.text(point.ssid);
  writer.boolean(point.hidden);
  writer.text(point.bssid);
  writer.u32(static_cast<quint32>(point.security));
  writer.u32(point.frequencyMHz);
  writer.u32(point.signalStrength);
}

bool readAccessPoint(Reader &reader, AccessPoint &point) {
  quint32 security = 0;
  if (!reader.text(point.deviceInterface, kMaxInterfaceUtf8Bytes)
      || !reader.text(point.ssid, kMaxSsidUtf8Bytes)
      || !reader.boolean(point.hidden)
      || !reader.text(point.bssid, kMaxBssidUtf8Bytes) || !reader.u32(security)
      || !reader.u32(point.frequencyMHz)
      || !reader.u32(point.signalStrength)) {
    return false;
  }
  point.security = static_cast<SecuritySuite>(security);
  return true;
}

void writeKnownNetwork(Writer &writer, const KnownNetwork &network) {
  writer.text(network.id);
  writer.text(network.ssid);
  writer.boolean(network.hidden);
  writer.u32(static_cast<quint32>(network.security));
  writer.boolean(network.autoConnect);
}

bool readKnownNetwork(Reader &reader, KnownNetwork &network) {
  quint32 security = 0;
  if (!reader.text(network.id, kMaxNetworkIdUtf8Bytes)
      || !reader.text(network.ssid, kMaxSsidUtf8Bytes)
      || !reader.boolean(network.hidden) || !reader.u32(security)
      || !reader.boolean(network.autoConnect)) {
    return false;
  }
  network.security = static_cast<SecuritySuite>(security);
  return true;
}

void writeActiveConnection(Writer &writer, const ActiveConnection &connection) {
  writer.text(connection.deviceInterface);
  writer.text(connection.knownNetworkId);
}

bool readActiveConnection(Reader &reader, ActiveConnection &connection) {
  return reader.text(connection.deviceInterface, kMaxInterfaceUtf8Bytes)
         && reader.text(connection.knownNetworkId, kMaxNetworkIdUtf8Bytes);
}

void writeScanLease(Writer &writer, const ScanLease &lease) {
  writer.text(lease.leaseId);
  writer.u64(lease.grantedEpoch);
  writer.u64(lease.grantedRevision);
  writer.i64(lease.deadlineEpochMs);
}

bool readScanLease(Reader &reader, ScanLease &lease) {
  return reader.text(lease.leaseId, kMaxLeaseIdUtf8Bytes)
         && reader.u64(lease.grantedEpoch) && reader.u64(lease.grantedRevision)
         && reader.i64(lease.deadlineEpochMs);
}

} // namespace QindaQt::Network::CodecPrivate

namespace QindaQt::Network {
using namespace CodecPrivate;

EncodeResult encodeSnapshot(const Snapshot &snapshot) {
  if (const ValidationResult validation = validateSnapshot(snapshot);
      !validation.accepted) {
    return {.payload = {},
            .error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  Writer writer;
  writer.raw(kSnapshotMagic, std::size(kSnapshotMagic));
  writer.u32(kCanonicalCodecVersion);
  writer.u32(snapshot.protocolVersion);
  writer.text(snapshot.owner);
  writer.u64(snapshot.epoch);
  writer.u64(snapshot.revision);
  writer.u32(static_cast<quint32>(snapshot.availability));
  writer.u32(static_cast<quint32>(snapshot.capabilities.toInt()));
  writer.u32(static_cast<quint32>(snapshot.connectivity));
  writer.text(snapshot.reasonCode);
  writer.text(snapshot.diagnostic);
  writeList(writer, snapshot.radios, writeRadio);
  writeList(writer, snapshot.devices, writeDevice);
  writeList(writer, snapshot.accessPoints, writeAccessPoint);
  writeList(writer, snapshot.knownNetworks, writeKnownNetwork);
  writeList(writer, snapshot.activeConnections, writeActiveConnection);
  writer.u32(static_cast<quint32>(snapshot.scanPhase));
  writeScanLease(writer, snapshot.scanLease);
  if (!writer.good()) {
    return {.payload = {},
            .error = CodecError::PayloadTooLarge,
            .reasonCode = QStringLiteral("snapshot-encode-failed")};
  }
  return {.payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeSnapshot(const QByteArrayView payload,
                            Snapshot &destination) {
  Reader reader(payload);
  char magic[std::size(kSnapshotMagic)]{};
  quint32 codecVersion = 0;
  quint32 availability = 0;
  quint32 capabilities = 0;
  quint32 connectivity = 0;
  quint32 scanPhase = 0;
  Snapshot snapshot;
  if (!reader.raw(magic, std::size(kSnapshotMagic))) {
    return readerFailure(reader, QStringLiteral("truncated-snapshot"));
  }
  if (!std::equal(std::begin(magic), std::end(magic),
                  std::begin(kSnapshotMagic))) {
    return {.error = CodecError::InvalidMagic,
            .reasonCode = QStringLiteral("invalid-snapshot-magic")};
  }
  if (!reader.u32(codecVersion)) {
    return readerFailure(reader, QStringLiteral("truncated-snapshot"));
  }
  if (codecVersion != kCanonicalCodecVersion) {
    return {.error = CodecError::UnsupportedCodecVersion,
            .reasonCode = QStringLiteral("unsupported-codec-version")};
  }
  if (!reader.u32(snapshot.protocolVersion)
         || !reader.text(snapshot.owner, kMaxOwnerUtf8Bytes)
         || !reader.u64(snapshot.epoch) || !reader.u64(snapshot.revision)
         || !reader.u32(availability) || !reader.u32(capabilities)
         || !reader.u32(connectivity)
         || !reader.text(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes)
         || !reader.text(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes)
         || !readBoundedList(reader, snapshot.radios, kMaxRadios, readRadio)
         || !readBoundedList(reader, snapshot.devices, kMaxDevices, readDevice)
         || !readBoundedList(reader, snapshot.accessPoints, kMaxAccessPoints,
                             readAccessPoint)
         || !readBoundedList(reader, snapshot.knownNetworks, kMaxKnownNetworks,
                             readKnownNetwork)
         || !readBoundedList(reader, snapshot.activeConnections,
                             kMaxActiveConnections, readActiveConnection)
         || !reader.u32(scanPhase) || !readScanLease(reader, snapshot.scanLease)) {
    return readerFailure(reader, QStringLiteral("invalid-snapshot-field"));
  }
  if (!reader.finished()) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = QStringLiteral("trailing-snapshot-bytes")};
  }
  snapshot.availability = static_cast<Availability>(availability);
  snapshot.capabilities = Capabilities::fromInt(capabilities);
  snapshot.connectivity = static_cast<ConnectivityKind>(connectivity);
  snapshot.scanPhase = static_cast<ScanPhase>(scanPhase);
  if (const ValidationResult validation = validateSnapshot(snapshot);
      !validation.accepted) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  destination = std::move(snapshot);
  return {};
}

EncodeResult encodeOperationResult(const OperationResult &result) {
  if (const ValidationResult validation = validateOperationResult(result);
      !validation.accepted) {
    return {.payload = {},
            .error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  Writer writer;
  writer.raw(kOperationResultMagic, std::size(kOperationResultMagic));
  writer.u32(kCanonicalCodecVersion);
  writer.u32(static_cast<quint32>(result.kind));
  writer.u32(static_cast<quint32>(result.status));
  writer.u64(result.initiatingEpoch);
  writer.u64(result.initiatingRevision);
  writer.text(result.reasonCode);
  writer.text(result.diagnostic);
  if (!writer.good()) {
    return {.payload = {},
            .error = CodecError::PayloadTooLarge,
            .reasonCode = QStringLiteral("operation-result-encode-failed")};
  }
  return {
      .payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeOperationResult(const QByteArrayView payload,
                                   OperationResult &destination) {
  Reader reader(payload);
  char magic[std::size(kOperationResultMagic)]{};
  quint32 codecVersion = 0;
  quint32 kind = 0;
  quint32 status = 0;
  OperationResult result;
  if (!reader.raw(magic, std::size(kOperationResultMagic))) {
    return readerFailure(reader, QStringLiteral("truncated-operation-result"));
  }
  if (!std::equal(std::begin(magic), std::end(magic),
                  std::begin(kOperationResultMagic))) {
    return {.error = CodecError::InvalidMagic,
            .reasonCode = QStringLiteral("invalid-operation-result-magic")};
  }
  if (!reader.u32(codecVersion)) {
    return readerFailure(reader, QStringLiteral("truncated-operation-result"));
  }
  if (codecVersion != kCanonicalCodecVersion) {
    return {.error = CodecError::UnsupportedCodecVersion,
            .reasonCode = QStringLiteral("unsupported-codec-version")};
  }
  if (!reader.u32(kind) || !reader.u32(status)
         || !reader.u64(result.initiatingEpoch)
         || !reader.u64(result.initiatingRevision)
         || !reader.text(result.reasonCode, kMaxReasonCodeUtf8Bytes)
         || !reader.text(result.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return readerFailure(reader, QStringLiteral("invalid-operation-result-field"));
  }
  if (!reader.finished()) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = QStringLiteral("trailing-operation-result-bytes")};
  }
  result.kind = static_cast<OperationKind>(kind);
  result.status = static_cast<OperationStatus>(status);
  if (const ValidationResult validation = validateOperationResult(result);
      !validation.accepted) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  destination = std::move(result);
  return {};
}

} // namespace QindaQt::Network
