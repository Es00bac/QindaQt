// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/power_policy/powerdevil_import.h"

#include <KConfig>
#include <KConfigGroup>
#include <QFile>
#include <QRegularExpression>

namespace QindaQt::Services::PowerPolicy {
bool readPowerDevilPreferences(const QString &path, QVariantMap *entries) {
  if (!entries || path.isEmpty())
    return false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
      file.size() > 1024 * 1024)
    return false;
  const QString contents = QString::fromUtf8(file.readAll());
  static const QRegularExpression groupLine(
      QStringLiteral("^(?:\\[[^\\]\\r\\n]+\\])+$"));
  for (const QString &line : contents.split(QLatin1Char('\n'))) {
    const QString trimmed = line.trimmed();
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
  KConfig source(path, KConfig::SimpleConfig);
  QVariantMap parsed;
  const QStringList profiles{QStringLiteral("AC"), QStringLiteral("Battery"),
                             QStringLiteral("LowBattery")};
  KConfigGroup battery(&source, QStringLiteral("BatteryManagement"));
  if (battery.hasKey(QStringLiteral("BatteryCriticalAction")))
    parsed.insert(
        QStringLiteral("BatteryManagement/BatteryCriticalAction"),
        battery.readEntry(QStringLiteral("BatteryCriticalAction"), QString{}));
  const QStringList displayKeys{QStringLiteral("TurnOffDisplayWhenIdle"),
                                QStringLiteral("TurnOffDisplayIdleTimeoutSec"),
                                QStringLiteral("DimDisplayWhenIdle"),
                                QStringLiteral("DimDisplayIdleTimeoutSec"),
                                QStringLiteral("LockBeforeTurnOffDisplay")};
  const QStringList suspendKeys{
      QStringLiteral("AutoSuspendAction"),
      QStringLiteral("AutoSuspendIdleTimeoutSec"), QStringLiteral("LidAction"),
      QStringLiteral("InhibitLidActionWhenExternalMonitorPresent")};
  for (const auto &profileName : profiles) {
    KConfigGroup profile(&source, profileName);
    for (const auto &groupName :
         {QStringLiteral("Display"), QStringLiteral("SuspendAndShutdown"),
          QStringLiteral("Performance")}) {
      KConfigGroup group(&profile, groupName);
      QStringList keys;
      if (groupName == QLatin1String("Display"))
        keys = displayKeys;
      else if (groupName == QLatin1String("SuspendAndShutdown"))
        keys = suspendKeys;
      else
        keys = {QStringLiteral("PowerProfile")};
      for (const auto &key : keys) {
        if (group.hasKey(key))
          parsed.insert(profileName + QLatin1Char('/') + groupName +
                            QLatin1Char('/') + key,
                        group.readEntry(key, QString{}));
      }
    }
  }
  *entries = std::move(parsed);
  return true;
}

namespace {
constexpr auto ImportedKey = "power.migration.powerDevilImported";
const QStringList Profiles{QStringLiteral("AC"), QStringLiteral("Battery"),
                           QStringLiteral("LowBattery")};
QString profileKey(const QString &profile) {
  if (profile == QLatin1String("AC"))
    return QStringLiteral("ac");
  if (profile == QLatin1String("Battery"))
    return QStringLiteral("battery");
  return QStringLiteral("lowBattery");
}
QString actionName(quint32 action) {
  switch (action) {
  case 0:
    return QStringLiteral("none");
  case 1:
    return QStringLiteral("suspend");
  case 2:
    return QStringLiteral("hibernate");
  case 8:
    return QStringLiteral("power-off");
  case 32:
    return QStringLiteral("lock");
  case 64:
    return QStringLiteral("screen-off");
  default:
    return {};
  }
}
void addIfNotExplicit(ImportPlan &plan, const QVariantMap &native,
                      const QString &key, const QVariant &value) {
  if (!native.contains(key))
    plan.values.push_back({key, value});
}
bool boolValue(const QVariant &value, bool *out) {
  const QString normalized = value.toString().toLower();
  if (normalized == QLatin1String("true") || normalized == QLatin1String("1")) {
    *out = true;
    return true;
  }
  if (normalized == QLatin1String("false") ||
      normalized == QLatin1String("0")) {
    *out = false;
    return true;
  }
  return false;
}
} // namespace

ImportPlan planPowerDevilImport(const QVariantMap &legacyEntries,
                                const QVariantMap &explicitNativeValues) {
  ImportPlan plan;
  if (explicitNativeValues.value(QLatin1String(ImportedKey)).toBool())
    return plan;
  plan.sourceSupported = true;
  bool criticalOk = false;
  const quint32 critical =
      legacyEntries
          .value(QStringLiteral("BatteryManagement/BatteryCriticalAction"))
          .toString()
          .toUInt(&criticalOk);
  if (criticalOk) {
    const QString action = actionName(critical);
    if (action == QLatin1String("suspend") ||
        action == QLatin1String("hibernate") ||
        action == QLatin1String("power-off"))
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.critical.action"), action);
  }
  for (const QString &profile : Profiles) {
    const QString target = profileKey(profile);
    const QString prefix = profile + QLatin1Char('/');
    const QString rawLid =
        legacyEntries
            .value(prefix + QStringLiteral("SuspendAndShutdown/LidAction"))
            .toString();
    bool lidOk = false;
    const quint32 lid = rawLid.toUInt(&lidOk);
    const QString rawLidValue = lidOk ? actionName(lid) : QString{};
    const QString lidValue =
        rawLidValue == QLatin1String("power-off") ? QString{} : rawLidValue;
    if (!lidValue.isEmpty()) {
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.lid.%1.action").arg(target),
                       lidValue);
    }
    bool inhibit = false;
    const QVariant rawInhibit = legacyEntries.value(
        prefix +
        QStringLiteral(
            "SuspendAndShutdown/InhibitLidActionWhenExternalMonitorPresent"));
    if (rawInhibit.isValid() && boolValue(rawInhibit, &inhibit) &&
        !lidValue.isEmpty()) {
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.lid.%1.dockedAction").arg(target),
                       inhibit ? QStringLiteral("none") : lidValue);
    }
    bool enabled = false;
    const QVariant rawEnabled = legacyEntries.value(
        prefix + QStringLiteral("Display/TurnOffDisplayWhenIdle"));
    if (rawEnabled.isValid() && boolValue(rawEnabled, &enabled) &&
        !explicitNativeValues.contains(QStringLiteral("power.idleDisplayOffMinutes"))) {
      addIfNotExplicit(
          plan, explicitNativeValues,
          QStringLiteral("power.idle.%1.displayOffEnabled").arg(target),
          enabled);
    }
    bool dimEnabled = false;
    const QVariant rawDim = legacyEntries.value(
        prefix + QStringLiteral("Display/DimDisplayWhenIdle"));
    if (rawDim.isValid() && boolValue(rawDim, &dimEnabled))
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.idle.%1.dimEnabled").arg(target),
                       dimEnabled);
    bool lockBeforeOff = false;
    const QVariant rawLock = legacyEntries.value(
        prefix + QStringLiteral("Display/LockBeforeTurnOffDisplay"));
    if (rawLock.isValid() && boolValue(rawLock, &lockBeforeOff))
      addIfNotExplicit(
          plan, explicitNativeValues,
          QStringLiteral("power.idle.%1.lockBeforeDisplayOff").arg(target),
          lockBeforeOff);
    bool dimTimeoutOk = false;
    const int dimSeconds =
        legacyEntries
            .value(prefix + QStringLiteral("Display/DimDisplayIdleTimeoutSec"))
            .toString()
            .toInt(&dimTimeoutOk);
    if (dimTimeoutOk && dimSeconds >= 0 && dimSeconds <= 14400 &&
        dimSeconds % 60 == 0)
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.idle.%1.dimMinutes").arg(target),
                       dimSeconds / 60);
    bool suspendOk = false;
    const quint32 suspendAction =
        legacyEntries
            .value(prefix +
                   QStringLiteral("SuspendAndShutdown/AutoSuspendAction"))
            .toString()
            .toUInt(&suspendOk);
    if (suspendOk &&
        (suspendAction == 0 || suspendAction == 1 || suspendAction == 2))
      addIfNotExplicit(
          plan, explicitNativeValues,
          QStringLiteral("power.idle.%1.suspendAction").arg(target),
          actionName(suspendAction));
    bool suspendTimeoutOk = false;
    const int suspendSeconds =
        legacyEntries
            .value(prefix + QStringLiteral(
                                "SuspendAndShutdown/AutoSuspendIdleTimeoutSec"))
            .toString()
            .toInt(&suspendTimeoutOk);
    if (suspendTimeoutOk && suspendSeconds >= 0 && suspendSeconds <= 14400 &&
        suspendSeconds % 60 == 0)
      addIfNotExplicit(
          plan, explicitNativeValues,
          QStringLiteral("power.idle.%1.suspendMinutes").arg(target),
          suspendSeconds / 60);
    const QVariant legacyProfile = legacyEntries.value(
        prefix + QStringLiteral("Performance/PowerProfile"));
    const QString profileId = legacyProfile.toString();
    if (legacyProfile.isValid() && (profileId == QLatin1String("power-saver") ||
                                    profileId == QLatin1String("balanced") ||
                                    profileId == QLatin1String("performance")))
      addIfNotExplicit(plan, explicitNativeValues,
                       QStringLiteral("power.profile.%1").arg(target),
                       profileId);
    bool timeoutOk = false;
    const int seconds =
        legacyEntries
            .value(prefix +
                   QStringLiteral("Display/TurnOffDisplayIdleTimeoutSec"))
            .toString()
            .toInt(&timeoutOk);
    const QString profileTimeoutKey =
        QStringLiteral("power.idle.%1.displayOffMinutes").arg(target);
    const QString profileEnabledKey =
        QStringLiteral("power.idle.%1.displayOffEnabled").arg(target);
    const bool explicitProfileIdle =
        explicitNativeValues.contains(profileEnabledKey) ||
        explicitNativeValues.contains(profileTimeoutKey);
    if (explicitNativeValues.contains(
            QStringLiteral("power.idleDisplayOffMinutes")) &&
        !explicitProfileIdle) {
      const int savedMinutes =
          explicitNativeValues.value(QStringLiteral("power.idleDisplayOffMinutes"))
              .toInt();
      const bool savedEnabled = savedMinutes > 0;
      addIfNotExplicit(plan, explicitNativeValues, profileEnabledKey,
                       savedEnabled);
      addIfNotExplicit(plan, explicitNativeValues, profileTimeoutKey,
                       savedEnabled ? savedMinutes : 0);
    } else {
      if (timeoutOk && seconds >= 0 && seconds <= 14400 &&
          seconds % 60 == 0)
        addIfNotExplicit(plan, explicitNativeValues, profileTimeoutKey,
                         seconds / 60);
      if (!rawEnabled.isValid() && timeoutOk && seconds >= 0 &&
          seconds <= 14400 && seconds % 60 == 0)
        addIfNotExplicit(plan, explicitNativeValues, profileEnabledKey, true);
    }
  }
  plan.values.push_back({QString::fromLatin1(ImportedKey), true});
  return plan;
}

} // namespace QindaQt::Services::PowerPolicy
