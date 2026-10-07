// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_network_manager_types.h"
#include "libnm_network_manager_port_p.h"
#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtTest>
#include <memory>
using namespace QindaQt::Network;
using namespace QindaQt::Network::NetworkManager;
namespace {
class PrivateSystemBus final {
public:
  bool start() {
    daemon.setProgram(QStringLiteral("dbus-daemon"));
    daemon.setArguments(
        {QStringLiteral("--session"), QStringLiteral("--nofork"),
         QStringLiteral("--nopidfile"), QStringLiteral("--print-address=1")});
    daemon.start();
    if (!daemon.waitForStarted() || !daemon.waitForReadyRead(5'000)) {
      return false;
    }
    address = QString::fromUtf8(daemon.readLine()).trimmed();
    return !address.isEmpty();
  }

  ~PrivateSystemBus() {
    daemon.terminate();
    if (!daemon.waitForFinished(1'000)) {
      daemon.kill();
      daemon.waitForFinished();
    }
  }

  QProcess daemon;
  QString address;
};

} // namespace
class HiddenProfileTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void cleanupTestCase();
  void actualSelectedDeviceAndRefusal_data();
  void actualSelectedDeviceAndRefusal();
  void exactProfiles_data();
  void exactProfiles();
  void refusalAndCapabilities();

private:
  PrivateSystemBus m_bus;
  QProcess m_fake;
  QByteArray m_original;
  bool m_wasSet = false;
  std::unique_ptr<QDBusConnection> m_control;
};
void HiddenProfileTests::exactProfiles_data() {
  QTest::addColumn<quint32>("suite");
  QTest::addColumn<QString>("key");
  QTest::newRow("wpa2") << quint32(SecuritySuite::Wpa2Personal)
                        << QStringLiteral("wpa-psk");
  QTest::newRow("wpa3") << quint32(SecuritySuite::Wpa3Personal)
                        << QStringLiteral("sae");
}
void HiddenProfileTests::exactProfiles() {
  QFETCH(quint32, suite);
  QFETCH(QString, key);
  const QByteArray ssid(" exact <name> ");
  std::unique_ptr<NMConnection, decltype(&g_object_unref)> profile(
      buildWifiProfile(ssid, SecuritySuite(suite), true), &g_object_unref);
  QVERIFY(profile);
  auto *wireless = nm_connection_get_setting_wireless(profile.get());
  QVERIFY(nm_setting_wireless_get_hidden(wireless));
  auto *bytes = nm_setting_wireless_get_ssid(wireless);
  gsize length = 0;
  const auto *raw = static_cast<const char *>(g_bytes_get_data(bytes, &length));
  QCOMPARE(QByteArray(raw, qsizetype(length)), ssid);
  auto *security = nm_connection_get_setting_wireless_security(profile.get());
  QCOMPARE(
      QString::fromUtf8(nm_setting_wireless_security_get_key_mgmt(security)),
      key);
  QCOMPARE(nm_setting_wireless_security_get_num_protos(security), 1u);
  QCOMPARE(QByteArray(nm_setting_wireless_security_get_proto(security, 0)),
           QByteArray("rsn"));
  QCOMPARE(nm_setting_wireless_security_get_num_pairwise(security), 1u);
  QCOMPARE(QByteArray(nm_setting_wireless_security_get_pairwise(security, 0)),
           QByteArray("ccmp"));
  QCOMPARE(nm_setting_wireless_security_get_num_groups(security), 1u);
  QCOMPARE(QByteArray(nm_setting_wireless_security_get_group(security, 0)),
           QByteArray("ccmp"));
  QCOMPARE(nm_setting_wireless_security_get_psk(security), nullptr);
  QCOMPARE(nm_setting_wireless_security_get_psk_flags(security),
           NM_SETTING_SECRET_FLAG_AGENT_OWNED);
  if (suite == quint32(SecuritySuite::Wpa3Personal)) {
    QCOMPARE(nm_setting_wireless_security_get_pmf(security),
             NM_SETTING_WIRELESS_SECURITY_PMF_REQUIRED);
  }
  GVariant *settings =
      nm_connection_to_dbus(profile.get(), NM_CONNECTION_SERIALIZE_ALL);
  QVERIFY(settings);
  auto *wifi = g_variant_lookup_value(
      settings, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, nullptr);
  QVERIFY(wifi);
  auto *psk =
      g_variant_lookup_value(wifi, NM_SETTING_WIRELESS_SECURITY_PSK, nullptr);
  QVERIFY(psk == nullptr);
  g_variant_unref(wifi);
  g_variant_unref(settings);
}
void HiddenProfileTests::refusalAndCapabilities() {
  for (auto security : {SecuritySuite::Open, SecuritySuite::Wep,
                        SecuritySuite::Wpa2Enterprise, SecuritySuite(99)}) {
    QVERIFY(buildWifiProfile("hidden", security, true) == nullptr);
  }
  QVERIFY(buildWifiProfile(QByteArray(33, 'x'), SecuritySuite::Wpa2Personal,
                           true) == nullptr);
  QVERIFY(!hiddenDeviceSecuritySupported(0));
  QVERIFY(!hiddenDeviceSecuritySupported(NM_WIFI_DEVICE_CAP_RSN));
  QVERIFY(!hiddenDeviceSecuritySupported(NM_WIFI_DEVICE_CAP_CIPHER_CCMP));
  QVERIFY(hiddenDeviceSecuritySupported(NM_WIFI_DEVICE_CAP_RSN |
                                        NM_WIFI_DEVICE_CAP_CIPHER_CCMP));
}
void HiddenProfileTests::initTestCase() {
  TestSupport::registerSettingsMap();
  QVERIFY(m_bus.start());
  m_wasSet = qEnvironmentVariableIsSet("DBUS_SYSTEM_BUS_ADDRESS");
  m_original = qgetenv("DBUS_SYSTEM_BUS_ADDRESS");
  qputenv("DBUS_SYSTEM_BUS_ADDRESS", m_bus.address.toUtf8());
  m_control = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
      m_bus.address, QStringLiteral("hidden-profile-%1")
                         .arg(QUuid::createUuid().toString(QUuid::Id128))));
  QVERIFY(m_control->isConnected());
}
void HiddenProfileTests::cleanup() {
  if (m_fake.state() != QProcess::NotRunning) {
    m_fake.terminate();
    if (!m_fake.waitForFinished(1000)) {
      m_fake.kill();
      m_fake.waitForFinished();
    }
  }
}
void HiddenProfileTests::cleanupTestCase() {
  cleanup();
  if (m_wasSet)
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", m_original);
  else
    qunsetenv("DBUS_SYSTEM_BUS_ADDRESS");
}
void HiddenProfileTests::actualSelectedDeviceAndRefusal_data() {
  QTest::addColumn<QString>("scenario");
  QTest::addColumn<QString>("device");
  QTest::addColumn<int>("captures");
  QTest::newRow("selected-hidden-no-AP")
      << QStringLiteral("hidden-rsn") << QStringLiteral("wlan0") << 1;
  QTest::newRow("unknown-device")
      << QStringLiteral("hidden-rsn") << QStringLiteral("wlan9") << 0;
  QTest::newRow("no-rsn") << QStringLiteral("hidden") << QStringLiteral("wlan0")
                          << 0;
  QTest::newRow("NM-authoritative-refusal")
      << QStringLiteral("hidden-refuse") << QStringLiteral("wlan0") << 1;
}
void HiddenProfileTests::actualSelectedDeviceAndRefusal() {
  QFETCH(QString, scenario);
  QFETCH(QString, device);
  QFETCH(int, captures);
  m_fake.setProgram(QStringLiteral(QINDAQT_FAKE_NETWORK_MANAGER_PATH));
  m_fake.setArguments({scenario});
  m_fake.start();
  QVERIFY(m_fake.waitForStarted(5000));
  QVERIFY(m_fake.waitForReadyRead(5000));
  QCOMPARE(m_fake.readLine().trimmed(), QByteArray("READY"));
  LibnmNetworkManagerPort port;
  QSignalSpy facts(&port, &NetworkManagerPort::factsReady);
  QSignalSpy outcomes(&port, &NetworkManagerPort::operationFinished);
  QVERIFY(port.start());
  QTRY_VERIFY(!facts.isEmpty());
  Service::BackendOperationRequest request;
  request.kind = OperationKind::ConnectKnownNetwork;
  request.hiddenJoin = ConnectHiddenIntent{
      device, QStringLiteral(" exact hidden "), SecuritySuite::Wpa3Personal};
  port.submit(1, request);
  QDBusInterface control(QStringLiteral("org.freedesktop.NetworkManager"),
                         QStringLiteral("/org/freedesktop/NetworkManager"),
                         QStringLiteral("org.qindaqt.tests.FakeNetworkManager"),
                         *m_control);
  QTRY_COMPARE(
      QDBusReply<quint32>(control.call(QStringLiteral("CaptureCount"))).value(),
      quint32(captures));
  if (captures) {
    QCOMPARE(QDBusReply<QString>(control.call(QStringLiteral("CapturedDevice")))
                 .value(),
             QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"));
    QCOMPARE(
        QDBusReply<QString>(control.call(QStringLiteral("CapturedSpecific")))
            .value(),
        QStringLiteral("/"));
    const QDBusReply<TestSupport::SettingsMap> settings =
        control.call(QStringLiteral("CapturedSettings"));
    QVERIFY(settings.isValid());
    QCOMPARE(settings.value()
                 .value(QStringLiteral("802-11-wireless"))
                 .value(QStringLiteral("ssid"))
                 .toByteArray(),
             QByteArray(" exact hidden "));
    QVERIFY(!settings.value()
                 .value(QStringLiteral("802-11-wireless-security"))
                 .contains(QStringLiteral("psk")));
  }
  if (!captures || scenario == QStringLiteral("hidden-refuse")) {
    QTRY_COMPARE(outcomes.size(), 1);
    const auto result =
        outcomes.first().at(1).value<Service::BackendOperationOutcome>();
    QCOMPARE(result.status, Service::BackendOperationStatus::Failed);
    QVERIFY(!result.reasonCode.contains(QStringLiteral("private")));
    QVERIFY(!result.diagnostic.contains(QStringLiteral("private")));
  }
  QCOMPARE(
      QDBusReply<quint32>(control.call(QStringLiteral("CaptureCount"))).value(),
      quint32(captures));
  port.stop();
}
QTEST_GUILESS_MAIN(HiddenProfileTests)
#include "tst_network_hidden_profile.moc"
