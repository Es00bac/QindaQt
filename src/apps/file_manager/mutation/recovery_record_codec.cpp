// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_record_codec.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtCore/QStringDecoder>
#include <limits>
#include <sys/stat.h>

namespace QindaQt::Apps::FileManager {
namespace {

bool unicodeValid(const QString &value) {
  const auto bytes = value.toUtf8();
  QStringDecoder decoder(QStringDecoder::Utf8,
      QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom);
  const QString decoded = decoder.decode(bytes);
  return !decoder.hasError() && decoded == value;
}
bool pathValid(const QString &path) {
  return path.size() <= 4096 && unicodeValid(path) && path.startsWith(QLatin1Char('/')) && path != QStringLiteral("/") &&
      !path.contains(QChar::Null) && QDir::cleanPath(path) == path &&
      QFile::decodeName(QFile::encodeName(path)) == path &&
      QFile::encodeName(path).size() <= 4096;
}

bool mountValid(const RecoveryMountObservation &value) {
  return value.mountId != 0 && value.device != 0 && value.inode != 0 &&
      S_ISDIR(value.mode);
}

bool recordValid(const RecoveryRecord &record) {
  const QUuid id(record.operationId);
  return !id.isNull() && id.toString(QUuid::WithoutBraces) == record.operationId &&
      record.sequence < maximumRecoveryRecords &&
      !recoveryPhaseKey(record.phase).isEmpty() &&
      pathValid(record.sourcePath) && pathValid(record.destinationPath) &&
      pathValid(record.stageDirectory) && pathValid(record.recoveryDirectory) &&
      record.sourcePath != record.destinationPath &&
      record.stageDirectory != record.recoveryDirectory &&
      record.sourceIdentity.valid() && record.sourceIdentity.size >= 0 &&
      (S_ISREG(record.sourceIdentity.mode) || S_ISDIR(record.sourceIdentity.mode)) &&
      mountValid(record.sourceParent) && mountValid(record.destinationParent) && mountValid(record.recoveryStorage) &&
      record.manifestDigest.size() == 32 &&
      record.diagnostic.size() <= 512 && !record.diagnostic.contains(QChar::Null) && unicodeValid(record.diagnostic);
}

QJsonObject mountJson(const RecoveryMountObservation &value) {
  return {{QStringLiteral("mount"), QString::number(value.mountId)},
          {QStringLiteral("device"), QString::number(value.device)},
          {QStringLiteral("inode"), QString::number(value.inode)},
          {QStringLiteral("mode"), QString::number(value.mode)},
          {QStringLiteral("owner"), QString::number(value.owner)}};
}

QJsonObject identityJson(const FileIdentity &value) {
  return {{QStringLiteral("device"), QString::number(value.device)},
          {QStringLiteral("inode"), QString::number(value.inode)},
          {QStringLiteral("size"), QString::number(value.size)},
          {QStringLiteral("mtime"), QString::number(value.modifiedNanoseconds)},
          {QStringLiteral("mode"), QString::number(value.mode)}};
}

template<typename Integer>
bool decimal(const QJsonObject &object, const QString &key, Integer &out) {
  const auto value = object.value(key);
  if (!value.isString()) return false;
  bool ok = false;
  if constexpr (std::numeric_limits<Integer>::is_signed) {
    const qint64 parsed = value.toString().toLongLong(&ok);
    if (!ok || parsed < std::numeric_limits<Integer>::min() ||
        parsed > std::numeric_limits<Integer>::max() ||
        QString::number(parsed) != value.toString()) return false;
    out = static_cast<Integer>(parsed);
  } else {
    const quint64 parsed = value.toString().toULongLong(&ok);
    if (!ok || parsed > std::numeric_limits<Integer>::max() ||
        QString::number(parsed) != value.toString()) return false;
    out = static_cast<Integer>(parsed);
  }
  return true;
}

bool readMount(const QJsonValue &json, RecoveryMountObservation &out) {
  if (!json.isObject()) return false;
  const auto object = json.toObject();
  return object.size() == 5 &&
      decimal(object, QStringLiteral("mount"), out.mountId) &&
      decimal(object, QStringLiteral("device"), out.device) &&
      decimal(object, QStringLiteral("inode"), out.inode) &&
      decimal(object, QStringLiteral("mode"), out.mode) &&
      decimal(object, QStringLiteral("owner"), out.owner);
}

bool readIdentity(const QJsonValue &json, FileIdentity &out) {
  if (!json.isObject()) return false;
  const auto object = json.toObject();
  return object.size() == 5 &&
      decimal(object, QStringLiteral("device"), out.device) &&
      decimal(object, QStringLiteral("inode"), out.inode) &&
      decimal(object, QStringLiteral("size"), out.size) &&
      decimal(object, QStringLiteral("mtime"), out.modifiedNanoseconds) &&
      decimal(object, QStringLiteral("mode"), out.mode);
}

RecoveryRecordRead invalid() {
  RecoveryRecordRead value;
  value.result.error = MutationError::InvalidRequest;
  value.result.diagnostic = QStringLiteral("Recovery record is invalid; preserve it for inspection.");
  return value;
}
} // namespace

QByteArray encodeRecoveryRecord(const RecoveryRecord &record) {
  if (!recordValid(record)) return {};
  const QJsonObject object{
      {QStringLiteral("schema"), 1},
      {QStringLiteral("operation"), record.operationId},
      {QStringLiteral("sequence"), QString::number(record.sequence)},
      {QStringLiteral("phase"), recoveryPhaseKey(record.phase)},
      {QStringLiteral("source"), record.sourcePath},
      {QStringLiteral("destination"), record.destinationPath},
      {QStringLiteral("stage"), record.stageDirectory},
      {QStringLiteral("recovery"), record.recoveryDirectory},
      {QStringLiteral("sourceIdentity"), identityJson(record.sourceIdentity)},
      {QStringLiteral("sourceParent"), mountJson(record.sourceParent)},
      {QStringLiteral("destinationParent"), mountJson(record.destinationParent)},
      {QStringLiteral("recoveryStorage"), mountJson(record.recoveryStorage)},
      {QStringLiteral("manifest"), QString::fromLatin1(record.manifestDigest.toHex())},
      {QStringLiteral("retainedBytesEstimate"), QString::number(record.retainedBytesEstimate)},
      {QStringLiteral("diagnostic"), record.diagnostic},
      {QStringLiteral("uncertain"), record.uncertain}};
  const QByteArray encoded = QJsonDocument(object).toJson(QJsonDocument::Compact);
  return encoded.size() <= maximumRecoveryRecordBytes ? encoded : QByteArray{};
}

RecoveryRecordRead decodeRecoveryRecord(const QByteArray &bytes) {
  if (bytes.isEmpty() || bytes.size() > maximumRecoveryRecordBytes) return invalid();
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return invalid();
  // AGENT-GUARD: Qt's parser silently collapses duplicate JSON keys. Exact
  // canonical-byte equality rejects duplicates, alternate numbers and trailing
  // data before this record can be considered even inspection evidence.
  if (document.toJson(QJsonDocument::Compact) != bytes) return invalid();
  const auto object = document.object();
  if (object.size() != 16 || object.value(QStringLiteral("schema")) != QJsonValue(1))
    return invalid();
  RecoveryRecord record;
  auto string = [&](const char *key, QString &target) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString()) return false;
    target = value.toString();
    return true;
  };
  QString phase;
  QString digest;
  if (!string("operation", record.operationId) ||
      !string("source", record.sourcePath) ||
      !string("destination", record.destinationPath) ||
      !string("stage", record.stageDirectory) ||
      !string("recovery", record.recoveryDirectory) ||
      !string("diagnostic", record.diagnostic) ||
      !string("phase", phase) || !string("manifest", digest) ||
      !decimal(object, QStringLiteral("sequence"), record.sequence) ||
      !decimal(object, QStringLiteral("retainedBytesEstimate"), record.retainedBytesEstimate) ||
      !readIdentity(object.value(QStringLiteral("sourceIdentity")), record.sourceIdentity) ||
      !readMount(object.value(QStringLiteral("sourceParent")), record.sourceParent) ||
      !readMount(object.value(QStringLiteral("destinationParent")), record.destinationParent) ||
      !readMount(object.value(QStringLiteral("recoveryStorage")), record.recoveryStorage) ||
      !object.value(QStringLiteral("uncertain")).isBool()) return invalid();
  const auto parsedPhase = recoveryPhaseFromKey(phase);
  if (!parsedPhase) return invalid();
  record.phase = *parsedPhase;
  record.uncertain = object.value(QStringLiteral("uncertain")).toBool();
  record.manifestDigest = QByteArray::fromHex(digest.toLatin1());
  if (QString::fromLatin1(record.manifestDigest.toHex()) != digest ||
      !recordValid(record) || encodeRecoveryRecord(record) != bytes) return invalid();
  return {{}, record};
}

} // namespace QindaQt::Apps::FileManager
