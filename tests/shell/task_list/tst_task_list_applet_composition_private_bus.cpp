// SPDX-License-Identifier: GPL-3.0-or-later

#include "tasklistappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/compositor/shellwindowactions.h"
#include "qindaqt/compositor/shellwindowidentity.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/task_list/task_list_source.h"
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

#include <optional>
#include <utility>

using namespace QindaQt;

namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/CompositorShell";
const QString WindowOne = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
const QString WindowTwo = QStringLiteral("11111111-2222-4333-8444-555555555555");

class FakeCompositor final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.CompositorShell1")

public:
    explicit FakeCompositor(QDBusConnection connection)
        : m_connection(std::move(connection))
    {
    }

    void setDelayReplies(bool delayed) { m_delayReplies = delayed; }

    bool replyPending()
    {
        if (!m_pendingMessage) {
            return false;
        }
        const bool sent = m_connection.send(m_pendingMessage->createReply(
            QVariantList{QVariant::fromValue(m_pendingReply)}));
        m_pendingMessage.reset();
        m_pendingReply.clear();
        return sent;
    }

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray ActiveWindowIdentity()
    {
        return Compositor::encodeShellWindowIdentitySnapshot({
            Compositor::ShellWindowIdentityStatus::Ok,
            QStringLiteral("identity-epoch"), 1,
            {QStringLiteral("identity-epoch"), 7},
            Compositor::ShellWindowIdentityFacts{
                WindowOne, static_cast<qint64>(QCoreApplication::applicationPid()),
                quint32{44}, std::nullopt, std::nullopt},
            {}, {}});
    }

    Q_SCRIPTABLE QByteArray ActivateWindow(const QString &windowId,
                                            const QString &epoch,
                                            const QString &revision)
    {
        return complete(Compositor::ShellWindowAction::Activate, windowId,
                        epoch, revision);
    }

    Q_SCRIPTABLE QByteArray MinimizeWindow(const QString &windowId,
                                            const QString &epoch,
                                            const QString &revision)
    {
        return complete(Compositor::ShellWindowAction::Minimize, windowId,
                        epoch, revision);
    }

    Q_SCRIPTABLE QByteArray UnminimizeWindow(const QString &windowId,
                                              const QString &epoch,
                                              const QString &revision)
    {
        return complete(Compositor::ShellWindowAction::Unminimize, windowId,
                        epoch, revision);
    }

    Q_SCRIPTABLE QByteArray CloseWindow(const QString &windowId,
                                         const QString &epoch,
                                         const QString &revision)
    {
        return complete(Compositor::ShellWindowAction::Close, windowId,
                        epoch, revision);
    }

    Q_SCRIPTABLE QByteArray RaiseWindow(const QString &windowId,
                                         const QString &epoch,
                                         const QString &revision)
    {
        return complete(Compositor::ShellWindowAction::Raise, windowId,
                        epoch, revision);
    }

public:
    QVector<Compositor::ShellWindowAction> actions;
    QStringList windowIds;
    QVector<Compositor::ShellWindowGeneration> generations;

private:
    QByteArray complete(Compositor::ShellWindowAction action,
                        const QString &windowId, const QString &epoch,
                        const QString &revision)
    {
        const Compositor::ShellWindowGeneration generation{
            epoch, revision.toULongLong()};
        actions.append(action);
        windowIds.append(windowId);
        generations.append(generation);
        const QByteArray result = Compositor::encodeShellWindowActionResult({
            Compositor::ShellWindowActionStatus::Admitted, action, windowId,
            generation, {}, {}});
        if (!m_delayReplies) {
            return result;
        }
        setDelayedReply(true);
        m_pendingMessage = message();
        m_pendingReply = result;
        return {};
    }

    QDBusConnection m_connection;
    std::optional<QDBusMessage> m_pendingMessage;
    QByteArray m_pendingReply;
    bool m_delayReplies = false;
};

struct CatalogFixture {
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

bool registerCompositor(QDBusConnection &connection, FakeCompositor &compositor)
{
    return connection.registerObject(
               QString::fromLatin1(ObjectPath), &compositor,
               QDBusConnection::ExportScriptableSlots)
        && connection.registerService(QString::fromLatin1(ServiceName));
}

void unregisterCompositor(QDBusConnection &connection)
{
    connection.unregisterService(QString::fromLatin1(ServiceName));
    connection.unregisterObject(QString::fromLatin1(ObjectPath));
}

} // namespace

class TaskListAppletCompositionPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanup();
    void routesWindowActionsAndClearsTruthOnOwnerLoss();
    void ownerLossFinishesAnInflightActionExactlyOnce();
};

void TaskListAppletCompositionPrivateBusTest::cleanup()
{
    for (const auto &name : {
             QStringLiteral("task-list-client"),
             QStringLiteral("task-list-service"),
             QStringLiteral("task-list-loss-client"),
             QStringLiteral("task-list-loss-service")}) {
        QDBusConnection::disconnectFromBus(name);
    }
}

void TaskListAppletCompositionPrivateBusTest::
routesWindowActionsAndClearsTruthOnOwnerLoss()
{
    auto serviceBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("task-list-service"));
    auto clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("task-list-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeCompositor compositor(serviceBus);
    QVERIFY(registerCompositor(serviceBus, compositor));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(clientBus);
    ShellWindowActionsClient::ShellWindowActionsClient actions(transport, 500);
    QVERIFY(actions.start());
    QTRY_COMPARE_WITH_TIMEOUT(actions.uniqueOwner(), serviceBus.baseService(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(actions.identityAvailable(), 500);

    CatalogFixture catalog;
    QString error;
    QVERIFY2(catalog.load(&error), qPrintable(error));
    ShellTaskList::TaskListSource source;
    TaskListOperationTest::FakeOperationAuthority authority;
    TaskListAppletTest::FakeTaskListOperationPort containerOperations;
    authority.owner = serviceBus.baseService();
    auto primary = TaskListTest::primary(WindowOne, QStringLiteral("app.one"),
                                         QStringLiteral("container-one"));
    auto member = TaskListTest::member(WindowTwo,
                                       QStringLiteral("container-one"));
    QVERIFY(source.publishGeneration({member, primary}).ok());
    authority.revision = source.revision();

    Shell::TaskListAppletComposition composition(
        catalog.catalog, catalog.policy, source, authority,
        containerOperations, actions);
    QVERIFY(composition.start(&error));
    auto *controller = composition.access();
    QVERIFY(controller != nullptr);
    Q_EMIT authority.stateChanged();
    QCOMPARE(controller->phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller->entryCount(), 1);

    QVERIFY(controller->activateTask(QStringLiteral("container-one"),
                                     source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QCOMPARE(compositor.actions.constLast(),
             Compositor::ShellWindowAction::Activate);
    QCOMPARE(compositor.windowIds.constLast(), WindowOne);

    QVERIFY(controller->minimizeTask(QStringLiteral("container-one"),
                                     source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QCOMPARE(compositor.actions.mid(1),
             (QVector{Compositor::ShellWindowAction::Minimize,
                      Compositor::ShellWindowAction::Minimize}));
    QCOMPARE(compositor.windowIds.mid(1), (QStringList{WindowTwo, WindowOne}));

    primary.minimized = true;
    QVERIFY(source.publishGeneration({member, primary}).ok());
    authority.revision = source.revision();
    Q_EMIT authority.stateChanged();
    QVERIFY(controller->minimizeTask(QStringLiteral("container-one"),
                                     source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QCOMPARE(compositor.actions.at(3),
             Compositor::ShellWindowAction::Unminimize);
    QCOMPARE(compositor.actions.at(4),
             Compositor::ShellWindowAction::Unminimize);

    QVERIFY(controller->closeTask(QStringLiteral("container-one"),
                                  source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QCOMPARE(compositor.actions.at(5), Compositor::ShellWindowAction::Close);
    QCOMPARE(compositor.actions.at(6), Compositor::ShellWindowAction::Close);
    QVERIFY(controller->raiseTask(QStringLiteral("container-one"),
                                  source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QCOMPARE(compositor.actions.constLast(), Compositor::ShellWindowAction::Raise);
    QCOMPARE(compositor.windowIds.constLast(), WindowOne);
    QCOMPARE(compositor.generations.constLast().epoch,
             QStringLiteral("identity-epoch"));
    QCOMPARE(compositor.generations.constLast().revision, quint64{7});
    QCOMPARE(containerOperations.calls.size(), 0);

    unregisterCompositor(serviceBus);
    QTRY_VERIFY_WITH_TIMEOUT(actions.uniqueOwner().isEmpty(), 500);
    authority.setUnavailable();
    QCOMPARE(source.status(), ShellTaskList::TaskListSourceStatus::Degraded);
    QCOMPARE(source.revision(), quint64{0});
    QCOMPARE(controller->entryCount(), 0);
    QCOMPARE(controller->phaseText(), QStringLiteral("degraded"));

    composition.stop();
    actions.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("task-list-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("task-list-service"));
}

void TaskListAppletCompositionPrivateBusTest::
ownerLossFinishesAnInflightActionExactlyOnce()
{
    auto serviceBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("task-list-loss-service"));
    auto clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("task-list-loss-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeCompositor compositor(serviceBus);
    compositor.setDelayReplies(true);
    QVERIFY(registerCompositor(serviceBus, compositor));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(clientBus);
    ShellWindowActionsClient::ShellWindowActionsClient actions(transport, 500);
    QVERIFY(actions.start());
    QTRY_COMPARE_WITH_TIMEOUT(actions.uniqueOwner(), serviceBus.baseService(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(actions.identityAvailable(), 500);
    CatalogFixture catalog;
    QString error;
    QVERIFY2(catalog.load(&error), qPrintable(error));
    ShellTaskList::TaskListSource source;
    TaskListOperationTest::FakeOperationAuthority authority;
    TaskListAppletTest::FakeTaskListOperationPort containerOperations;
    authority.owner = serviceBus.baseService();
    QVERIFY(source.publishGeneration(
                {TaskListTest::standalone(WindowOne, QStringLiteral("app.one"))})
                .ok());
    authority.revision = source.revision();
    Shell::TaskListAppletComposition composition(
        catalog.catalog, catalog.policy, source, authority,
        containerOperations, actions);
    Q_EMIT authority.stateChanged();
    auto *controller = composition.access();

    QVERIFY(controller->activateTask(WindowOne, source.revision()));
    QTRY_COMPARE_WITH_TIMEOUT(compositor.actions.size(), 1, 500);
    QCOMPARE(controller->pendingOperationCount(), 1);
    QVERIFY(serviceBus.unregisterService(QString::fromLatin1(ServiceName)));
    QTRY_COMPARE_WITH_TIMEOUT(controller->pendingOperationCount(), 0, 500);
    QVERIFY(controller->feedback().contains(QStringLiteral("owner changed")));
    const QString terminalFeedback = controller->feedback();
    QVERIFY(compositor.replyPending());
    QTest::qWait(50);
    QCOMPARE(controller->pendingOperationCount(), 0);
    QCOMPARE(controller->feedback(), terminalFeedback);

    authority.setUnavailable();
    QCOMPARE(source.revision(), quint64{0});
    serviceBus.unregisterObject(QString::fromLatin1(ObjectPath));
    composition.stop();
    actions.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("task-list-loss-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("task-list-loss-service"));
}

QTEST_GUILESS_MAIN(TaskListAppletCompositionPrivateBusTest)
#include "tst_task_list_applet_composition_private_bus.moc"
