// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsPower::TestSupport {

class StubSessionActions final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool canLock MEMBER canLock NOTIFY availabilityChanged)
  Q_PROPERTY(bool canLogout MEMBER canLogout NOTIFY availabilityChanged)
  Q_PROPERTY(bool canSuspend MEMBER canSuspend NOTIFY availabilityChanged)
  Q_PROPERTY(bool canReboot MEMBER canReboot NOTIFY availabilityChanged)
  Q_PROPERTY(bool canPowerOff MEMBER canPowerOff NOTIFY availabilityChanged)
  Q_PROPERTY(bool pending MEMBER pending NOTIFY pendingChanged)
  Q_PROPERTY(QString feedback MEMBER feedback NOTIFY feedbackChanged)

public:
  bool canLock = true;
  bool canLogout = true;
  bool canSuspend = true;
  bool canReboot = true;
  bool canPowerOff = true;
  bool pending = false;
  QString feedback;
  QStringList requests;

  Q_INVOKABLE bool requestLock() { requests.append(QStringLiteral("lock")); return true; }
  Q_INVOKABLE bool requestLogout() { requests.append(QStringLiteral("logout")); return true; }
  Q_INVOKABLE bool requestSuspend() { requests.append(QStringLiteral("suspend")); return true; }
  Q_INVOKABLE bool requestReboot() { requests.append(QStringLiteral("reboot")); return true; }
  Q_INVOKABLE bool requestPowerOff() { requests.append(QStringLiteral("poweroff")); return true; }

Q_SIGNALS:
  void availabilityChanged();
  void pendingChanged();
  void feedbackChanged();
};

class StubScreenLockSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool automaticLock MEMBER automaticLock NOTIFY changed)
  Q_PROPERTY(int timeoutMinutes MEMBER timeoutMinutes NOTIFY changed)
  Q_PROPERTY(bool lockOnResume MEMBER lockOnResume NOTIFY changed)
  Q_PROPERTY(int lockGraceSeconds MEMBER lockGraceSeconds NOTIFY changed)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
public:
  using QObject::QObject;
  bool automaticLock = false;
  int timeoutMinutes = 5;
  bool lockOnResume = true;
  int lockGraceSeconds = 30;
  bool busy = false;
  QString statusText = QStringLiteral("Automatic screen locking is off.");
  QString errorText;
  int automaticLockCalls = 0;
  int timeoutCalls = 0;
  int lockOnResumeCalls = 0;
  int lockGraceCalls = 0;
  int retryCalls = 0;
  Q_INVOKABLE bool setAutomaticLock(bool enabled) {
    ++automaticLockCalls; automaticLock = enabled; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool setTimeoutMinutes(int minutes) {
    ++timeoutCalls; timeoutMinutes = minutes; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool setLockOnResume(bool enabled) {
    ++lockOnResumeCalls; lockOnResume = enabled; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool setLockGraceSeconds(int seconds) {
    ++lockGraceCalls; lockGraceSeconds = seconds; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool retryLiveApply() { ++retryCalls; return true; }
Q_SIGNALS:
  void changed();
};

class StubIdleDisplaySettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool hasConfirmed MEMBER hasConfirmed NOTIFY changed)
  Q_PROPERTY(bool available MEMBER available NOTIFY changed)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY changed)
  Q_PROPERTY(bool enabled MEMBER enabled NOTIFY changed)
  Q_PROPERTY(int minutes MEMBER minutes NOTIFY changed)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
  Q_PROPERTY(bool conflict MEMBER conflict NOTIFY changed)
  Q_PROPERTY(bool uncertain MEMBER uncertain NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
public:
  using QObject::QObject;
  bool hasConfirmed = true;
  bool available = true;
  bool canEdit = true;
  bool enabled = true;
  int minutes = 10;
  bool busy = false;
  bool conflict = false;
  bool uncertain = false;
  bool acceptEnabled = true;
  bool acceptMinutes = true;
  QString statusText = QStringLiteral("10 minutes of inactivity turns the display off.");
  QString errorText;
  int enabledCalls = 0;
  int minutesCalls = 0;
  int retryCalls = 0;
  Q_INVOKABLE bool setEnabled(bool value) {
    ++enabledCalls;
    if (!acceptEnabled) {
      errorText = QStringLiteral("Display-off save refused");
      Q_EMIT changed();
      return false;
    }
    enabled = value; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool setMinutes(int value) {
    ++minutesCalls;
    if (!acceptMinutes) {
      errorText = QStringLiteral("Display-off save refused");
      Q_EMIT changed();
      return false;
    }
    minutes = value; Q_EMIT changed(); return true;
  }
  Q_INVOKABLE bool retry() { ++retryCalls; return true; }
Q_SIGNALS:
  void changed();
};

class StubLidPowerButtonPolicy final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available MEMBER available NOTIFY availabilityChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY busyChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY errorChanged)
  Q_PROPERTY(quint32 lidAction MEMBER lidAction NOTIFY policyChanged)
  Q_PROPERTY(bool wakeWithExternalMonitor MEMBER wakeWithExternalMonitor
                 NOTIFY policyChanged)
  Q_PROPERTY(quint32 powerButtonAction MEMBER powerButtonAction NOTIFY
                 policyChanged)

public:
  using QObject::QObject;
  bool available = true;
  bool busy = false;
  QString errorText;
  quint32 lidAction = 1;
  bool wakeWithExternalMonitor = false;
  quint32 powerButtonAction = 32;
  int applyCalls = 0;
  QList<QVariantList> applyArguments;
  Q_INVOKABLE void applyPolicy(quint32 requestedLidAction,
                               bool requestedWakeWithExternalMonitor,
                               quint32 requestedPowerButtonAction) {
    ++applyCalls;
    applyArguments.append({QVariant::fromValue(requestedLidAction),
                           requestedWakeWithExternalMonitor,
                           QVariant::fromValue(requestedPowerButtonAction)});
    lidAction = requestedLidAction;
    wakeWithExternalMonitor = requestedWakeWithExternalMonitor;
    powerButtonAction = requestedPowerButtonAction;
    Q_EMIT policyChanged();
  }

Q_SIGNALS:
  void availabilityChanged();
  void busyChanged();
  void errorChanged();
  void policyChanged();
  void applyFinished(bool success, const QString &error);
};

// Checkpoint L row 5: injected AutomaticPowerProfilePolicyPort stand-in,
// mirroring StubLidPowerButtonPolicy's shape.
class StubAutomaticPowerProfilePolicy final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available MEMBER available NOTIFY availabilityChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY busyChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY errorChanged)
  Q_PROPERTY(QString acProfileId MEMBER acProfileId NOTIFY policyChanged)
  Q_PROPERTY(QString batteryProfileId MEMBER batteryProfileId NOTIFY policyChanged)
  Q_PROPERTY(QString lowBatteryProfileId MEMBER lowBatteryProfileId NOTIFY
                 policyChanged)

public:
  using QObject::QObject;
  bool available = true;
  bool busy = false;
  QString errorText;
  QString acProfileId;
  QString batteryProfileId;
  QString lowBatteryProfileId;
  int applyCalls = 0;
  QStringList lastArguments;
  Q_INVOKABLE void applyProfiles(const QString &requestedAcProfileId,
                                 const QString &requestedBatteryProfileId,
                                 const QString &requestedLowBatteryProfileId) {
    ++applyCalls;
    lastArguments = {requestedAcProfileId, requestedBatteryProfileId,
                     requestedLowBatteryProfileId};
    acProfileId = requestedAcProfileId;
    batteryProfileId = requestedBatteryProfileId;
    lowBatteryProfileId = requestedLowBatteryProfileId;
    Q_EMIT policyChanged();
  }

Q_SIGNALS:
  void availabilityChanged();
  void busyChanged();
  void errorChanged();
  void policyChanged();
  void applyFinished(bool success, const QString &error);
};

// External-display rows as the injected ADR-0150 route model presents them.
class StubExternalBrightness final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows MEMBER rows NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY viewChanged)

public:
  using QObject::QObject;
  QVariantList rows;
  bool busy = false;
  QString errorText;
  QString operationStatusText;
  int requestCount = 0;
  QString lastTarget;
  int lastNormalized = -1;

  static QVariantMap row(const QString &id, const QString &name, bool settable,
                         int normalized, const QString &reason) {
    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
      {QStringLiteral("known"), settable}, {QStringLiteral("normalized"), normalized},
      {QStringLiteral("settable"), settable}, {QStringLiteral("available"), settable},
      {QStringLiteral("reason"), reason},
      {QStringLiteral("accessibleDescription"), settable
           ? QStringLiteral("%1, brightness %2 percent").arg(name).arg(normalized / 100)
           : QStringLiteral("%1, read-only").arg(name)}};
  }
  void setRows(const QVariantList &value) {
    rows = value;
    Q_EMIT viewChanged();
  }
  void setAvailable(qsizetype index, bool available) {
    QVariantMap value = rows.at(index).toMap();
    value.insert(QStringLiteral("available"), available);
    rows[index] = value;
    Q_EMIT viewChanged();
  }

  Q_INVOKABLE bool requestBrightness(const QString &id, int normalized) {
    ++requestCount; lastTarget = id; lastNormalized = normalized; return true;
  }

Q_SIGNALS:
  void viewChanged();
};

class StubPowerSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *externalBrightness READ externalBrightness CONSTANT)
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale MEMBER stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool retryAvailable MEMBER retryAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool sessionActionsSupported MEMBER sessionActionsSupported CONSTANT)
  Q_PROPERTY(QObject *sessionActions READ sessionActions CONSTANT)
  Q_PROPERTY(bool lidPresent MEMBER lidPresent NOTIFY viewChanged)
  Q_PROPERTY(QObject *lidPolicy READ lidPolicy CONSTANT)
  Q_PROPERTY(QObject *profilePolicy READ profilePolicy CONSTANT)
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
  bool sessionActionsSupported = true;
  bool lidPresent = false;
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
  int internalBrightnessCount = 0;
  bool republishOnInternalRequest = false;
  QString lastTarget;
  int lastNormalized = -1;
  StubSessionActions sessionActionState;
  StubLidPowerButtonPolicy lidPolicyState;
  StubAutomaticPowerProfilePolicy profilePolicyState;

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

  [[nodiscard]] QObject *sessionActions() noexcept { return &sessionActionState; }
  [[nodiscard]] QObject *lidPolicy() noexcept { return &lidPolicyState; }
  [[nodiscard]] QObject *profilePolicy() noexcept { return &profilePolicyState; }
  StubExternalBrightness externalBrightnessState;
  [[nodiscard]] QObject *externalBrightness() noexcept {
    return &externalBrightnessState;
  }

  Q_INVOKABLE bool retry() { ++retryCount; return true; }
  Q_INVOKABLE bool requestProfile(const QString &id) {
    ++profileCount; lastTarget = id; return true;
  }
  Q_INVOKABLE bool requestKeyboardBrightness(const QString &id, int normalized) {
    ++brightnessCount; lastTarget = id; lastNormalized = normalized; return true;
  }
  Q_INVOKABLE bool requestInternalBrightness(const QString &id, int normalized) {
    ++internalBrightnessCount; lastTarget = id; lastNormalized = normalized;
    // Mirrors the real model, which republishes rows synchronously inside a
    // request; the page must keep the same control through that.
    if (republishOnInternalRequest) setInternalRow(true, normalized);
    return true;
  }

  void setInternalRow(bool available, int normalized = -1) {
    QVariantMap row = internalBrightnessRows.first().toMap();
    row.insert(QStringLiteral("available"), available);
    if (normalized >= 0) row.insert(QStringLiteral("normalized"), normalized);
    internalBrightnessRows = {row};
    Q_EMIT viewChanged();
  }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsPower::TestSupport
