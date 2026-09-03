// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu::Registrar;

namespace
{

QDBusMessage call(const QDBusConnection &connection, const QString &method,
                  const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kRegistrarServiceName), QString::fromLatin1(kRegistrarObjectPath),
        QString::fromLatin1(kRegistrarInterface), method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        (void)finished.wait(5'000);
    }
    return watcher.reply();
}

} // namespace

class AppMenuRegistrarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registryIsBoundedAndGenerationFenced();
    void privateBusEnforcesCallerOwnershipAndOwnerLoss();
};

void AppMenuRegistrarTest::registryIsBoundedAndGenerationFenced()
{
    RegistrarRegistry registry;
    const QString owner = QStringLiteral(":1.42");
    const QDBusObjectPath path(QStringLiteral("/Menu"));
    const RegistrationResult first = registry.registerWindow(1, owner, path);
    QCOMPARE(first.outcome, RegistrationOutcome::Registered);
    QVERIFY(first.registration.has_value());
    const quint64 generation = first.registration->ownerGeneration;
    QCOMPARE(registry.retireOwner(owner, generation + 1), RemovalOutcome::StaleOwnerGeneration);
    QCOMPARE(registry.size(), 1);
    QCOMPARE(registry.unregisterWindow(1, QStringLiteral(":1.99")), RemovalOutcome::NotOwner);
    QCOMPARE(registry.registerWindow(1, QStringLiteral(":1.99"), path).outcome,
             RegistrationOutcome::OwnedByAnotherPeer);
    QCOMPARE(registry.registerWindow(
                 2, owner,
                 QDBusObjectPath(QStringLiteral("/")
                                 + QString(kMaxMenuObjectPathUtf8Bytes, u'a')))
                 .outcome,
             RegistrationOutcome::Invalid);
    QCOMPARE(registry.unregisterWindow(1, owner), RemovalOutcome::Removed);

    for (quint32 index = 1; index <= static_cast<quint32>(kMaxRegisteredWindows); ++index) {
        QCOMPARE(registry.registerWindow(index, owner, path).outcome,
                 RegistrationOutcome::Registered);
    }
    QCOMPARE(registry.registerWindow(static_cast<quint32>(kMaxRegisteredWindows + 1), owner, path)
                 .outcome,
             RegistrationOutcome::CapacityExceeded);
    QCOMPARE(registry.size(), kMaxRegisteredWindows);
}

void AppMenuRegistrarTest::privateBusEnforcesCallerOwnershipAndOwnerLoss()
{
    registerRegistrarWireTypes();
    const QString serverName = QStringLiteral("qindaqt-registrar-server");
    const QString clientAName = QStringLiteral("qindaqt-registrar-client-a");
    const QString clientBName = QStringLiteral("qindaqt-registrar-client-b");
    QDBusConnection server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, serverName);
    QDBusConnection clientA = QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientAName);
    QDBusConnection clientB = QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientBName);
    QVERIFY(server.isConnected());
    QVERIFY(clientA.isConnected());
    QVERIFY(clientB.isConnected());

    AppMenuRegistrar registrar(server);
    QCOMPARE(registrar.start(), RegistrarStartStatus::Started);
    AppMenuRegistrar collision(clientB);
    QCOMPARE(collision.start(), RegistrarStartStatus::NameAlreadyOwned);
    QVERIFY(!collision.isRunning());
    QSignalSpy unregistered(registrar.registry(), &RegistrarRegistry::windowUnregistered);

    const QDBusMessage registered = call(
        clientA, QStringLiteral("RegisterWindow"),
        {QVariant::fromValue(quint32{77}), QVariant::fromValue(QDBusObjectPath("/Menu"))});
    QCOMPARE(registered.type(), QDBusMessage::ReplyMessage);
    const std::optional<AppMenuRegistration> registration = registrar.registry()->registrationFor(77);
    QVERIFY(registration.has_value());
    QCOMPARE(registration->ownerUniqueName, clientA.baseService());

    const QDBusMessage lookup = call(clientB, QStringLiteral("GetMenuForWindow"),
                                     {QVariant::fromValue(quint32{77})});
    QDBusPendingReply<QString, QDBusObjectPath> lookupReply(lookup);
    QVERIFY(lookupReply.isValid());
    QCOMPARE(lookupReply.argumentAt<0>(), clientA.baseService());
    QCOMPARE(lookupReply.argumentAt<1>().path(), QStringLiteral("/Menu"));

    const QDBusMessage menusMessage = call(clientB, QStringLiteral("GetMenus"));
    QDBusPendingReply<RegistrarMenuList> menusReply(menusMessage);
    QVERIFY(menusReply.isValid());
    QCOMPARE(menusReply.value().size(), 1);
    QCOMPARE(menusReply.value().first().windowId, quint32{77});

    const QDBusMessage spoofedUnregister = call(
        clientB, QStringLiteral("UnregisterWindow"), {QVariant::fromValue(quint32{77})});
    QCOMPARE(spoofedUnregister.type(), QDBusMessage::ErrorMessage);
    QVERIFY(registrar.registry()->registrationFor(77).has_value());

    QDBusConnection::disconnectFromBus(clientAName);
    clientA = QDBusConnection(QStringLiteral("qindaqt-retired-client-a"));
    QTRY_VERIFY_WITH_TIMEOUT(!registrar.registry()->registrationFor(77).has_value(), 5'000);
    QCOMPARE(unregistered.size(), 1);

    registrar.stop();
    QVERIFY(!registrar.isRunning());
    QDBusConnection::disconnectFromBus(clientBName);
    QDBusConnection::disconnectFromBus(serverName);
}

QTEST_GUILESS_MAIN(AppMenuRegistrarTest)

#include "tst_appmenu_registrar.moc"
