// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_protocol/network_redaction.h>

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QVariantList>

namespace QindaQt::Network {
namespace {

constexpr qsizetype kMaximumDiagnosticScanCodeUnits =
    kMaxDiagnosticUtf8Bytes * 8;

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

bool containsSecrets(const QVariant &value, const int depth,
                     qsizetype &remainingNodes) {
  if (depth > kMaximumIntentWireDepth || remainingNodes <= 0) {
    // Over-deep nesting is itself rejected by validation elsewhere; treat it
    // as hostile here rather than recursing blindly.
    return true;
  }
  --remainingNodes;
  const QVariantList list = toList(value);
  if (list.size() > kMaximumIntentWireContainerEntries) {
    return true;
  }
  for (const QVariant &item : list) {
    if (containsSecrets(item, depth + 1, remainingNodes)) {
      return true;
    }
  }
  const QVariantMap map = toMap(value);
  if (map.size() > kMaximumIntentWireContainerEntries) {
    return true;
  }
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    if (isSecretKeyName(it.key())
        || containsSecrets(it.value(), depth + 1, remainingNodes)) {
      return true;
    }
  }
  return false;
}

bool isKeyCharacter(const QChar character) {
  return character.isLetterOrNumber() || character == u'_'
         || character == u'-' || character == u'.';
}

struct Assignment {
  qsizetype keyStart = -1;
  qsizetype keyEnd = -1;
  qsizetype separator = -1;
};

Assignment assignmentAtSeparator(QStringView message, qsizetype separator) {
  qsizetype keyEnd = separator;
  while (keyEnd > 0 && message.at(keyEnd - 1).isSpace()) {
    --keyEnd;
  }
  qsizetype keyStart = keyEnd;
  while (keyStart > 0 && isKeyCharacter(message.at(keyStart - 1))) {
    --keyStart;
  }
  if (keyStart == keyEnd
      || (keyStart > 0 && isKeyCharacter(message.at(keyStart - 1)))) {
    return {};
  }
  return {keyStart, keyEnd, separator};
}

Assignment findAssignment(QStringView message, qsizetype from,
                          bool secretsOnly) {
  for (qsizetype index = from; index < message.size(); ++index) {
    if (message.at(index) != u'=' && message.at(index) != u':') {
      continue;
    }
    const Assignment found = assignmentAtSeparator(message, index);
    if (found.keyStart < 0) {
      continue;
    }
    const QStringView key =
        message.sliced(found.keyStart, found.keyEnd - found.keyStart);
    if (!secretsOnly || isSecretKeyName(key)) {
      return found;
    }
  }
  return {};
}

qsizetype findUnquotedValueEnd(QStringView message, qsizetype valueStart) {
  for (qsizetype index = valueStart; index < message.size(); ++index) {
    if (message.at(index) == u';' || message.at(index) == u',') {
      return index;
    }
    if (!message.at(index).isSpace()) {
      continue;
    }
    qsizetype next = index;
    while (next < message.size() && message.at(next).isSpace()) {
      ++next;
    }
    const Assignment following = findAssignment(message, next, false);
    if (following.keyStart == next) {
      return index;
    }
  }
  return message.size();
}

QString clampDiagnostic(QString value) {
  const QByteArray bytes = value.toUtf8();
  if (bytes.size() <= kMaxDiagnosticUtf8Bytes) {
    return value;
  }

  const QString suffix = QStringLiteral("…");
  const qsizetype contentBudget =
      kMaxDiagnosticUtf8Bytes - suffix.toUtf8().size();
  QString bounded;
  bounded.reserve(qMin(value.size(), contentBudget));
  for (qsizetype index = 0; index < value.size(); ++index) {
    const qsizetype units = value.at(index).isHighSurrogate() ? 2 : 1;
    if (index + units > value.size()) {
      break;
    }
    const QStringView scalar = QStringView(value).sliced(index, units);
    if (bounded.toUtf8().size() + scalar.toUtf8().size() > contentBudget) {
      break;
    }
    bounded.append(scalar);
    index += units - 1;
  }
  bounded.append(suffix);
  return bounded;
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
         || suffixMatch(name, QStringView(u".password"))
         || suffixMatch(name, QStringView(u"_password"))
         || suffixMatch(name, QStringView(u"-passphrase"))
         || suffixMatch(name, QStringView(u".passphrase"))
         || suffixMatch(name, QStringView(u"_passphrase"))
         || suffixMatch(name, QStringView(u"-psk"))
         || suffixMatch(name, QStringView(u".psk"))
         || suffixMatch(name, QStringView(u"_psk"))
         || suffixMatch(name, QStringView(u"-secret"))
         || suffixMatch(name, QStringView(u".secret"))
         || suffixMatch(name, QStringView(u"_secret"));
}

bool wireContainsSecrets(const QVariantMap &wire) {
  qsizetype remainingNodes = kMaximumIntentWireNodes;
  return containsSecrets(wire, 0, remainingNodes);
}

QString redactDiagnostic(QString message) {
  if (message.size() > kMaximumDiagnosticScanCodeUnits) {
    message.truncate(kMaximumDiagnosticScanCodeUnits);
  }
  if (!isPresentationSafeText(message)) {
    return QStringLiteral("network diagnostic withheld");
  }
  const QString sanitized = std::move(message);
  const QStringView view(sanitized);
  QString redacted;
  redacted.reserve(qMin(sanitized.size(), kMaxDiagnosticUtf8Bytes));
  qsizetype cursor = 0;
  while (cursor < view.size()) {
    const Assignment assignment = findAssignment(view, cursor, true);
    if (assignment.keyStart < 0) {
      redacted.append(view.sliced(cursor));
      break;
    }
    redacted.append(view.sliced(cursor, assignment.keyStart - cursor));
    redacted.append(view.sliced(assignment.keyStart,
                                assignment.keyEnd - assignment.keyStart));
    redacted.append(QStringLiteral("=<redacted>"));

    qsizetype valueStart = assignment.separator + 1;
    while (valueStart < view.size() && view.at(valueStart).isSpace()) {
      ++valueStart;
    }
    if (valueStart >= view.size()) {
      cursor = view.size();
      break;
    }
    const QChar quote = view.at(valueStart);
    if (quote == u'\'' || quote == u'"') {
      qsizetype closing = valueStart + 1;
      while (closing < view.size() && view.at(closing) != quote) {
        if (view.at(closing) == u'\\' && closing + 1 < view.size()) {
          closing += 2;
          continue;
        }
        ++closing;
      }
      if (closing == view.size()) {
        // AGENT-GUARD: An unterminated quoted credential consumes the rest of
        // the diagnostic. Preserving any suffix could disclose secret bytes.
        cursor = view.size();
        break;
      }
      cursor = closing + 1;
      continue;
    }
    cursor = findUnquotedValueEnd(view, valueStart);
  }
  return clampDiagnostic(std::move(redacted));
}

} // namespace QindaQt::Network
