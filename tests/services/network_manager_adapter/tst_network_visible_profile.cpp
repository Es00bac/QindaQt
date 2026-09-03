// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_network_manager_types.h"
#include "libnm_network_manager_port_p.h"

#include <qindaqt/services/network_protocol/network_identity.h>

#include <QtCore/QProcessEnvironment>
#include <QtCore/QUuid>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtTest>

#include <algorithm>
#include <limits>
#include <memory>

using namespace QindaQt::Network;
using namespace QindaQt::Network::NetworkManager;
using QindaQt::Network::NetworkManager::TestSupport::SettingsMap;

namespace {

constexpr auto kService = "org.freedesktop.NetworkManager";
constexpr auto kManagerPath = "/org/freedesktop/NetworkManager";
constexpr auto kControlInterface = "org.qindaqt.tests.FakeNetworkManager";

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

bool secretPropertyName(const QString &name) {
  const QString lower = name.toLower();
  return lower == QLatin1String("psk") || lower == QLatin1String("pin") ||
         lower.startsWith(QLatin1String("wep-key")) ||
         lower.contains(QLatin1String("password")) ||
         lower == QLatin1String("private-key");
}

bool containsSecretProperty(const QVariant &value) {
  if (value.metaType() == QMetaType::fromType<QVariantMap>()) {
    const QVariantMap map = value.toMap();
    for (auto entry = map.cbegin(); entry != map.cend(); ++entry) {
      if (secretPropertyName(entry.key()) ||
          containsSecretProperty(entry.value())) {
        return true;
      }
    }
    return false;
  }
  if (value.metaType() == QMetaType::fromType<QVariantHash>()) {
    const QVariantHash map = value.toHash();
    for (auto entry = map.cbegin(); entry != map.cend(); ++entry) {
      if (secretPropertyName(entry.key()) ||
          containsSecretProperty(entry.value())) {
        return true;
      }
    }
    return false;
  }
  if (value.metaType() == QMetaType::fromType<QVariantList>()) {
    const QVariantList list = value.toList();
    return std::any_of(list.cbegin(), list.cend(), containsSecretProperty);
  }
  return false;
}

QString observedAccessPointId(const QSignalSpy &facts,
                              const SecuritySuite expected) {
  for (const QList<QVariant> &arguments : facts) {
    const Facts snapshot = arguments.constFirst().value<Facts>();
    const auto point = std::find_if(
        snapshot.accessPoints.cbegin(), snapshot.accessPoints.cend(),
        [expected](const AccessPointFact &candidate) {
          return candidate.security == expected;
        });
    if (point != snapshot.accessPoints.cend()) {
      return visibleAccessPointId(point->deviceInterface, point->bssid);
    }
  }
  return {};
}

} // namespace

class NetworkVisibleProfileTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void cleanupTestCase();
  void submitsExactSecretFreeSettings_data();
  void submitsExactSecretFreeSettings();
  void refusesUnsupportedBeforeAddAndActivate_data();
  void refusesUnsupportedBeforeAddAndActivate();

private:
  bool startFake(const QString &security);
  quint32 captureCount() const;
  SettingsMap capturedSettings() const;

  PrivateSystemBus m_bus;
  QProcess m_fake;
  std::unique_ptr<QDBusConnection> m_controlConnection;
  QByteArray m_originalSystemBus;
  bool m_systemBusWasSet = false;
};

void NetworkVisibleProfileTests::initTestCase() {
  TestSupport::registerSettingsMap();
  QVERIFY(m_bus.start());
  m_systemBusWasSet = qEnvironmentVariableIsSet("DBUS_SYSTEM_BUS_ADDRESS");
  m_originalSystemBus = qgetenv("DBUS_SYSTEM_BUS_ADDRESS");
  qputenv("DBUS_SYSTEM_BUS_ADDRESS", m_bus.address.toUtf8());
  const QString connectionName =
      QStringLiteral("network-visible-profile-%1")
          .arg(QUuid::createUuid().toString(QUuid::Id128));
  m_controlConnection = std::make_unique<QDBusConnection>(
      QDBusConnection::connectToBus(m_bus.address, connectionName));
  QVERIFY(m_controlConnection->isConnected());
}

void NetworkVisibleProfileTests::cleanup() {
  m_fake.terminate();
  if (!m_fake.waitForFinished(1'000)) {
    m_fake.kill();
    m_fake.waitForFinished();
  }
}

void NetworkVisibleProfileTests::cleanupTestCase() {
  cleanup();
  if (m_systemBusWasSet) {
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", m_originalSystemBus);
  } else {
    qunsetenv("DBUS_SYSTEM_BUS_ADDRESS");
  }
}

bool NetworkVisibleProfileTests::startFake(const QString &security) {
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), m_bus.address);
  environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
  m_fake.setProcessEnvironment(environment);
  m_fake.setProgram(QStringLiteral(QINDAQT_FAKE_NETWORK_MANAGER_PATH));
  m_fake.setArguments({security});
  m_fake.start();
  if (!m_fake.waitForStarted(5'000) || !m_fake.waitForReadyRead(5'000)) {
    return false;
  }
  return m_fake.readLine().trimmed() == QByteArrayLiteral("READY");
}

quint32 NetworkVisibleProfileTests::captureCount() const {
  QDBusInterface control(
      QString::fromLatin1(kService), QString::fromLatin1(kManagerPath),
      QString::fromLatin1(kControlInterface), *m_controlConnection);
  const QDBusReply<quint32> reply =
      control.call(QStringLiteral("CaptureCount"));
  return reply.isValid() ? reply.value() : std::numeric_limits<quint32>::max();
}

SettingsMap NetworkVisibleProfileTests::capturedSettings() const {
  QDBusInterface control(
      QString::fromLatin1(kService), QString::fromLatin1(kManagerPath),
      QString::fromLatin1(kControlInterface), *m_controlConnection);
  const QDBusReply<SettingsMap> reply =
      control.call(QStringLiteral("CapturedSettings"));
  return reply.isValid() ? reply.value() : SettingsMap{};
}

void NetworkVisibleProfileTests::submitsExactSecretFreeSettings_data() {
  QTest::addColumn<QString>("fakeSecurity");
  QTest::addColumn<SecuritySuite>("security");
  QTest::addColumn<QString>("keyManagement");
  QTest::newRow("open") << QStringLiteral("open") << SecuritySuite::Open
                        << QString{};
  QTest::newRow("wpa-psk") << QStringLiteral("wpa2")
                           << SecuritySuite::Wpa2Personal
                           << QStringLiteral("wpa-psk");
  QTest::newRow("sae") << QStringLiteral("wpa3") << SecuritySuite::Wpa3Personal
                       << QStringLiteral("sae");
}

void NetworkVisibleProfileTests::submitsExactSecretFreeSettings() {
  QFETCH(QString, fakeSecurity);
  QFETCH(SecuritySuite, security);
  QFETCH(QString, keyManagement);
  QVERIFY(startFake(fakeSecurity));

  LibnmNetworkManagerPort port;
  QSignalSpy facts(&port, &NetworkManagerPort::factsReady);
  QVERIFY(port.start());
  QTRY_VERIFY_WITH_TIMEOUT(!observedAccessPointId(facts, security).isEmpty(),
                           5'000);
  const QString accessPointId = observedAccessPointId(facts, security);
  Service::BackendOperationRequest request;
  request.kind = OperationKind::ConnectVisibleNetwork;
  request.identifier = accessPointId;
  port.submit(1, request);
  QTRY_COMPARE_WITH_TIMEOUT(captureCount(), quint32(1), 5'000);

  const SettingsMap settings = capturedSettings();
  const QStringList expectedSections =
      security == SecuritySuite::Open
          ? QStringList{QStringLiteral("802-11-wireless"),
                        QStringLiteral("connection")}
          : QStringList{QStringLiteral("802-11-wireless"),
                        QStringLiteral("802-11-wireless-security"),
                        QStringLiteral("connection")};
  QCOMPARE(settings.keys(), expectedSections);
  QVERIFY(!std::any_of(settings.cbegin(), settings.cend(),
                       [](const QVariantMap &section) {
                         return containsSecretProperty(section);
                       }));
  QCOMPARE(settings.value(QStringLiteral("802-11-wireless"))
               .value(QStringLiteral("ssid"))
               .toByteArray(),
           QByteArrayLiteral("Adapter network"));
  const QVariantMap wifiSecurity =
      settings.value(QStringLiteral("802-11-wireless-security"));
  if (security == SecuritySuite::Open) {
    QVERIFY(wifiSecurity.isEmpty());
  } else {
    QCOMPARE(wifiSecurity.value(QStringLiteral("key-mgmt")).toString(),
             keyManagement);
    QVERIFY(!wifiSecurity.contains(QStringLiteral("psk")));
    QCOMPARE(wifiSecurity.value(QStringLiteral("psk-flags")).toUInt(),
             quint32(NM_SETTING_SECRET_FLAG_AGENT_OWNED));
  }
  port.stop();
}

void NetworkVisibleProfileTests::refusesUnsupportedBeforeAddAndActivate_data() {
  QTest::addColumn<QString>("fakeSecurity");
  QTest::addColumn<SecuritySuite>("security");
  QTest::newRow("hidden") << QStringLiteral("hidden") << SecuritySuite::Open;
  QTest::newRow("wep") << QStringLiteral("wep") << SecuritySuite::Wep;
  QTest::newRow("enterprise")
      << QStringLiteral("enterprise") << SecuritySuite::Wpa2Enterprise;
}

void NetworkVisibleProfileTests::refusesUnsupportedBeforeAddAndActivate() {
  QFETCH(QString, fakeSecurity);
  QFETCH(SecuritySuite, security);
  QVERIFY(startFake(fakeSecurity));

  LibnmNetworkManagerPort port;
  QSignalSpy facts(&port, &NetworkManagerPort::factsReady);
  QSignalSpy finished(&port, &NetworkManagerPort::operationFinished);
  QVERIFY(port.start());
  QTRY_VERIFY_WITH_TIMEOUT(!observedAccessPointId(facts, security).isEmpty(),
                           5'000);
  Service::BackendOperationRequest request;
  request.kind = OperationKind::ConnectVisibleNetwork;
  request.identifier = observedAccessPointId(facts, security);
  port.submit(1, request);
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1'000);
  QCOMPARE(finished.constFirst()
               .at(1)
               .value<Service::BackendOperationOutcome>()
               .status,
           Service::BackendOperationStatus::Failed);
  QCOMPARE(captureCount(), quint32(0));
  port.stop();
}

QTEST_GUILESS_MAIN(NetworkVisibleProfileTests)
#include "tst_network_visible_profile.moc"
