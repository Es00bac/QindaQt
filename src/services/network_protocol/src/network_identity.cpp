// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_protocol/network_identity.h>

#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringDecoder>

#include <array>

namespace QindaQt::Network {
namespace {

bool isPrintableNonControl(const QString &text) {
  for (const QChar character : text) {
    const char32_t code = character.unicode();
    if (code <= 0x1FU || code == 0x7FU) {
      return false;
    }
  }
  return true;
}

QString digestToHex(const QByteArray &digest) {
  static constexpr char hexDigits[] = "0123456789abcdef";
  QString hex;
  hex.reserve(64);
  for (int index = 0; index < digest.size(); ++index) {
    const quint8 byte = static_cast<quint8>(digest.at(index));
    hex.append(QLatin1Char(hexDigits[byte >> 4U]));
    hex.append(QLatin1Char(hexDigits[byte & 0x0FU]));
  }
  return hex;
}

} // namespace

SsidIdentity normalizeSsid(const QByteArrayView rawSsid) {
  SsidIdentity identity;
  if (rawSsid.size() < 0 || rawSsid.size() > kMaxSsidRawBytes) {
    return identity;
  }
  if (rawSsid.isEmpty()) {
    identity.hidden = true;
    identity.valid = true;
    return identity;
  }
  QStringDecoder decoder(QStringDecoder::Utf8);
  QString decoded = decoder.decode(rawSsid);
  if (decoder.hasError() || decoded.contains(QChar::Null)
      || !isPrintableNonControl(decoded)) {
    // AGENT-NOTE: Non-printable or non-UTF-8 SSIDs are legal on the wire but
    // must not reach presentation as mojibake that could be spoofed into a
    // lookalike identity; they are modeled as hidden networks instead.
    identity.hidden = true;
    identity.valid = true;
    return identity;
  }
  identity.text = std::move(decoded);
  identity.valid = true;
  return identity;
}

bool normalizeBssid(const QString &rawBssid, QString *normalized) {
  static const QRegularExpression pattern(
      QStringLiteral("^[0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5}$"));
  const QRegularExpressionMatch match = pattern.match(rawBssid);
  if (!match.hasMatch()) {
    return false;
  }
  if (normalized != nullptr) {
    *normalized = rawBssid.toLower();
  }
  return true;
}

bool normalizeInterfaceName(const QString &rawName, QString *normalized) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,14}$"));
  if (!pattern.match(rawName).hasMatch()) {
    return false;
  }
  if (normalized != nullptr) {
    *normalized = rawName;
  }
  return true;
}

QString knownNetworkId(const QByteArrayView rawSsid, const SecuritySuite security) {
  if (rawSsid.size() < 0 || rawSsid.size() > kMaxSsidRawBytes) {
    return {};
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(rawSsid.toByteArray());
  std::array<char, 1> suiteMarker{
      static_cast<char>(static_cast<quint32>(security) & 0xFFU)};
  hash.addData(QByteArrayView(suiteMarker.data(), 1));
  // The full 64-character digest doubles as a collision-resistant handle; the
  // kMaxNetworkIdUtf8Bytes cap is sized for exactly this representation.
  return digestToHex(hash.result());
}

} // namespace QindaQt::Network
