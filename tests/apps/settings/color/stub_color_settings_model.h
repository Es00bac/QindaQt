// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsColor::TestSupport {

class StubColorSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale MEMBER stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool retryAvailable MEMBER retryAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool compositorApplicationSupported MEMBER compositorApplicationSupported CONSTANT)
  Q_PROPERTY(bool catalogScanned MEMBER catalogScanned NOTIFY viewChanged)
  Q_PROPERTY(bool catalogComplete MEMBER catalogComplete NOTIFY viewChanged)
  Q_PROPERTY(bool unassignAvailable MEMBER unassignAvailable NOTIFY viewChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(QString importStatusText MEMBER importStatusText NOTIFY viewChanged)
  Q_PROPERTY(QString catalogSummaryText MEMBER catalogSummaryText NOTIFY viewChanged)
  Q_PROPERTY(QString displayEpoch MEMBER displayEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong displayRevision MEMBER displayRevision NOTIFY viewChanged)
  Q_PROPERTY(qulonglong settingsRevision MEMBER settingsRevision NOTIFY viewChanged)
  Q_PROPERTY(QString selectedOutputId MEMBER selectedOutputId NOTIFY viewChanged)
  Q_PROPERTY(QString selectedOutputName MEMBER selectedOutputName NOTIFY viewChanged)
  Q_PROPERTY(QVariantList outputRows MEMBER outputRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileRows MEMBER profileRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList inactiveAssignmentRows MEMBER inactiveAssignmentRows NOTIFY viewChanged)

public:
  bool loading = false;
  bool ready = true;
  bool degraded = false;
  bool unavailable = false;
  bool stale = false;
  bool busy = false;
  bool retryAvailable = true;
  bool compositorApplicationSupported = false;
  bool catalogScanned = true;
  bool catalogComplete = true;
  bool unassignAvailable = false;
  QString statusText = QStringLiteral("Authoritative display color state is shown.");
  QString errorText;
  QString operationStatusText;
  QString importStatusText;
  QString catalogSummaryText = QStringLiteral("1 color profiles discovered.");
  QString displayEpoch = QStringLiteral("dsep");
  qulonglong displayRevision = 3;
  qulonglong settingsRevision = 4;
  QString selectedOutputId = QStringLiteral("edid:dp1");
  QString selectedOutputName = QStringLiteral("Main Monitor");
  QVariantList outputRows;
  QVariantList profileRows;
  QVariantList inactiveAssignmentRows;
  int retryCount = 0;
  int assignCount = 0;
  int unassignCount = 0;
  int importCount = 0;
  bool routeActive = false;
  QString lastProfileId;
  QString lastOutputId;
  QUrl lastImportSource;

  explicit StubColorSettingsModel(QObject *parent = nullptr) : QObject(parent) {
    outputRows = {QVariantMap{
        {QStringLiteral("id"), QStringLiteral("edid:dp1")},
        {QStringLiteral("name"), QStringLiteral("Main Monitor")},
        {QStringLiteral("connectorName"), QStringLiteral("DP-1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), true},
        {QStringLiteral("selected"), true},
        {QStringLiteral("assigned"), false},
        {QStringLiteral("assignedProfileId"), QString{}},
        {QStringLiteral("assignmentText"), QStringLiteral("No profile assigned")},
        {QStringLiteral("accessibleDescription"),
         QStringLiteral("Main Monitor, DP-1, no color profile assigned")}}};
    profileRows = {QVariantMap{
        {QStringLiteral("id"), QStringLiteral("vendor-srgb")},
        {QStringLiteral("name"), QStringLiteral("vendor-srgb")},
        {QStringLiteral("description"), QString{}},
        {QStringLiteral("originText"), QStringLiteral("System profile")},
        {QStringLiteral("assigned"), false},
        {QStringLiteral("available"), true},
        {QStringLiteral("accessibleDescription"),
         QStringLiteral("Assign vendor-srgb, System profile")}}};
  }

  Q_INVOKABLE bool retry() { ++retryCount; return true; }
  Q_INVOKABLE void setRouteActive(bool active) { routeActive = active; }
  Q_INVOKABLE bool selectOutput(const QString &stableId) {
    lastOutputId = stableId;
    selectedOutputId = stableId;
    Q_EMIT viewChanged();
    return true;
  }
  Q_INVOKABLE bool assignProfile(const QString &profileId) {
    ++assignCount;
    lastProfileId = profileId;
    return true;
  }
  Q_INVOKABLE bool unassignSelected() { ++unassignCount; return true; }
  Q_INVOKABLE bool importProfile(const QUrl &source) {
    ++importCount;
    lastImportSource = source;
    return true;
  }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsColor::TestSupport
