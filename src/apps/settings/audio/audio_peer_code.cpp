// SPDX-License-Identifier: LGPL-3.0-or-later
#include "audio_peer_code.h"

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

namespace QindaQt::Apps::SettingsAudio {
namespace {
constexpr auto kPrefix = "QINDAQT-AUDIO-1:";
constexpr qsizetype kMaxCodeLength = 256;

bool validTuple(const PeerCode &peer) {
  QindaQt::Audio::VbanStream stream;
  stream.name = peer.name;
  stream.outgoing = false;
  stream.host = peer.sourceIpv4;
  stream.port = peer.port;
  stream.outputNodeName = QStringLiteral("peer.code.output");
  return QindaQt::Audio::validateVbanDefinition(stream).accepted;
}
} // namespace

QString encodePeerCode(const PeerCode &peer) {
  if (!validTuple(peer)) return {};
  const QJsonObject object{{QStringLiteral("version"), 1},
                           {QStringLiteral("name"), peer.name},
                           {QStringLiteral("sourceIpv4"), peer.sourceIpv4},
                           {QStringLiteral("port"), static_cast<int>(peer.port)}};
  const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
  return QString::fromLatin1(kPrefix)
      + QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding
                                           | QByteArray::OmitTrailingEquals));
}

bool decodePeerCode(const QString &text, PeerCode *peer, QString *reason) {
  auto fail = [reason](const QString &value) {
    if (reason) *reason = value;
    return false;
  };
  const QString prefix = QString::fromLatin1(kPrefix);
  if (!text.startsWith(prefix)) return fail(QStringLiteral("foreign-code"));
  if (text.size() > kMaxCodeLength) return fail(QStringLiteral("invalid-code"));
  const QByteArray encoded = text.mid(prefix.size()).toLatin1();
  if (encoded.isEmpty()) return fail(QStringLiteral("invalid-code"));
  for (const char ch : encoded) {
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
          || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'))
      return fail(QStringLiteral("invalid-code"));
  }
  const QByteArray bytes = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding
                                                  | QByteArray::AbortOnBase64DecodingErrors);
  if (bytes.isEmpty()
      || bytes.toBase64(QByteArray::Base64UrlEncoding
                        | QByteArray::OmitTrailingEquals) != encoded)
    return fail(QStringLiteral("invalid-code"));
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return fail(QStringLiteral("invalid-code"));
  const QJsonObject object = document.object();
  if (object.size() != 4 || !object.contains(QStringLiteral("version"))
      || !object.contains(QStringLiteral("name"))
      || !object.contains(QStringLiteral("sourceIpv4"))
      || !object.contains(QStringLiteral("port")))
    return fail(QStringLiteral("invalid-code"));
  if (!object.value(QStringLiteral("version")).isDouble()
      || object.value(QStringLiteral("version")).toInt(-1) != 1)
    return fail(QStringLiteral("foreign-code"));
  if (QJsonDocument(object).toJson(QJsonDocument::Compact) != bytes)
    return fail(QStringLiteral("invalid-code"));
  if (!object.value(QStringLiteral("name")).isString()
      || !object.value(QStringLiteral("sourceIpv4")).isString()
      || !object.value(QStringLiteral("port")).isDouble())
    return fail(QStringLiteral("invalid-code"));
  const double port = object.value(QStringLiteral("port")).toDouble();
  if (port < 1 || port > 65535 || port != static_cast<int>(port))
    return fail(QStringLiteral("invalid-code"));
  const PeerCode decoded{object.value(QStringLiteral("name")).toString(),
                         object.value(QStringLiteral("sourceIpv4")).toString(),
                         static_cast<quint32>(port)};
  if (!validTuple(decoded)) return fail(QStringLiteral("invalid-code"));
  if (peer) *peer = decoded;
  if (reason) reason->clear();
  return true;
}

} // namespace QindaQt::Apps::SettingsAudio
