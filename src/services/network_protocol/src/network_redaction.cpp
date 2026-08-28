// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_protocol/network_redaction.h>

#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QRegularExpression>
#include <QtCore/QVariantList>

namespace QindaQt::Network {
namespace {

constexpr int kMaximumWireDepth = 8;

bool suffixMatch(QStringView key, QStringView suffix) {
  return key.size() >= suffix.size()
         && key.right(suffix.size()).compare(suffix, Qt::CaseInsensitive) == 0;
}

bool equalsIgnoreCase(QStringView left, QStringView right) {
  return left.compare(right, Qt::CaseInsensitive) == 0;
}

QVariantMap toMap(const QVariant &value) {
  if (value.userType() != QMetaType::QVariantMap) {
    return {};
  }
  return value.toMap();
}

QVariantList toList(const QVariant &value) {
  if (value.userType() != QMetaType::QVariantList) {
    return {};
  }
  return value.toList();
}

bool containsSecrets(const QVariant &value, const int depth) {
  if (depth > kMaximumWireDepth) {
    // Over-deep nesting is itself rejected by validation elsewhere; treat it
    // as hostile here rather than recursing blindly.
    return true;
  }
  const QVariantList list = toList(value);
  for (const QVariant &item : list) {
    if (containsSecrets(item, depth + 1)) {
      return true;
    }
  }
  const QVariantMap map = toMap(value);
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    if (isSecretKeyName(it.key()) || containsSecrets(it.value(), depth + 1)) {
      return true;
    }
  }
  return false;
}

} // namespace

bool isSecretKeyName(const QStringView key) {
  if (key.isEmpty()) {
    return false;
  }
  const QStringView trimmed = key.trimmed();
  const QStringView name(trimmed);
  static const QList<QStringView> exact{
      QStringView(u"passphrase"), QStringView(u"password"), QStringView(u"psk"),
      QStringView(u"secret"), QStringView(u"private-key"),
      QStringView(u"privatekey"), QStringView(u"client-key"),
      QStringView(u"8021x-password"), QStringView(u"eap-password"),
      QStringView(u"wpa-psk")};
  for (const QStringView candidate : exact) {
    if (equalsIgnoreCase(name, candidate)) {
      return true;
    }
  }
  return suffixMatch(name, QStringView(u"-password"))
         || suffixMatch(name, QStringView(u"-passphrase"))
         || suffixMatch(name, QStringView(u"-psk"))
         || suffixMatch(name, QStringView(u"-secret"));
}

bool wireContainsSecrets(const QVariantMap &wire) {
  return containsSecrets(wire, 0);
}

QString redactDiagnostic(QString message) {
  static const QRegularExpression assignment(
      QStringLiteral("(?i)(\\b[\\w.-]*(?:password|passphrase|psk|secret)"
                     "[\\w.-]*)\\s*[:=]\\s*[^\\s;,\"]+"));
  QString redacted =
      message.replace(assignment, QStringLiteral("\\1=<redacted>"));
  redacted.remove(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0b-\\x1f]")));
  if (redacted.toUtf8().size() > kMaxDiagnosticUtf8Bytes) {
    while (!redacted.isEmpty()
           && redacted.toUtf8().size() > kMaxDiagnosticUtf8Bytes) {
      redacted.chop(1);
    }
    redacted.append(QStringLiteral("…"));
  }
  return redacted;
}

} // namespace QindaQt::Network
