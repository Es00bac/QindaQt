// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/lock_preferences/legacy_lock_import.h"

#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>

namespace QindaQt::Services::LockPreferences {
namespace {
const QString marker = QStringLiteral("lock.migration.kscreenlockerImported");
bool legacyBool(const QVariant &value, bool *output) {
  const auto text = value.toString().trimmed().toLower();
  if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
    *output = true;
    return true;
  }
  if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
    *output = false;
    return true;
  }
  return false;
}
} // namespace

bool readLegacyPreferences(const QString &path, QVariantMap *output) {
  if (!output || path.isEmpty())
    return false;
  QFile file(path);
  constexpr qint64 maximumBytes = 1024 * 1024;
  if (!file.open(QIODevice::ReadOnly) || file.size() < 1 ||
      file.size() > maximumBytes)
    return false;
  const auto snapshot = file.read(maximumBytes + 1);
  if (snapshot.size() > maximumBytes || file.error() != QFileDevice::NoError)
    return false;
  const auto contents = QString::fromUtf8(snapshot);
  if (contents.toUtf8() != snapshot || contents.contains(QChar::Null))
    return false;
  static const QRegularExpression groupLine(
      QStringLiteral("^(?:\\[[^\\]\\r\\n]+\\])+$"));
  for (const auto &line : contents.split(QLatin1Char('\n'))) {
    const auto trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) ||
        trimmed.startsWith(QLatin1Char(';')))
      continue;
    if (trimmed.startsWith(QLatin1Char('['))) {
      if (!groupLine.match(trimmed).hasMatch())
        return false;
    } else if (!trimmed.contains(QLatin1Char('=')) ||
               trimmed.startsWith(QLatin1Char('='))) {
      return false;
    }
  }
  // AGENT-GUARD: QSettings reopens the path. Parse only this bounded private
  // snapshot so source replacement/growth cannot bypass the read cap.
  QTemporaryFile bounded;
  if (!bounded.open() || bounded.write(snapshot) != snapshot.size() ||
      !bounded.flush())
    return false;
  QSettings settings(bounded.fileName(), QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("Daemon"));
  QVariantMap next;
  for (const auto &key :
       {QStringLiteral("Autolock"), QStringLiteral("Timeout"),
        QStringLiteral("LockOnResume"), QStringLiteral("LockGrace")}) {
    if (settings.contains(key))
      next.insert(key, settings.value(key));
  }
  settings.endGroup();
  if (settings.status() != QSettings::NoError || next.isEmpty())
    return false;
  *output = next;
  return true;
}

ImportPlan planLegacyImport(const QVariantMap &legacy,
                            const QVariantMap &explicitNative) {
  ImportPlan plan;
  if (explicitNative.value(marker).metaType() == QMetaType::fromType<bool>() &&
      explicitNative.value(marker).toBool())
    return plan;
  QVariantMap converted;
  for (const auto &pair : {qMakePair(QStringLiteral("Autolock"),
                                     QStringLiteral("lock.automaticEnabled")),
                           qMakePair(QStringLiteral("LockOnResume"),
                                     QStringLiteral("lock.onResume"))}) {
    if (!legacy.contains(pair.first))
      continue;
    bool value = false;
    if (!legacyBool(legacy.value(pair.first), &value))
      return plan;
    converted.insert(pair.second, value);
  }
  if (legacy.contains(QStringLiteral("Timeout"))) {
    bool ok = false;
    const auto minutes =
        legacy.value(QStringLiteral("Timeout")).toString().toLongLong(&ok);
    if (!ok || minutes < 1 || minutes > 240)
      return plan;
    converted.insert(QStringLiteral("lock.idleTimeoutSeconds"), minutes * 60);
  }
  if (legacy.contains(QStringLiteral("LockGrace"))) {
    bool ok = false;
    const auto grace =
        legacy.value(QStringLiteral("LockGrace")).toString().toLongLong(&ok);
    if (!ok || grace < 0 || grace > 300)
      return plan;
    converted.insert(QStringLiteral("lock.graceSeconds"), grace);
  }
  if (converted.isEmpty())
    return plan;
  plan.sourceSupported = true;
  for (auto it = converted.cbegin(); it != converted.cend(); ++it) {
    if (!explicitNative.contains(it.key()))
      plan.values.insert(it.key(), it.value());
  }
  plan.values.insert(marker, true);
  return plan;
}
} // namespace QindaQt::Services::LockPreferences
