// SPDX-License-Identifier: GPL-3.0-or-later

// Private-bus admission of the NetworkManager IP-configuration wire forms that
// Qt D-Bus leaves as QDBusArgument. These shapes only exist after a real
// demarshal, so they cannot be covered by the in-process controller tests.

#include "private_network_manager_bus.h"

#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusPendingReply>
#include <QtTest/QTest>

using namespace QindaQt::Network::SecretAgent;
using namespace QindaQt::Network::SecretAgent::Testing;

namespace {

// (ayuay) legacy IPv6 address and (ayuayu) legacy IPv6 route records.
struct Ipv6Address final {
  QByteArray address;
  quint32 prefix = 0;
  QByteArray gateway;
};
struct Ipv6Route final {
  QByteArray destination;
  quint32 prefix = 0;
  QByteArray nextHop;
  quint32 metric = 0;
};

QDBusArgument &operator<<(QDBusArgument &wire, const Ipv6Address &value) {
  wire.beginStructure();
  wire << value.address << value.prefix << value.gateway;
  wire.endStructure();
  return wire;
}
const QDBusArgument &operator>>(const QDBusArgument &wire, Ipv6Address &value) {
  wire.beginStructure();
  wire >> value.address >> value.prefix >> value.gateway;
  wire.endStructure();
  return wire;
}
QDBusArgument &operator<<(QDBusArgument &wire, const Ipv6Route &value) {
  wire.beginStructure();
  wire << value.destination << value.prefix << value.nextHop << value.metric;
  wire.endStructure();
  return wire;
}
const QDBusArgument &operator>>(const QDBusArgument &wire, Ipv6Route &value) {
  wire.beginStructure();
  wire >> value.destination >> value.prefix >> value.nextHop >> value.metric;
  wire.endStructure();
  return wire;
}

} // namespace

Q_DECLARE_METATYPE(Ipv6Address)
Q_DECLARE_METATYPE(Ipv6Route)

namespace {

using UIntRows = QList<QList<quint32>>;
using StringMap = QMap<QString, QString>;

QByteArray ipv6Bytes(const qsizetype size = 16) {
  return QByteArray(size, '\x20');
}

QList<QVariantMap> addressData(const int rows,
                               const QString &key = QStringLiteral("address")) {
  QList<QVariantMap> result;
  for (int index = 0; index < rows; ++index) {
    result.append({{key, QStringLiteral("192.0.2.1")},
                   {QStringLiteral("prefix"), quint32(24)}});
  }
  return result;
}

UIntRows uintRows(const int rows, const int columns = 3) {
  return UIntRows(rows, QList<quint32>(columns, quint32(24)));
}

QList<Ipv6Address> ipv6Addresses(const int rows, const qsizetype bytes = 16) {
  return QList<Ipv6Address>(rows,
                            Ipv6Address{ipv6Bytes(bytes), 64, ipv6Bytes()});
}

QList<Ipv6Route> ipv6Routes(const int rows, const qsizetype bytes = 16) {
  return QList<Ipv6Route>(rows,
                          Ipv6Route{ipv6Bytes(bytes), 64, ipv6Bytes(), 100});
}

StringMap stringMap(const int entries) {
  StringMap result;
  for (int index = 0; index < entries; ++index) {
    result.insert(QStringLiteral("key%1").arg(index), QStringLiteral("value"));
  }
  return result;
}

// Wraps `value` in `levels` single-entry a{sv} maps below the property.
QVariant nested(QVariant value, const int levels) {
  for (int level = 0; level < levels; ++level) {
    value = QVariantMap{{QStringLiteral("n"), value}};
  }
  return value;
}

NmSettingsMap withProperty(const QString &section, const QString &key,
                           const QVariant &value) {
  return {{section, {{key, value}}}};
}

// The exact non-secret IP shape NetworkManager 1.56 sends for a DHCP Wi-Fi
// profile (observed via Settings.Connection.GetSettings on qinda).
NmSettingsMap networkManagerDhcpProfile() {
  const auto emptyMaps = QVariant::fromValue(QList<QVariantMap>{});
  return {
      {QStringLiteral("ipv4"),
       {{QStringLiteral("address-data"), emptyMaps},
        {QStringLiteral("addresses"), QVariant::fromValue(UIntRows{})},
        {QStringLiteral("method"), QStringLiteral("auto")},
        {QStringLiteral("route-data"), emptyMaps},
        {QStringLiteral("routes"), QVariant::fromValue(UIntRows{})}}},
      {QStringLiteral("ipv6"),
       {{QStringLiteral("address-data"), emptyMaps},
        {QStringLiteral("addresses"),
         QVariant::fromValue(QList<Ipv6Address>{})},
        {QStringLiteral("method"), QStringLiteral("auto")},
        {QStringLiteral("route-data"), emptyMaps},
        {QStringLiteral("routes"), QVariant::fromValue(QList<Ipv6Route>{})}}}};
}

} // namespace

class NetworkSecretAgentIpConfigTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void admitsBoundedWireFormsAndRefusesOversized_data();
  void admitsBoundedWireFormsAndRefusesOversized();

private:
  PrivateNetworkManagerBus m_bus;
};

void NetworkSecretAgentIpConfigTest::initTestCase() {
  qDBusRegisterMetaType<QList<QVariantMap>>();
  qDBusRegisterMetaType<QList<quint32>>();
  qDBusRegisterMetaType<UIntRows>();
  qDBusRegisterMetaType<QList<QByteArray>>();
  qDBusRegisterMetaType<StringMap>();
  qDBusRegisterMetaType<QList<qint32>>();
  qDBusRegisterMetaType<Ipv6Address>();
  qDBusRegisterMetaType<QList<Ipv6Address>>();
  qDBusRegisterMetaType<Ipv6Route>();
  qDBusRegisterMetaType<QList<Ipv6Route>>();
  const QString failure = m_bus.start();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void NetworkSecretAgentIpConfigTest::cleanupTestCase() {
  const QString failure = m_bus.stop();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void NetworkSecretAgentIpConfigTest::
    admitsBoundedWireFormsAndRefusesOversized_data() {
  QTest::addColumn<NmSettingsMap>("extra");
  QTest::addColumn<bool>("prompts");
  const QString v4 = QStringLiteral("ipv4");
  const QString v6 = QStringLiteral("ipv6");
  const auto row = [](const char *name, const NmSettingsMap &extra,
                      const bool prompts) {
    QTest::newRow(name) << extra << prompts;
  };

  row("networkmanager-1.56-dhcp-profile", networkManagerDhcpProfile(), true);

  // aa{sv}
  const QString data = QStringLiteral("address-data");
  row("ipv4-address-data-1",
      withProperty(v4, data, QVariant::fromValue(addressData(1))), true);
  row("ipv4-address-data-256",
      withProperty(v4, data, QVariant::fromValue(addressData(256))), true);
  row("ipv4-address-data-257",
      withProperty(v4, data, QVariant::fromValue(addressData(257))), false);
  row("ipv4-address-data-oversized-key",
      withProperty(v4, data,
                   QVariant::fromValue(addressData(1, QString(513, u'k')))),
      false);

  // aau
  const QString addresses = QStringLiteral("addresses");
  row("ipv4-legacy-addresses-256",
      withProperty(v4, addresses, QVariant::fromValue(uintRows(256))), true);
  row("ipv4-legacy-addresses-257",
      withProperty(v4, addresses, QVariant::fromValue(uintRows(257))), false);
  row("ipv4-legacy-address-row-257-values",
      withProperty(v4, addresses, QVariant::fromValue(uintRows(1, 257))),
      false);

  // a(ayuay)
  row("ipv6-legacy-addresses-1",
      withProperty(v6, addresses, QVariant::fromValue(ipv6Addresses(1))), true);
  row("ipv6-legacy-addresses-256",
      withProperty(v6, addresses, QVariant::fromValue(ipv6Addresses(256))),
      true);
  row("ipv6-legacy-addresses-257",
      withProperty(v6, addresses, QVariant::fromValue(ipv6Addresses(257))),
      false);
  row("ipv6-legacy-address-oversized-bytes",
      withProperty(v6, addresses,
                   QVariant::fromValue(ipv6Addresses(1, 65'537))),
      false);

  // a(ayuayu)
  const QString routes = QStringLiteral("routes");
  row("ipv6-legacy-routes-1",
      withProperty(v6, routes, QVariant::fromValue(ipv6Routes(1))), true);
  row("ipv6-legacy-routes-256",
      withProperty(v6, routes, QVariant::fromValue(ipv6Routes(256))), true);
  row("ipv6-legacy-routes-257",
      withProperty(v6, routes, QVariant::fromValue(ipv6Routes(257))), false);
  row("ipv6-legacy-route-oversized-bytes",
      withProperty(v6, routes, QVariant::fromValue(ipv6Routes(1, 65'537))),
      false);

  // au and aay: configured DNS servers.
  const QString dns = QStringLiteral("dns");
  row("ipv4-dns-2",
      withProperty(v4, dns,
                   QVariant::fromValue(QList<quint32>(2, 0x08080808U))),
      true);
  row("ipv4-dns-257",
      withProperty(v4, dns, QVariant::fromValue(QList<quint32>(257, 1U))),
      false);
  row("ipv6-dns-2",
      withProperty(v6, dns,
                   QVariant::fromValue(QList<QByteArray>(2, ipv6Bytes()))),
      true);
  row("ipv6-dns-257",
      withProperty(v6, dns,
                   QVariant::fromValue(QList<QByteArray>(257, ipv6Bytes()))),
      false);

  // a{ss}: wired profiles always carry s390-options.
  const QString wired = QStringLiteral("802-3-ethernet");
  const QString s390 = QStringLiteral("s390-options");
  row("wired-s390-options-empty",
      withProperty(wired, s390, QVariant::fromValue(stringMap(0))), true);
  row("wired-s390-options-256",
      withProperty(wired, s390, QVariant::fromValue(stringMap(256))), true);
  row("wired-s390-options-257",
      withProperty(wired, s390, QVariant::fromValue(stringMap(257))), false);

  // Depth: a two-level wire form must end at or above the eighth level.
  row("aau-depth-6",
      withProperty(v4, QStringLiteral("nested"),
                   nested(QVariant::fromValue(uintRows(1)), 6)),
      true);
  row("aau-depth-7",
      withProperty(v4, QStringLiteral("nested"),
                   nested(QVariant::fromValue(uintRows(1)), 7)),
      false);

  // Unfamiliar wire signatures still fail closed.
  row("unknown-signature-ai",
      withProperty(v4, QStringLiteral("unknown"),
                   QVariant::fromValue(QList<qint32>{1})),
      false);
}

void NetworkSecretAgentIpConfigTest::
    admitsBoundedWireFormsAndRefusesOversized() {
  QFETCH(NmSettingsMap, extra);
  QFETCH(bool, prompts);
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, m_bus.agent(), m_bus.presence());
  QCOMPARE(resident.start(), ResidentStartStatus::Started);

  auto pending =
      watch(m_bus.manager(),
            getSecretsMessage(m_bus.agent().baseService(),
                              QString::fromLatin1(kKnownConnectionPath), 0x1U,
                              extra));
  if (prompts) {
    // Admission is synchronous; a refused row completes in milliseconds, so a
    // short wait keeps a regression from hitting the ctest timeout.
    QTRY_COMPARE_WITH_TIMEOUT(prompt.requests.size(), 1, 2'000);
    prompt.submit(false);
    QTRY_VERIFY(pending->isFinished());
    const QDBusPendingReply<NmSettingsMap> reply = *pending;
    QVERIFY2(reply.isValid(), qPrintable(reply.error().name()));
  } else {
    QTRY_VERIFY(pending->isFinished());
    QCOMPARE(QDBusPendingReply<NmSettingsMap>(*pending).error().name(),
             QString::fromLatin1(kNoSecretsError));
    QVERIFY(prompt.requests.isEmpty());
  }
  resident.stop();
}

QTEST_GUILESS_MAIN(NetworkSecretAgentIpConfigTest)
#include "tst_network_secret_agent_ip_config.moc"
