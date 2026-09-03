// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalmenuappletcomposition.h"

#include "fake_dbusmenu_exporter.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/compositor/shellwindowidentity.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_client.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_transport.h>

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt;

namespace {

struct CatalogFixture final
{
    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;

    bool load(QString *error)
    {
        if (!catalog.loadDirectory(
                QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
            return false;
        }
        const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR
                           "/data/applet-policy/default.json"));
        if (!loaded.ok) {
            *error = loaded.error;
            return false;
        }
        policy = loaded.policy;
        return true;
    }
};

class FakeIdentityTransport final
    : public ShellWindowActionsClient::ShellWindowActionsTransport
{
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void request(quint64, const QString &, Compositor::ShellWindowAction,
                 const QString &,
                 const Compositor::ShellWindowGeneration &) override
    {
    }
    void requestIdentity(quint64 token, const QString &owner) override
    {
        m_request = qMakePair(token, owner);
    }

    void publishOwner(const QString &owner)
    {
        Q_EMIT serviceOwnerChanged(owner);
    }
    void publishIdentity(quint32 registrarWindowId)
    {
        QVERIFY(m_request.has_value());
        const Compositor::ShellWindowIdentitySnapshot snapshot{
            Compositor::ShellWindowIdentityStatus::Ok,
            QStringLiteral("test-identity-epoch"), 1,
            {QStringLiteral("action-epoch"), 3},
            Compositor::ShellWindowIdentityFacts{
                QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"),
                static_cast<qint64>(QCoreApplication::applicationPid()),
                registrarWindowId, std::nullopt, std::nullopt},
            {}, {}};
        Q_EMIT identityReplyReceived(
            m_request->first, m_request->second,
            Compositor::encodeShellWindowIdentitySnapshot(snapshot));
    }

private:
    std::optional<QPair<quint64, QString>> m_request;
};

QDBusMessage registrarCall(const QDBusConnection &connection,
                           const QString &method,
                           const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarServiceName),
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarInterface),
        method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        (void)finished.wait(5'000);
    }
    return watcher.reply();
}

} // namespace

class GlobalMenuRuntimeCompositionTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void composesAuthenticatedIdentityAndClearsOnOwnerLoss();
    void registrarCollisionPublishesDegraded();
};

void GlobalMenuRuntimeCompositionTest::
composesAuthenticatedIdentityAndClearsOnOwnerLoss()
{
    auto shellBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-runtime-shell"));
    auto providerBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-runtime-provider"));
    QVERIFY(shellBus.isConnected());
    QVERIFY(providerBus.isConnected());

    Shell::GlobalMenu::DbusMenu::registerDbusMenuWireTypes();
    Shell::GlobalMenu::Registrar::registerRegistrarWireTypes();
    Shell::GlobalMenu::Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Shell::GlobalMenu::Test::menuLayout());
    QVERIFY(providerBus.registerObject(
        QStringLiteral("/Menu"), &exporter,
        QDBusConnection::ExportScriptableSlots
            | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));

    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    transport.publishOwner(QStringLiteral(":1.900"));
    transport.publishIdentity(77);
    QTRY_VERIFY(client.identityAvailable());

    Shell::GlobalMenuAppletComposition composition(
        fixture.catalog, fixture.policy, shellBus, client);
    composition.start();
    QCOMPARE(static_cast<int>(composition.status()),
             static_cast<int>(Shell::GlobalMenuRuntimeStatus::Ready));
    QCOMPARE(registrarCall(
                 providerBus, QStringLiteral("RegisterWindow"),
                 {QVariant::fromValue(quint32{77}),
                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))})
                 .type(),
             QDBusMessage::ReplyMessage);
    QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 5'000);

    QSignalSpy eventSpy(
        &exporter, &Shell::GlobalMenu::Test::FakeDbusMenuExporter::eventObserved);
    composition.access()->activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(eventSpy.size(), 1, 5'000);

    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-runtime-provider"));
    providerBus = QDBusConnection(QStringLiteral("retired-provider"));
    QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
    QVERIFY(composition.access()->items().isEmpty());

    composition.stop();
    client.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-runtime-shell"));
}

void GlobalMenuRuntimeCompositionTest::registrarCollisionPublishesDegraded()
{
    auto squatterBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-squatter"));
    auto shellBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-collision-shell"));
    QVERIFY(squatterBus.isConnected());
    QVERIFY(shellBus.isConnected());
    QVERIFY(squatterBus.registerService(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarServiceName)));

    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    Shell::GlobalMenuAppletComposition composition(
        fixture.catalog, fixture.policy, shellBus, client);
    composition.start();

    QCOMPARE(static_cast<int>(composition.status()),
             static_cast<int>(Shell::GlobalMenuRuntimeStatus::Degraded));
    QCOMPARE(composition.reasonCode(), QStringLiteral("registrar-name-owned"));
    QCOMPARE(composition.access()->phase(), QStringLiteral("degraded"));
    QVERIFY(!composition.access()->available());

    composition.stop();
    client.stop();
    squatterBus.unregisterService(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarServiceName));
    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-collision-shell"));
    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-squatter"));
}

QTEST_GUILESS_MAIN(GlobalMenuRuntimeCompositionTest)
#include "tst_global_menu_runtime_composition.moc"
