// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsBluetooth::TestSupport {

class StubBluetoothSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool routeActive MEMBER routeActive NOTIFY viewChanged)
  Q_PROPERTY(bool discoveryLeaseHeld MEMBER discoveryLeaseHeld NOTIFY viewChanged)
  Q_PROPERTY(bool departureReleasePending MEMBER departureReleasePending NOTIFY viewChanged)
  Q_PROPERTY(bool pairingSupported MEMBER pairingSupported NOTIFY viewChanged)
  Q_PROPERTY(bool pairingReplyPending MEMBER pairingReplyPending NOTIFY viewChanged)
  Q_PROPERTY(QVariantMap pairingPrompt MEMBER pairingPrompt NOTIFY viewChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(QString serviceOwner MEMBER serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch MEMBER serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision MEMBER serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QVariantList adapters MEMBER adapters NOTIFY viewChanged)
  Q_PROPERTY(QVariantList devices MEMBER devices NOTIFY viewChanged)

public:
  bool loading = false;
  bool ready = true;
  bool degraded = false;
  bool unavailable = false;
  bool busy = false;
  bool routeActive = true;
  bool discoveryLeaseHeld = false;
  bool departureReleasePending = false;
  bool pairingSupported = true;
  bool pairingReplyPending = false;
  QVariantMap pairingPrompt;
  QString statusText = QStringLiteral("Bluetooth information is current.");
  QString errorText;
  QString operationStatusText;
  QString serviceOwner = QStringLiteral(":1.42");
  qulonglong serviceEpoch = 61;
  qulonglong serviceRevision = 5;
  QVariantList adapters;
  QVariantList devices;
  QString lastAdapter;
  QString lastDevice;
  bool lastBoolean = false;
  int powerRequests = 0;
  int discoveryRequests = 0;
  int connectionRequests = 0;
  int pairingRequests = 0;
  int forgetRequests = 0;
  int trustRequests = 0;
  int promptReplies = 0;
  int routeActiveChanges = 0;

  explicit StubBluetoothSettingsModel(QObject *parent = nullptr)
      : QObject(parent) {
    adapters = {QVariantMap{
        {QStringLiteral("id"), QStringLiteral("adapter-61-400")},
        {QStringLiteral("label"), QStringLiteral("Internal adapter")},
        {QStringLiteral("powered"), true},
        {QStringLiteral("discovering"), false},
        {QStringLiteral("discoveryLeaseOwned"), false},
        {QStringLiteral("powerAvailable"), true},
        {QStringLiteral("startDiscoveryAvailable"), true},
        {QStringLiteral("stopDiscoveryAvailable"), false},
        {QStringLiteral("accessibleDescription"),
         QStringLiteral("powered on, not discovering")},
    }};
    devices = {
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("device-61-700")},
            {QStringLiteral("adapterId"), QStringLiteral("adapter-61-400")},
            {QStringLiteral("label"), QStringLiteral("Headphones")},
            {QStringLiteral("classLabel"), QStringLiteral("Headphones")},
            {QStringLiteral("iconName"), QStringLiteral("audio-headphones")},
            {QStringLiteral("iconText"), QStringLiteral("HP")},
            {QStringLiteral("paired"), true},
            {QStringLiteral("connected"), true},
            {QStringLiteral("trusted"), true},
            {QStringLiteral("rssiKnown"), true},
            {QStringLiteral("rssi"), -42},
            {QStringLiteral("connectAvailable"), false},
            {QStringLiteral("disconnectAvailable"), true},
            {QStringLiteral("pairAvailable"), false},
            {QStringLiteral("forgetAvailable"), true},
            {QStringLiteral("trustAvailable"), true},
            {QStringLiteral("accessibleDescription"),
             QStringLiteral("paired, connected, signal -42 dBm")},
        },
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("device-61-701")},
            {QStringLiteral("adapterId"), QStringLiteral("adapter-61-400")},
            {QStringLiteral("label"), QStringLiteral("Keyboard")},
            {QStringLiteral("classLabel"), QStringLiteral("Keyboard")},
            {QStringLiteral("iconName"), QStringLiteral("input-keyboard")},
            {QStringLiteral("iconText"), QStringLiteral("KB")},
            {QStringLiteral("paired"), true},
            {QStringLiteral("connected"), false},
            {QStringLiteral("trusted"), false},
            {QStringLiteral("rssiKnown"), true},
            {QStringLiteral("rssi"), -58},
            {QStringLiteral("connectAvailable"), true},
            {QStringLiteral("disconnectAvailable"), false},
            {QStringLiteral("pairAvailable"), false},
            {QStringLiteral("forgetAvailable"), true},
            {QStringLiteral("trustAvailable"), true},
            {QStringLiteral("accessibleDescription"),
             QStringLiteral("paired, disconnected, signal -58 dBm")},
        },
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("device-61-702")},
            {QStringLiteral("adapterId"), QStringLiteral("adapter-61-400")},
            {QStringLiteral("label"), QStringLiteral("New phone")},
            {QStringLiteral("classLabel"), QStringLiteral("Phone")},
            {QStringLiteral("iconName"), QStringLiteral("phone")},
            {QStringLiteral("iconText"), QStringLiteral("PH")},
            {QStringLiteral("paired"), false},
            {QStringLiteral("connected"), false},
            {QStringLiteral("trusted"), false},
            {QStringLiteral("rssiKnown"), false},
            {QStringLiteral("rssi"), 0},
            {QStringLiteral("connectAvailable"), false},
            {QStringLiteral("disconnectAvailable"), false},
            {QStringLiteral("pairAvailable"), true},
            {QStringLiteral("forgetAvailable"), false},
            {QStringLiteral("trustAvailable"), false},
            {QStringLiteral("accessibleDescription"),
             QStringLiteral("not paired, disconnected")},
        },
    };
  }

  Q_INVOKABLE void setRouteActive(bool active) {
    routeActive = active;
    ++routeActiveChanges;
    Q_EMIT viewChanged();
  }
  Q_INVOKABLE bool requestAdapterPower(const QString &id, bool powered) {
    lastAdapter = id; lastBoolean = powered; ++powerRequests; return true;
  }
  Q_INVOKABLE bool requestDiscovery(const QString &id, bool enabled) {
    lastAdapter = id; lastBoolean = enabled; ++discoveryRequests; return true;
  }
  Q_INVOKABLE bool requestDeviceConnection(const QString &id, bool connected) {
    lastDevice = id; lastBoolean = connected; ++connectionRequests; return true;
  }
  Q_INVOKABLE bool requestPairing(const QString &id) {
    lastDevice = id; ++pairingRequests; return true;
  }
  Q_INVOKABLE bool requestForget(const QString &id) {
    lastDevice = id; ++forgetRequests; return true;
  }
  Q_INVOKABLE bool requestTrust(const QString &id, bool trusted) {
    lastDevice = id; lastBoolean = trusted; ++trustRequests; return true;
  }
  Q_INVOKABLE bool replyConfirmation(bool accepted) {
    lastBoolean = accepted; ++promptReplies; return true;
  }
  Q_INVOKABLE bool replyPasskey(const QString &) { ++promptReplies; return true; }
  Q_INVOKABLE bool replyPin(const QString &) { ++promptReplies; return true; }
  Q_INVOKABLE bool cancelPrompt() { ++promptReplies; return true; }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsBluetooth::TestSupport
