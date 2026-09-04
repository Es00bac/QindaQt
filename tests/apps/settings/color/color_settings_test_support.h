// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QVariantMap>
#include <QtEndian>

#include <cstring>

namespace QindaQt::Apps::SettingsColor::TestSupport {

using QindaQt::Services::SettingsClient::SettingsTransport;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using WC = QindaQt::Services::SettingsProtocol::WireContract;

inline constexpr auto kAssignmentsKey = "displays.colorAssignments";

// Scriptable Settings1 transport fake. Replies are driven by the test; no
// bus, timer, or owner resolution happens inside.
class FakeSettingsTransport final : public SettingsTransport {
  Q_OBJECT
public:
  bool start(QString *error) override {
    if (!startSucceeds) {
      if (error != nullptr) *error = QStringLiteral("transport unavailable");
      return false;
    }
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &keys) override {
    snapshots.append({token, owner, keys});
  }
  void commit(quint64 token, const QString &owner, const QString &epoch,
              quint64 baseRevision, const QVariantList &operations) override {
    commits.append({token, owner, epoch, baseRevision, operations});
  }
  void requestActivation() override { ++activations; }

  struct SnapshotRequest {
    quint64 token = 0;
    QString owner;
    QStringList keys;
  };
  struct CommitRequest {
    quint64 token = 0;
    QString owner;
    QString epoch;
    quint64 revision = 0;
    QVariantList operations;
  };
  QList<SnapshotRequest> snapshots;
  QList<CommitRequest> commits;
  int activations = 0;
  bool startSucceeds = true;
};

inline QVariant assignmentValue(const QString &stableId,
                                const QString &profileId,
                                const QString &lineage) {
  return QVariantMap{
      {stableId,
       QVariantMap{{QStringLiteral("profile"), profileId},
                   {QStringLiteral("lineage"), lineage}}}};
}

inline QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                                const QVariant &assignments) {
  return {{QLatin1StringView(WC::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WC::FieldWireSchemaVersion), WC::WireSchemaVersion},
          {QLatin1StringView(WC::FieldSettingsSchemaVersion), quint32{2}},
          {QLatin1StringView(WC::FieldEpoch), epoch},
          {QLatin1StringView(WC::FieldRevision), revision},
          {QLatin1StringView(WC::FieldValues),
           QVariantMap{{QLatin1String(kAssignmentsKey), assignments}}},
          {QLatin1StringView(WC::FieldSourceLayers),
           QVariantMap{{QLatin1String(kAssignmentsKey),
                        QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WC::FieldMessage), QString{}}};
}

inline QVariantMap commitWire(SettingsWireStatus status, quint64 before,
                              quint64 after, const QVariant &authoritativeValue,
                              const QStringList &changed) {
  return {{QLatin1StringView(WC::FieldStatus), quint32(status)},
          {QLatin1StringView(WC::FieldWireSchemaVersion), WC::WireSchemaVersion},
          {QLatin1StringView(WC::FieldSettingsSchemaVersion), quint32{2}},
          {QLatin1StringView(WC::FieldEpoch), QStringLiteral("epoch-a")},
          {QLatin1StringView(WC::FieldRevisionBefore), before},
          {QLatin1StringView(WC::FieldRevisionAfter), after},
          {QLatin1StringView(WC::FieldValues),
           QVariantMap{{QLatin1String(kAssignmentsKey), authoritativeValue}}},
          {QLatin1StringView(WC::FieldSourceLayers),
           QVariantMap{{QLatin1String(kAssignmentsKey),
                        QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WC::FieldChangedKeys), changed},
          {QLatin1StringView(WC::FieldMessage), QString{}}};
}

inline QindaQt::Services::SettingsClient::ClientTiming colorTestTiming() {
  QindaQt::Services::SettingsClient::ClientTiming timing;
  timing.requestTimeoutMilliseconds = 120;
  timing.debounceMilliseconds = 0;
  timing.retryMilliseconds = {10};
  return timing;
}

// Minimal C0-valid ICC profile: 128-byte header plus a zero-entry tag table,
// declared size equal to the actual file size. Discovery reads no body, so
// these bytes are catalogable under the sanitized file stem.
inline QByteArray fixtureProfileBytes() {
  QByteArray bytes(132, '\0');
  qToBigEndian<quint32>(132, bytes.data());
  qToBigEndian<quint32>(0x04300000, bytes.data() + 8);
  std::memcpy(bytes.data() + 12, "mntr", 4);
  std::memcpy(bytes.data() + 16, "RGB ", 4);
  std::memcpy(bytes.data() + 20, "XYZ ", 4);
  std::memcpy(bytes.data() + 36, "acsp", 4);
  return bytes;
}

inline QString writeFixtureProfile(const QDir &directory,
                                   const QString &fileName,
                                   const QByteArray &bytes = fixtureProfileBytes()) {
  const QString path = directory.absoluteFilePath(fileName);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  if (file.write(bytes) != bytes.size()) return {};
  file.close();
  return path;
}

} // namespace QindaQt::Apps::SettingsColor::TestSupport
