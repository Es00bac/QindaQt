// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_protocol/network_identity.h>

#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringDecoder>

#include <array>

namespace QindaQt::Network {
namespace {

bool isRejectedPresentationCategory(const QChar::Category category) {
  switch (category) {
  case QChar::Separator_Line:
  case QChar::Separator_Paragraph:
  case QChar::Other_Control:
  case QChar::Other_Format:
  case QChar::Other_Surrogate:
  case QChar::Other_PrivateUse:
  case QChar::Other_NotAssigned:
    return true;
  default:
    return false;
  }
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

bool isPresentationSafeText(const QStringView text) {
  for (qsizetype index = 0; index < text.size(); ++index) {
    const QChar first = text.at(index);
    char32_t scalar = first.unicode();
    if (first.isHighSurrogate()) {
      if (index + 1 >= text.size() || !text.at(index + 1).isLowSurrogate()) {
        return false;
      }
      scalar = QChar::surrogateToUcs4(first, text.at(++index));
    } else if (first.isLowSurrogate()) {
      return false;
    }
    if (isRejectedPresentationCategory(QChar::category(scalar))) {
      return false;
    }
  }
  return true;
}

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
      || !isPresentationSafeText(decoded)) {
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
