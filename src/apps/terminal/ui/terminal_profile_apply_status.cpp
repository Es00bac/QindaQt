// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_profile_apply_status.h"

#include "profiles/terminal_profile_settings.h"

#include <QHash>
#include <QSet>
#include <QVariantMap>

namespace QindaQt::Apps::Terminal {
namespace {

constexpr qsizetype kMaximumMessageCharacters = 160;

QString boundedMessage(const QVariant &value) {
  if (value.metaType().id() != QMetaType::QString) {
    return {};
  }
  return value.toString().simplified().left(kMaximumMessageCharacters);
}

QString keyLabel(const QString &key) {
  if (key == TerminalKeys::Profiles) {
    return QStringLiteral("Profiles");
  }
  if (key == TerminalKeys::DefaultProfile) {
    return QStringLiteral("Default profile");
  }
  if (key == TerminalKeys::RestoreTabs) {
    return QStringLiteral("Compatibility preference");
  }
  return {};
}

QString resultLabel(const QString &result) {
  return result == QLatin1String("not-attempted")
             ? QStringLiteral("not attempted")
             : result;
}

TerminalProfileApplyStatus malformedLedger() {
  return {false, TerminalProfileApplySeverity::Error,
          QStringLiteral("Error: Profile save result was malformed; the "
                         "current settings outcome is uncertain")};
}

} // namespace

TerminalProfileApplyStatus
terminalProfileApplyStatus(const QVariantList &ledger) {
  const QStringList expectedKeys = TerminalKeys::scopedKeys();
  if (ledger.size() != expectedKeys.size()) {
    return malformedLedger();
  }

  QHash<QString, QVariantMap> byKey;
  const QSet<QString> knownResults{
      QStringLiteral("applied"), QStringLiteral("failed"),
      QStringLiteral("conflict"), QStringLiteral("uncertain"),
      QStringLiteral("not-attempted")};
  for (const QVariant &rawEntry : ledger) {
    if (rawEntry.metaType().id() != QMetaType::QVariantMap) {
      return malformedLedger();
    }
    const QVariantMap entry = rawEntry.toMap();
    const QVariant keyValue = entry.value(QStringLiteral("key"));
    const QVariant resultValue = entry.value(QStringLiteral("result"));
    if (keyValue.metaType().id() != QMetaType::QString ||
        resultValue.metaType().id() != QMetaType::QString) {
      return malformedLedger();
    }
    const QString key = keyValue.toString();
    const QString result = resultValue.toString();
    if (keyLabel(key).isEmpty() || !knownResults.contains(result) ||
        byKey.contains(key)) {
      return malformedLedger();
    }
    byKey.insert(key, entry);
  }

  bool allApplied = true;
  bool hasFailed = false;
  bool hasConflict = false;
  bool hasUncertain = false;
  QStringList details;
  for (const QString &key : expectedKeys) {
    if (!byKey.contains(key)) {
      return malformedLedger();
    }
    const QVariantMap entry = byKey.value(key);
    const QString result = entry.value(QStringLiteral("result")).toString();
    allApplied = allApplied && result == QLatin1String("applied");
    hasFailed = hasFailed || result == QLatin1String("failed");
    hasConflict = hasConflict || result == QLatin1String("conflict");
    hasUncertain = hasUncertain || result == QLatin1String("uncertain");
    QString detail =
        QStringLiteral("%1: %2").arg(keyLabel(key), resultLabel(result));
    const QString message =
        boundedMessage(entry.value(QStringLiteral("message")));
    if (!message.isEmpty()) {
      detail += QStringLiteral(" — %1").arg(message);
    }
    details.append(detail);
  }

  QString prefix;
  TerminalProfileApplySeverity severity = TerminalProfileApplySeverity::Warning;
  if (allApplied) {
    prefix = QStringLiteral("Terminal profiles saved.");
    severity = TerminalProfileApplySeverity::Information;
  } else if (hasUncertain) {
    prefix = QStringLiteral("Warning: Profile save outcome is uncertain; the "
                            "write was not replayed.");
  } else if (hasConflict) {
    prefix = QStringLiteral("Warning: Profile settings changed elsewhere; "
                            "review and apply again.");
  } else if (hasFailed) {
    prefix = QStringLiteral("Error: Terminal profiles could not be saved.");
    severity = TerminalProfileApplySeverity::Error;
  } else {
    prefix = QStringLiteral("Warning: Terminal profile save was incomplete.");
  }
  return {
      allApplied, severity,
      QStringLiteral("%1 %2").arg(prefix, details.join(QStringLiteral("; ")))};
}

} // namespace QindaQt::Apps::Terminal
