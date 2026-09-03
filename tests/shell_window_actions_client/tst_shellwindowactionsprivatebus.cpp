// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <QDBusConnection>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt;

namespace {

const QString WindowId = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

class FakeCompositor final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.CompositorShell1")

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray ActivateWindow(const QString &windowId,
                                           const QString &epoch,
                                           const QString &revision)
    {
        ++calls;
        return Compositor::encodeShellWindowActionResult({
            Compositor::ShellWindowActionStatus::Admitted,
            Compositor::ShellWindowAction::Activate, windowId,
            {epoch, revision.toULongLong()}, {}, {}});
    }

public:
    int calls = 0;
};

} // namespace

class ShellWindowActionsPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void callsFakeCompositorThroughExactOwner();
};

void ShellWindowActionsPrivateBusTest::callsFakeCompositorThroughExactOwner()
{
    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    FakeCompositor compositor;
    QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Compositor")));
    QVERIFY(bus.registerObject(QStringLiteral("/org/qindaqt/CompositorShell"),
                               &compositor, QDBusConnection::ExportScriptableSlots));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(bus);
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    QTRY_VERIFY_WITH_TIMEOUT(client.available(), 500);
    QVERIFY(client.uniqueOwner().startsWith(u':'));
    const Compositor::ShellWindowGeneration generation{QStringLiteral("epoch-private"), 3};
    QSignalSpy finished(&client,
                        &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished);
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, generation));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 500);
    QCOMPARE(compositor.calls, 1);
    QVERIFY(client.lastResult()->serverResult->admitted());
    QCOMPARE(client.lastResult()->generation, generation);

    bus.unregisterObject(QStringLiteral("/org/qindaqt/CompositorShell"));
    QVERIFY(bus.unregisterService(QStringLiteral("org.qindaqt.Compositor")));
}

QTEST_GUILESS_MAIN(ShellWindowActionsPrivateBusTest)
#include "tst_shellwindowactionsprivatebus.moc"
