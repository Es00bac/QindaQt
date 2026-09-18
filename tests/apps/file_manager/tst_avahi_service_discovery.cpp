// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/avahi_service_discovery.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <QVariant>

using namespace QindaQt::Apps::FileManager;

namespace {

// AGENT-NOTE: a QDBusPendingCallWatcher delivers `finished` through the event
// loop even for an already-completed call, so every assertion that waits on a
// browser or resolve reply is a QTRY_* one. That is the whole reason these
// rows pump events at all -- no bus is ever contacted.

// Test double proving the production browser's own state machine -- which
// types are browsed, how an announcement becomes one published identity, how
// duplicates on several interfaces collapse, and how a removal retires a row
// -- without a bus, an Avahi daemon, a network, or a resolver. Every D-Bus
// edge is answered with a canned reply, and the browser signals are delivered
// by calling the handlers directly, which is what the protected seams exist
// for.
class FakeAvahi final : public AvahiServiceDiscovery {
public:
  FakeAvahi() : AvahiServiceDiscovery(QDBusConnection(QString())) {}

  // A resolved ResolveService reply: iissssisqaayu.
  static QDBusPendingCall resolved(const QString &name, const QString &type,
                                   const QString &host, const QString &address,
                                   quint16 port) {
    const QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Avahi"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.Avahi.Server"),
        QStringLiteral("ResolveService"));
    QDBusMessage reply = request.createReply();
    reply.setArguments({0, 0, name, type, QStringLiteral("local"), host, 0, address,
                        QVariant::fromValue(port), QVariant::fromValue(QStringList{}),
                        static_cast<uint>(0)});
    return QDBusPendingCall::fromCompletedCall(reply);
  }

  static QDBusPendingCall failure(const QString &message) {
    return QDBusPendingCall::fromError(QDBusError(QDBusError::ServiceUnknown, message));
  }

  void deliverItemNew(const QString &name, const QString &type,
                      int interfaceIndex = 2) {
    handleItemNew(interfaceIndex, 0, name, type, QStringLiteral("local"));
  }
  void deliverItemRemove(const QString &name, const QString &type,
                         int interfaceIndex = 2) {
    handleItemRemove(interfaceIndex, 0, name, type, QStringLiteral("local"));
  }
  void deliverFailure(const QString &diagnostic) { handleBrowserFailure(diagnostic); }

  [[nodiscard]] bool busAvailable() const override { return m_busAvailable; }

  [[nodiscard]] QDBusPendingCall
  createBrowserCall(const QString &serviceType) const override {
    m_browsedTypes.append(serviceType);
    const QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Avahi"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.Avahi.Server"),
        QStringLiteral("ServiceBrowserNew"));
    return QDBusPendingCall::fromCompletedCall(request.createReply(QVariant::fromValue(
        QDBusObjectPath(QStringLiteral("/Client1/ServiceBrowser%1")
                            .arg(m_browsedTypes.size())))));
  }

  [[nodiscard]] QDBusPendingCall createResolveCall(int, int, const QString &name,
                                                   const QString &type,
                                                   const QString &) const override {
    m_resolvedNames.append(name);
    if (m_replies.isEmpty()) {
      return failure(QStringLiteral("no canned reply"));
    }
    Q_UNUSED(type);
    return m_replies.takeFirst();
  }

  void subscribeToBrowser(const QString &objectPath) override {
    m_subscribed.append(objectPath);
  }
  void releaseBrowser(const QString &objectPath) override {
    m_released.append(objectPath);
  }

  bool m_busAvailable = true;
  mutable QStringList m_browsedTypes;
  mutable QStringList m_resolvedNames;
  mutable QVector<QDBusPendingCall> m_replies;
  QStringList m_subscribed;
  QStringList m_released;
};

} // namespace

class TestAvahiServiceDiscovery final : public QObject {
  Q_OBJECT

private slots:
  void browsesOnlyWhatTheFileManagerCanOpen();
  void mapsServiceTypesToSchemes_data();
  void mapsServiceTypesToSchemes();
  void buildsCanonicalKeysAndHidesDefaultPorts();
  void resolvesAnAnnouncementIntoOneService();
  void collapsesOneMachineSeenOnSeveralInterfaces();
  void reportsAnUnreachableBusAndAFailedBrowser();
  void stopRetiresEveryRowAndReleasesEveryBrowser();
};

void TestAvahiServiceDiscovery::browsesOnlyWhatTheFileManagerCanOpen() {
  // AGENT-GUARD: NFS is deliberately absent -- the ADR-0137 allowlist cannot
  // open it, and a row that cannot be opened is worse than no row.
  const QStringList types = AvahiServiceDiscovery::browsedServiceTypes();
  QCOMPARE(types, QStringList({QStringLiteral("_sftp-ssh._tcp"),
                               QStringLiteral("_ssh._tcp"),
                               QStringLiteral("_smb._tcp")}));
  QVERIFY(!types.contains(QStringLiteral("_nfs._tcp")));

  FakeAvahi discovery;
  discovery.start();
  QCOMPARE(discovery.m_browsedTypes, types);
  QTRY_COMPARE(discovery.m_subscribed.size(), types.size());
  // Starting twice browses once.
  discovery.start();
  QCOMPARE(discovery.m_browsedTypes, types);
}

void TestAvahiServiceDiscovery::mapsServiceTypesToSchemes_data() {
  QTest::addColumn<QString>("type");
  QTest::addColumn<QString>("scheme");
  QTest::newRow("sftp-ssh") << QStringLiteral("_sftp-ssh._tcp")
                            << QStringLiteral("sftp");
  // KIO's sftp worker speaks SSH, so a plain _ssh._tcp is an sftp location.
  QTest::newRow("ssh") << QStringLiteral("_ssh._tcp") << QStringLiteral("sftp");
  QTest::newRow("smb") << QStringLiteral("_smb._tcp") << QStringLiteral("smb");
  QTest::newRow("nfs") << QStringLiteral("_nfs._tcp") << QString();
  QTest::newRow("http") << QStringLiteral("_http._tcp") << QString();
}

void TestAvahiServiceDiscovery::mapsServiceTypesToSchemes() {
  QFETCH(QString, type);
  QFETCH(QString, scheme);
  QCOMPARE(AvahiServiceDiscovery::schemeForServiceType(type), scheme);
}

void TestAvahiServiceDiscovery::buildsCanonicalKeysAndHidesDefaultPorts() {
  QCOMPARE(AvahiServiceDiscovery::keyFor(QStringLiteral("sftp"),
                                         QStringLiteral("Qinda-14.local."), 22),
           QStringLiteral("sftp://qinda-14.local"));
  QCOMPARE(AvahiServiceDiscovery::keyFor(QStringLiteral("sftp"),
                                         QStringLiteral("qinda.local"), 2222),
           QStringLiteral("sftp://qinda.local:2222"));
  QCOMPARE(AvahiServiceDiscovery::keyFor(QStringLiteral("smb"),
                                         QStringLiteral("nas.local"), 445),
           QStringLiteral("smb://nas.local"));
  // AGENT-GUARD: the identity is whatever the ADR-0137 allowlist accepts and
  // nothing else, so a hostile advertisement cannot introduce an address the
  // rest of the file manager would refuse later.
  QVERIFY(AvahiServiceDiscovery::keyFor(QStringLiteral("sftp"), QString(), 22).isEmpty());
  QVERIFY(AvahiServiceDiscovery::keyFor(QString(), QStringLiteral("q.local"), 22)
              .isEmpty());
  QVERIFY(AvahiServiceDiscovery::keyFor(QStringLiteral("sftp"),
                                        QStringLiteral("user@q.local"), 22)
              .isEmpty());
}

void TestAvahiServiceDiscovery::resolvesAnAnnouncementIntoOneService() {
  FakeAvahi discovery;
  QSignalSpy found(&discovery, &ServiceDiscovery::serviceFound);
  discovery.m_replies.append(FakeAvahi::resolved(
      QStringLiteral("qinda-14"), QStringLiteral("_sftp-ssh._tcp"),
      QStringLiteral("qinda-14.local"), QStringLiteral("100.67.154.111"), 22));
  discovery.start();
  discovery.deliverItemNew(QStringLiteral("qinda-14"),
                           QStringLiteral("_sftp-ssh._tcp"));

  QTRY_COMPARE(found.size(), 1);
  const auto service = found.constFirst().constFirst().value<DiscoveredService>();
  QCOMPARE(service.key, QStringLiteral("sftp://qinda-14.local"));
  QCOMPARE(service.name, QStringLiteral("qinda-14"));
  QCOMPARE(service.host, QStringLiteral("qinda-14.local"));
  QCOMPARE(service.scheme, QStringLiteral("sftp"));
  QCOMPARE(service.address, QStringLiteral("100.67.154.111"));
  // The default port is not carried, which is what keeps the address free of
  // a redundant ":22".
  QCOMPARE(service.port, 0);

  // An announcement of a type this module does not browse never resolves.
  discovery.deliverItemNew(QStringLiteral("printer"), QStringLiteral("_ipp._tcp"));
  QCoreApplication::processEvents();
  QCOMPARE(discovery.m_resolvedNames, QStringList{QStringLiteral("qinda-14")});
}

void TestAvahiServiceDiscovery::collapsesOneMachineSeenOnSeveralInterfaces() {
  FakeAvahi discovery;
  QSignalSpy found(&discovery, &ServiceDiscovery::serviceFound);
  QSignalSpy lost(&discovery, &ServiceDiscovery::serviceLost);
  for (int index = 0; index < 3; ++index) {
    discovery.m_replies.append(FakeAvahi::resolved(
        QStringLiteral("qinda-14"), QStringLiteral("_sftp-ssh._tcp"),
        QStringLiteral("qinda-14.local"), QStringLiteral("100.67.154.111"), 22));
  }
  discovery.start();
  // wlan0, tailscale0 and lo all announce the same machine.
  discovery.deliverItemNew(QStringLiteral("qinda-14"),
                           QStringLiteral("_sftp-ssh._tcp"), 2);
  discovery.deliverItemNew(QStringLiteral("qinda-14"),
                           QStringLiteral("_sftp-ssh._tcp"), 3);
  discovery.deliverItemNew(QStringLiteral("qinda-14"),
                           QStringLiteral("_sftp-ssh._tcp"), 1);
  QTRY_COMPARE(found.size(), 1);

  // The row survives until the last interface withdraws it.
  QCoreApplication::processEvents();
  discovery.deliverItemRemove(QStringLiteral("qinda-14"),
                              QStringLiteral("_sftp-ssh._tcp"), 2);
  discovery.deliverItemRemove(QStringLiteral("qinda-14"),
                              QStringLiteral("_sftp-ssh._tcp"), 3);
  QCOMPARE(lost.size(), 0);
  discovery.deliverItemRemove(QStringLiteral("qinda-14"),
                              QStringLiteral("_sftp-ssh._tcp"), 1);
  QCOMPARE(lost.size(), 1);
  QCOMPARE(lost.constFirst().constFirst().toString(),
           QStringLiteral("sftp://qinda-14.local"));
  // A removal for something never announced is a no-op.
  discovery.deliverItemRemove(QStringLiteral("ghost"), QStringLiteral("_ssh._tcp"));
  QCOMPARE(lost.size(), 1);
}

void TestAvahiServiceDiscovery::reportsAnUnreachableBusAndAFailedBrowser() {
  FakeAvahi unreachable;
  unreachable.m_busAvailable = false;
  QSignalSpy silent(&unreachable, &ServiceDiscovery::unavailable);
  unreachable.start();
  // AGENT-GUARD: silence is indistinguishable from "no servers here", so an
  // unreachable bus must say so.
  QCOMPARE(silent.size(), 1);
  QVERIFY(!silent.constFirst().constFirst().toString().isEmpty());
  QVERIFY(unreachable.m_browsedTypes.isEmpty());

  FakeAvahi failing;
  QSignalSpy reported(&failing, &ServiceDiscovery::unavailable);
  failing.start();
  failing.deliverFailure(QStringLiteral("browser died"));
  QCOMPARE(reported.size(), 2);
  QVERIFY(reported.constLast().constFirst().toString().contains(
      QStringLiteral("browser died")));

  // A resolve that fails publishes nothing rather than a half-built row.
  FakeAvahi unresolvable;
  QSignalSpy found(&unresolvable, &ServiceDiscovery::serviceFound);
  unresolvable.m_replies.append(FakeAvahi::failure(QStringLiteral("gone")));
  unresolvable.start();
  unresolvable.deliverItemNew(QStringLiteral("gone"), QStringLiteral("_ssh._tcp"));
  QTRY_COMPARE(unresolvable.m_resolvedNames.size(), 1);
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();
  QCOMPARE(found.size(), 0);
}

void TestAvahiServiceDiscovery::stopRetiresEveryRowAndReleasesEveryBrowser() {
  FakeAvahi discovery;
  QSignalSpy lost(&discovery, &ServiceDiscovery::serviceLost);
  QSignalSpy found(&discovery, &ServiceDiscovery::serviceFound);
  discovery.m_replies.append(FakeAvahi::resolved(
      QStringLiteral("qinda-14"), QStringLiteral("_ssh._tcp"),
      QStringLiteral("qinda-14.local"), QStringLiteral("10.0.0.1"), 22));
  discovery.start();
  discovery.deliverItemNew(QStringLiteral("qinda-14"), QStringLiteral("_ssh._tcp"));
  QTRY_COMPARE(found.size(), 1);

  discovery.stop();
  QCOMPARE(lost.size(), 1);
  QTRY_COMPARE(discovery.m_released.size(), discovery.m_subscribed.size());
  // Stopping twice releases once, and a late announcement after stop is
  // ignored.
  const qsizetype released = discovery.m_released.size();
  discovery.stop();
  QCOMPARE(discovery.m_released.size(), released);
  const qsizetype before = found.size();
  discovery.deliverItemNew(QStringLiteral("qinda-14"), QStringLiteral("_ssh._tcp"));
  QCoreApplication::processEvents();
  QCOMPARE(found.size(), before);
}

QTEST_GUILESS_MAIN(TestAvahiServiceDiscovery)
#include "tst_avahi_service_discovery.moc"
