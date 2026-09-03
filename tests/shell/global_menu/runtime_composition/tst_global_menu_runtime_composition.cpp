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
#include <QElapsedTimer>
#include <QProcess>
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
    void publishIdentity(qint64 processId, quint32 registrarWindowId,
                         quint64 revision = 1)
    {
        QVERIFY(m_request.has_value());
        const Compositor::ShellWindowIdentitySnapshot snapshot{
            Compositor::ShellWindowIdentityStatus::Ok,
            QStringLiteral("test-identity-epoch"), revision,
            {QStringLiteral("action-epoch"), 3},
            Compositor::ShellWindowIdentityFacts{
                QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"),
                processId,
                registrarWindowId, std::nullopt, std::nullopt},
            {}, {}};
        Q_EMIT identityReplyReceived(
            m_request->first, m_request->second,
            Compositor::encodeShellWindowIdentitySnapshot(snapshot));
    }

    void invalidateIdentity()
    {
        Q_EMIT identityInvalidated(QStringLiteral(":1.900"));
    }

private:
    std::optional<QPair<quint64, QString>> m_request;
};

class ScopedHostileProvider final
{
public:
    ~ScopedHostileProvider()
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(2'000)) {
            m_process.kill();
            (void)m_process.waitForFinished(2'000);
        }
    }

    bool start(QString *error)
    {
        m_process.setProgram(QStringLiteral(QINDAQT_HOSTILE_PROVIDER));
        m_process.setProcessChannelMode(QProcess::SeparateChannels);
        m_process.start();
        if (!m_process.waitForStarted(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        QElapsedTimer deadline;
        deadline.start();
        while (!m_process.canReadLine() && deadline.elapsed() < 5'000
               && m_process.state() != QProcess::NotRunning) {
            // The child registers through this process's registrar object.
            // Keep the private-bus dispatcher moving while awaiting its
            // readiness line or the blocking registration would deadlock.
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            (void)m_process.waitForReadyRead(20);
        }
        const QByteArray line = m_process.readLine().trimmed();
        if (line != QByteArrayLiteral("READY")) {
            *error = QStringLiteral("provider did not become ready: %1 %2")
                         .arg(QString::fromUtf8(line),
                              QString::fromUtf8(m_process.readAllStandardError()));
            return false;
        }
        return true;
    }

    [[nodiscard]] qint64 processId() const noexcept
    {
        return static_cast<qint64>(m_process.processId());
    }

private:
    QProcess m_process;
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

int providerEventCount(const QDBusConnection &connection)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.qindaqt.TestHostileGlobalMenu"),
        QStringLiteral("/Probe"),
        QStringLiteral("org.qindaqt.TestHostileGlobalMenuProbe1"),
        QStringLiteral("EventCount"));
    const QDBusMessage reply = connection.call(message, QDBus::Block, 5'000);
    return reply.type() == QDBusMessage::ReplyMessage
            && reply.arguments().size() == 1
        ? reply.arguments().constFirst().toInt()
        : -1;
}

} // namespace

class GlobalMenuRuntimeCompositionTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void composesAuthenticatedIdentityAndClearsOnOwnerLoss();
    void hostileProviderPidMismatchNeverPublishesOrActivates();
    void hostileProviderStaleIdentityNeverPublishesOrActivates();
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
    transport.publishIdentity(
        static_cast<qint64>(QCoreApplication::applicationPid()), 77);
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

void GlobalMenuRuntimeCompositionTest::
hostileProviderPidMismatchNeverPublishesOrActivates()
{
    // AGENT-NOTE: P2-01 requires a real second process so the private bus
    // daemon, rather than a fake credential seam, proves the PID mismatch.
    auto shellBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-hostile-pid-shell"));
    QVERIFY(shellBus.isConnected());
    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    transport.publishOwner(QStringLiteral(":1.900"));
    Shell::GlobalMenuAppletComposition composition(
        fixture.catalog, fixture.policy, shellBus, client);
    composition.start();

    ScopedHostileProvider provider;
    QVERIFY2(provider.start(&error), qPrintable(error));
    QVERIFY(provider.processId() > 0);
    QVERIFY(provider.processId()
            != static_cast<qint64>(QCoreApplication::applicationPid()));
    transport.publishIdentity(
        static_cast<qint64>(QCoreApplication::applicationPid()), 77);
    QTRY_VERIFY(client.identityAvailable());
    QTest::qWait(150);
    QVERIFY(!composition.access()->available());
    QVERIFY(composition.access()->items().isEmpty());
    composition.access()->activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(providerEventCount(shellBus), 0);

    composition.stop();
    client.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-hostile-pid-shell"));
}

void GlobalMenuRuntimeCompositionTest::
hostileProviderStaleIdentityNeverPublishesOrActivates()
{
    // AGENT-NOTE: P2-01 also requires stale compositor lineage to stay closed
    // when a foreign provider's later claim would otherwise match its PID.
    auto shellBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("global-menu-hostile-stale-shell"));
    QVERIFY(shellBus.isConnected());
    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    transport.publishOwner(QStringLiteral(":1.900"));
    Shell::GlobalMenuAppletComposition composition(
        fixture.catalog, fixture.policy, shellBus, client);
    composition.start();

    ScopedHostileProvider provider;
    QVERIFY2(provider.start(&error), qPrintable(error));
    QVERIFY(provider.processId() > 0);
    QVERIFY(provider.processId()
            != static_cast<qint64>(QCoreApplication::applicationPid()));
    transport.publishIdentity(
        static_cast<qint64>(QCoreApplication::applicationPid()), 77, 2);
    QTRY_VERIFY(client.identityAvailable());
    QVERIFY(!composition.access()->available());
    transport.invalidateIdentity();
    QVERIFY(!client.identityAvailable());
    transport.publishIdentity(provider.processId(), 77, 1);
    QVERIFY(!client.identityAvailable());
    QVERIFY(!composition.access()->available());
    QVERIFY(composition.access()->items().isEmpty());
    composition.access()->activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(providerEventCount(shellBus), 0);

    composition.stop();
    client.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("global-menu-hostile-stale-shell"));
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
