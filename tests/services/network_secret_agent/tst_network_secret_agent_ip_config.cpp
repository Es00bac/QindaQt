// SPDX-License-Identifier: GPL-3.0-or-later

// Private-bus admission of the NetworkManager IP-configuration wire forms that
// Qt D-Bus leaves as QDBusArgument. These shapes only exist after a real
// demarshal, so they cannot be covered by the in-process controller tests.

#include "private_network_manager_bus.h"
#include "secret_agent_types_p.h"
#include "secret_request_admission_p.h"

#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusPendingReply>
#include <QtTest/QTest>

#include <algorithm>

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

qsizetype baseConnectionBytes() {
  using QindaQt::Network::SecretAgent::Private::utf8ByteCount;
  return utf8ByteCount(QStringLiteral("connection")) +
         utf8ByteCount(QStringLiteral("id")) +
         utf8ByteCount(QStringLiteral("Private Bus Wi-Fi")) +
         utf8ByteCount(QStringLiteral("uuid")) +
         utf8ByteCount(
             QStringLiteral("12345678-1234-4234-9234-123456789abc")) +
         utf8ByteCount(QStringLiteral("802-11-wireless-security")) +
         utf8ByteCount(QStringLiteral("key-mgmt")) +
         utf8ByteCount(QStringLiteral("wpa-psk"));
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

QDBusMessage vpnSecretsGetSecretsMessage(const QString &destination) {
  NmSettingsMap settings = connectionMap();
  settings.insert(QStringLiteral("vpn"),
                  {{QStringLiteral("secrets"),
                    QVariant::fromValue(
                        StringMap{{QStringLiteral("password"),
                                  QStringLiteral("vpn-secret-canary")}})}});
  QDBusMessage message = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface), QStringLiteral("GetSecrets"));
  message.setArguments(
      {QVariant::fromValue(settings),
       QDBusObjectPath(QString::fromLatin1(kKnownConnectionPath)),
       QStringLiteral("vpn"), QStringList{}, 0x1U});
  return message;
}

NmSettingsMap requestSettings(const QDBusMessage &message) {
  return message.arguments().value(0).value<NmSettingsMap>();
}

} // namespace

class NetworkSecretAgentIpConfigTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void countsUtf8BytesWithoutEncoding();
  void scrubsSharedSecretAliasesByDesign();
  void admitsBoundedWireFormsAndRefusesOversized_data();
  void admitsBoundedWireFormsAndRefusesOversized();
  void admitsVpnSecretShapeBeforeUnsupportedSettingRefusal();
  void preservesRequestDataAcrossAdmissionAndRefusal();

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

void NetworkSecretAgentIpConfigTest::countsUtf8BytesWithoutEncoding() {
  const char32_t smileCodePoint = 0x1f642;
  const QString emoji = QString::fromUcs4(&smileCodePoint, 1);
  QString validSurrogatePair;
  validSurrogatePair.append(QChar(0xd800));
  validSurrogatePair.append(QChar(0xdc00));
  const QList<QString> inputs{
      QStringLiteral("ASCII"), QString::fromUtf8("caf\xc3\xa9"), emoji,
      validSurrogatePair};
  for (const QString &input : inputs) {
    QCOMPARE(QindaQt::Network::SecretAgent::Private::utf8ByteCount(input),
             input.toUtf8().size());
  }
  // Qt 6.11.1 suppresses isolated surrogates in QString::toUtf8(); admission
  // deliberately counts the requested U+FFFD replacement size instead.
  QCOMPARE(QindaQt::Network::SecretAgent::Private::utf8ByteCount(
               QString(QChar(0xd800))),
           3);
  QCOMPARE(QindaQt::Network::SecretAgent::Private::utf8ByteCount(
               QString(QChar(0xdc00))),
           3);
}

void NetworkSecretAgentIpConfigTest::scrubsSharedSecretAliasesByDesign() {
  // Every retained alias below is a secret-bearing copy that must be scrubbed
  // together with the value passed to the wipe helper.
  QByteArray decoded("wire-secret-canary");
  decoded.detach();
  const QByteArray retained = decoded;
  const char *const bytes = retained.constData();
  const qsizetype byteCount = retained.size();
  QindaQt::Network::SecretAgent::Private::wipeByteArrayValue(decoded);
  QVERIFY(decoded.isEmpty());
  for (qsizetype index = 0; index < byteCount; ++index) {
    QCOMPARE(bytes[index], '\0');
    QCOMPARE(retained.at(index), '\0');
  }

  QString sectionKey = QString::fromUtf8("section-key-canary");
  QString propertyKey = QString::fromUtf8("property-key-canary");
  QString nestedKey = QString::fromUtf8("nested-key-canary");
  QString nestedValue = QString::fromUtf8("nested-value-canary");
  const QString sectionAlias = sectionKey;
  const QString propertyAlias = propertyKey;
  const QString nestedKeyAlias = nestedKey;
  const QString nestedValueAlias = nestedValue;
  QVariantMap nestedMap{{nestedKey, nestedValue}};
  QVariantMap section{{propertyKey, nestedMap}};
  NmSettingsMap settings{{sectionKey, section}};
  wipeSettingsMap(settings);
  QVERIFY(settings.isEmpty());
  const auto allZero = [](const QString &text) {
    return std::all_of(text.cbegin(), text.cend(),
                       [](const QChar character) { return character.isNull(); });
  };
  QVERIFY(allZero(sectionAlias));
  QVERIFY(allZero(propertyAlias));
  QVERIFY(allZero(nestedKeyAlias));
  QVERIFY(allZero(nestedValueAlias));
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
  row("vpn-secret-shaped-a-ss-admitted",
      withProperty(QStringLiteral("vpn"), QStringLiteral("secrets"),
                   QVariant::fromValue(StringMap{
                       {QStringLiteral("password"),
                        QStringLiteral("vpn-secret-canary")}})),
      true);

  // Exercise the exact aggregate byte boundary, including the base profile
  // plus the section/property names and per-record QVariant overhead.
  const qsizetype base = baseConnectionBytes();
  const qsizetype ipv6NameBytes =
      QindaQt::Network::SecretAgent::Private::utf8ByteCount(v6);
  const qsizetype dnsKeyBytes =
      QindaQt::Network::SecretAgent::Private::utf8ByteCount(dns);
  const qsizetype dnsPayloadAtLimit =
      65'536 - base - ipv6NameBytes - dnsKeyBytes - qsizetype(sizeof(QVariant));
  row("ipv6-dns-aay-aggregate-at-limit",
      withProperty(v6, dns,
                   QVariant::fromValue(QList<QByteArray>{
                       ipv6Bytes(dnsPayloadAtLimit)})),
      true);
  row("ipv6-dns-aay-aggregate-over-limit",
      withProperty(v6, dns,
                   QVariant::fromValue(QList<QByteArray>{
                       ipv6Bytes(dnsPayloadAtLimit + 1)})),
      false);

  const qsizetype addressesKeyBytes =
      QindaQt::Network::SecretAgent::Private::utf8ByteCount(addresses);
  const qsizetype recordPayloadAtLimit =
      65'536 - base - ipv6NameBytes - addressesKeyBytes -
      qsizetype(sizeof(QVariant)) - qsizetype(sizeof(quint32));
  const qsizetype addressBytes = recordPayloadAtLimit / 2;
  const qsizetype nextHopBytes = recordPayloadAtLimit - addressBytes;
  row("ipv6-address-a-record-aggregate-at-limit",
      withProperty(v6, addresses,
                   QVariant::fromValue(QList<Ipv6Address>{
                       Ipv6Address{ipv6Bytes(addressBytes), 64,
                                   ipv6Bytes(nextHopBytes)}})),
      true);
  row("ipv6-address-a-record-aggregate-over-limit",
      withProperty(v6, addresses,
                   QVariant::fromValue(QList<Ipv6Address>{
                       Ipv6Address{ipv6Bytes(addressBytes), 64,
                                   ipv6Bytes(nextHopBytes + 1)}})),
      false);

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

void NetworkSecretAgentIpConfigTest::
    admitsVpnSecretShapeBeforeUnsupportedSettingRefusal() {
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, m_bus.agent(), m_bus.presence());
  QCOMPARE(resident.start(), ResidentStartStatus::Started);

  auto pending = watch(m_bus.manager(),
                       vpnSecretsGetSecretsMessage(m_bus.agent().baseService()));
  QTRY_VERIFY(pending->isFinished());
  QCOMPARE(QDBusPendingReply<NmSettingsMap>(*pending).error().name(),
           QString::fromLatin1(kNoSecretsError));
  QVERIFY(prompt.requests.isEmpty());
  resident.stop();
}

void NetworkSecretAgentIpConfigTest::
    preservesRequestDataAcrossAdmissionAndRefusal() {
  const QString settingName = QStringLiteral("802-11-wireless-security");
  const QString ipSection = QStringLiteral("ipv4");
  const QString addressDataKey = QStringLiteral("address-data");
  NmSettingsMap ipSettings = connectionMap();
  ipSettings.insert(ipSection,
                    {{addressDataKey,
                      QVariant::fromValue(addressData(1))}});
  const NmSettingsMap expectedIpSettings = ipSettings;
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, m_bus.agent(), m_bus.presence());
  QCOMPARE(resident.start(), ResidentStartStatus::Started);

  const QDBusMessage admittedRequest = getSecretsMessage(
      m_bus.agent().baseService(),
      QString::fromLatin1(kKnownConnectionPath), 0x1U,
      {{ipSection, {{addressDataKey,
                     QVariant::fromValue(addressData(1))}}}});
  // Keep the caller's complete settings map and the agent's retained prompt
  // metadata alive across both scrub points: input admission and reply send.
  const NmSettingsMap retainedIpSettings = requestSettings(admittedRequest);
  QCOMPARE(retainedIpSettings, expectedIpSettings);
  auto admitted = watch(m_bus.manager(), admittedRequest);
  QTRY_COMPARE_WITH_TIMEOUT(prompt.requests.size(), 1, 2'000);
  prompt.submit(false);
  QTRY_VERIFY(admitted->isFinished());
  const QDBusPendingReply<NmSettingsMap> reply = *admitted;
  QVERIFY2(reply.isValid(), qPrintable(reply.error().name()));

  const NmSettingsMap afterAdmission = requestSettings(admittedRequest);
  QCOMPARE(afterAdmission.keys(), retainedIpSettings.keys());
  QCOMPARE(afterAdmission, retainedIpSettings);
  const auto addressRows =
      afterAdmission.value(ipSection)
          .value(addressDataKey)
          .value<QList<QVariantMap>>();
  QCOMPARE(addressRows, addressData(1));
  const QStringList expectedAddressKeys{QStringLiteral("address"),
                                        QStringLiteral("prefix")};
  QCOMPARE(addressRows.first().keys(), expectedAddressKeys);
  QCOMPARE(addressRows.first().value(QStringLiteral("address")).toString(),
           QStringLiteral("192.0.2.1"));
  QCOMPARE(addressRows.first().value(QStringLiteral("prefix")).toUInt(),
           24U);

  const PromptRequest &storedPrompt = prompt.requests.first();
  QCOMPARE(storedPrompt.connectionName, QStringLiteral("Private Bus Wi-Fi"));
  QCOMPARE(storedPrompt.connectionPath,
           QString::fromLatin1(kKnownConnectionPath));
  QCOMPARE(storedPrompt.settingName, settingName);
  QCOMPARE(storedPrompt.fields.size(), 1);
  QCOMPARE(storedPrompt.fields.first().key, QStringLiteral("psk"));
  QCOMPARE(storedPrompt.fields.first().label,
           QStringLiteral("Wi-Fi password"));
  const NmSettingsMap replySettings = reply.value();
  const QVariantMap replySection = replySettings.value(settingName);
  QCOMPARE(replySection.value(QStringLiteral("psk")).toString(),
           QStringLiteral("private-bus-canary"));
  QCOMPARE(replySection.value(QStringLiteral("psk-flags")).toUInt(), 2U);
  resident.stop();

  // VPN's secrets property is the same a{ss} form traversed by admission, but
  // VPN settings remain refused because they are outside this prompt contract.
  FakePrompt refusedPrompt;
  ResidentSecretAgent refusedAgent(refusedPrompt, m_bus.agent(),
                                   m_bus.presence());
  QCOMPARE(refusedAgent.start(), ResidentStartStatus::Started);
  const QDBusMessage refusedRequest =
      vpnSecretsGetSecretsMessage(m_bus.agent().baseService());
  const NmSettingsMap retainedVpnSettings = requestSettings(refusedRequest);
  auto refused = watch(m_bus.manager(), refusedRequest);
  QTRY_VERIFY(refused->isFinished());
  QCOMPARE(QDBusPendingReply<NmSettingsMap>(*refused).error().name(),
           QString::fromLatin1(kNoSecretsError));
  QVERIFY(refusedPrompt.requests.isEmpty());
  const NmSettingsMap afterRefusal = requestSettings(refusedRequest);
  QCOMPARE(afterRefusal.keys(), retainedVpnSettings.keys());
  QCOMPARE(afterRefusal, retainedVpnSettings);
  const StringMap vpnSecrets =
      afterRefusal.value(QStringLiteral("vpn"))
          .value(QStringLiteral("secrets"))
          .value<StringMap>();
  QCOMPARE(vpnSecrets.keys(), QStringList{QStringLiteral("password")});
  QCOMPARE(vpnSecrets.value(QStringLiteral("password")),
           QStringLiteral("vpn-secret-canary"));

  refusedAgent.stop();
}

QTEST_GUILESS_MAIN(NetworkSecretAgentIpConfigTest)
#include "tst_network_secret_agent_ip_config.moc"
