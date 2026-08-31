// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsNetwork::TestSupport {

class StubNetworkSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale MEMBER stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool reloadAvailable MEMBER reloadAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool scanAvailable MEMBER scanAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool credentialEntrySupported MEMBER credentialEntrySupported
                 CONSTANT)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY
                 viewChanged)
  Q_PROPERTY(QString serviceOwner MEMBER serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch MEMBER serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision MEMBER serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QString connectivityText MEMBER connectivityText NOTIFY viewChanged)
  Q_PROPERTY(QString scanStatusText MEMBER scanStatusText NOTIFY viewChanged)
  Q_PROPERTY(QVariantList radios MEMBER radios NOTIFY viewChanged)
  Q_PROPERTY(QVariantList devices MEMBER devices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList accessPoints MEMBER accessPoints NOTIFY viewChanged)
  Q_PROPERTY(QVariantList knownNetworks MEMBER knownNetworks NOTIFY viewChanged)

public:
  bool loading = false;
  bool ready = true;
  bool degraded = false;
  bool unavailable = false;
  bool stale = false;
  bool busy = false;
  bool reloadAvailable = true;
  bool scanAvailable = true;
  bool credentialEntrySupported = false;
  QString statusText = QStringLiteral("Connected to the internet");
  QString errorText;
  QString operationStatusText;
  QString serviceOwner = QStringLiteral(":1.20");
  qulonglong serviceEpoch = 20;
  qulonglong serviceRevision = 1;
  QString connectivityText = QStringLiteral("Connected to the internet");
  QString scanStatusText = QStringLiteral("Scan results can be refreshed.");
  QVariantList radios;
  QVariantList devices;
  QVariantList accessPoints;
  QVariantList knownNetworks;
  int reloadCount = 0;
  int scanCount = 0;
  QString connectedNetwork;
  QString disconnectedDevice;

  explicit StubNetworkSettingsModel(QObject *parent = nullptr)
      : QObject(parent) {
    radios = {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Wi-Fi")},
                    {QStringLiteral("statusText"), QStringLiteral("On")}},
    };
    devices = {
        QVariantMap{
            {QStringLiteral("interfaceName"), QStringLiteral("wlan0")},
            {QStringLiteral("kindText"), QStringLiteral("Wi-Fi")},
            {QStringLiteral("stateText"), QStringLiteral("Connected")},
            {QStringLiteral("active"), true},
            {QStringLiteral("activeNetworkName"), QStringLiteral("Home")},
            {QStringLiteral("disconnectAvailable"), true},
        },
    };
    const QString id(64, u'b');
    knownNetworks = {
        QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("displayName"), QStringLiteral("Cafe")},
            {QStringLiteral("securityText"), QStringLiteral("WPA2 Personal")},
            {QStringLiteral("active"), false},
            {QStringLiteral("activeDeviceInterface"), QString()},
            {QStringLiteral("connectAvailable"), true},
            {QStringLiteral("mayRequireExternalCredentials"), true},
        },
    };
    accessPoints = {
        QVariantMap{
            {QStringLiteral("displayName"), QStringLiteral("Cafe")},
            {QStringLiteral("securityText"), QStringLiteral("WPA2 Personal")},
            {QStringLiteral("deviceInterface"), QStringLiteral("wlan0")},
            {QStringLiteral("frequencyMHz"), 5'180},
            {QStringLiteral("signalStrength"), 72},
            {QStringLiteral("saved"), true},
        },
    };
  }

  Q_INVOKABLE bool reload() {
    ++reloadCount;
    return true;
  }
  Q_INVOKABLE bool requestScan() {
    ++scanCount;
    return true;
  }
  Q_INVOKABLE bool connectKnownNetwork(const QString &id) {
    connectedNetwork = id;
    return true;
  }
  Q_INVOKABLE bool disconnectDevice(const QString &interfaceName) {
    disconnectedDevice = interfaceName;
    return true;
  }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsNetwork::TestSupport
