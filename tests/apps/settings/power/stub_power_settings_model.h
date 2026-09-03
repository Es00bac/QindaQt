// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsPower::TestSupport {

class StubPowerSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale MEMBER stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool retryAvailable MEMBER retryAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool sessionActionsSupported MEMBER sessionActionsSupported CONSTANT)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch MEMBER serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision MEMBER serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QVariantList supplyRows MEMBER supplyRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileRows MEMBER profileRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileHoldRows MEMBER profileHoldRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList internalBrightnessRows MEMBER internalBrightnessRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList keyboardBrightnessRows MEMBER keyboardBrightnessRows NOTIFY viewChanged)

public:
  bool loading = false;
  bool ready = true;
  bool degraded = false;
  bool unavailable = false;
  bool stale = false;
  bool busy = false;
  bool retryAvailable = true;
  bool sessionActionsSupported = false;
  QString statusText = QStringLiteral("Authoritative power state is shown.");
  QString errorText;
  QString operationStatusText;
  qulonglong serviceEpoch = 41;
  qulonglong serviceRevision = 7;
  QVariantList supplyRows;
  QVariantList profileRows;
  QVariantList profileHoldRows;
  QVariantList internalBrightnessRows;
  QVariantList keyboardBrightnessRows;
  int retryCount = 0;
  int profileCount = 0;
  int brightnessCount = 0;
  QString lastTarget;
  int lastNormalized = -1;

  explicit StubPowerSettingsModel(QObject *parent = nullptr) : QObject(parent) {
    supplyRows = {QVariantMap{{QStringLiteral("id"), QStringLiteral("ac-adapter")},
        {QStringLiteral("name"), QStringLiteral("AC adapter")},
        {QStringLiteral("kindText"), QStringLiteral("Power adapter")},
        {QStringLiteral("stateText"), QStringLiteral("Connected")},
        {QStringLiteral("percentageText"), QString{}},
        {QStringLiteral("timeText"), QString{}},
        {QStringLiteral("warningText"), QStringLiteral("No warning")},
        {QStringLiteral("warningSeverity"), 0},
        {QStringLiteral("accessibleDescription"), QStringLiteral("AC adapter connected")}}};
    profileRows = {QVariantMap{{QStringLiteral("id"), QStringLiteral("balanced")},
        {QStringLiteral("label"), QStringLiteral("Balanced")},
        {QStringLiteral("active"), false}, {QStringLiteral("available"), true},
        {QStringLiteral("accessibleDescription"), QStringLiteral("Select Balanced")}}};
    profileHoldRows = {QVariantMap{{QStringLiteral("profileId"), QStringLiteral("performance")},
        {QStringLiteral("applicationName"), QStringLiteral("Renderer")},
        {QStringLiteral("reason"), QStringLiteral("Preview")},
        {QStringLiteral("accessibleDescription"), QStringLiteral("Renderer holds performance: Preview")}}};
    internalBrightnessRows = {brightnessRow(QStringLiteral("internal-41-1"),
        QStringLiteral("Laptop display"), false, 4'493, 421, 937)};
    keyboardBrightnessRows = {brightnessRow(QStringLiteral("keyboard-41-1"),
        QStringLiteral("Built-in keyboard"), true, 5'000, 5, 10)};
  }

  static QVariantMap brightnessRow(const QString &id, const QString &name,
                                   bool available, int normalized,
                                   int raw, int maximum) {
    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
      {QStringLiteral("known"), true}, {QStringLiteral("normalized"), normalized},
      {QStringLiteral("rawValue"), raw}, {QStringLiteral("rawMaximum"), maximum},
      {QStringLiteral("rawText"), QStringLiteral("Raw %1 of %2").arg(raw).arg(maximum)},
      {QStringLiteral("available"), available}, {QStringLiteral("canSet"), available},
      {QStringLiteral("reason"), QStringLiteral("Read-only in Power1 version 1")},
      {QStringLiteral("accessibleDescription"), QStringLiteral("%1 brightness").arg(name)}};
  }

  Q_INVOKABLE bool retry() { ++retryCount; return true; }
  Q_INVOKABLE bool requestProfile(const QString &id) {
    ++profileCount; lastTarget = id; return true;
  }
  Q_INVOKABLE bool requestKeyboardBrightness(const QString &id, int normalized) {
    ++brightnessCount; lastTarget = id; lastNormalized = normalized; return true;
  }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsPower::TestSupport
