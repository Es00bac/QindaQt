// SPDX-License-Identifier: GPL-3.0-or-later
#include "qtcompositoroutputauthority.h"
#include "notification_output_test_support.h"

#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>
#include <QUuid>

using namespace QindaQt::Shell;

namespace {

class PrivateSessionBus final {
public:
    ~PrivateSessionBus() { stop(); }

    bool start(QString *error)
    {
        m_process.start(QStringLiteral("dbus-daemon"),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000)
            || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private dbus-daemon published no address");
            return false;
        }
        return true;
    }

    void stop() noexcept
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            (void)m_process.waitForFinished(1'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class FakeCompositor final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
    explicit FakeCompositor(QByteArray payload)
        : m_payload(std::move(payload))
    {
    }

    void publish(QByteArray payload)
    {
        m_payload = std::move(payload);
        Q_EMIT OutputsChanged();
    }

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray Outputs() const { return m_payload; }

Q_SIGNALS:
    Q_SCRIPTABLE void OutputsChanged();

private:
    QByteArray m_payload;
};

QString connectionName(const QString &role)
{
    return QStringLiteral("qindaqt-output-authority-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

bool registerCompositor(QDBusConnection &connection, FakeCompositor *object)
{
    return connection.registerObject(
               QStringLiteral("/org/qindaqt/Compositor"), object,
               QDBusConnection::ExportScriptableSlots
                   | QDBusConnection::ExportScriptableSignals)
        && connection.registerService(QStringLiteral("org.qindaqt.Compositor"));
}

} // namespace

class QtCompositorOutputAuthorityTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void followsInvalidationAndExactOwnerReplacement();
};

void QtCompositorOutputAuthorityTests::
    followsInvalidationAndExactOwnerReplacement()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));

    const QString serverAName = connectionName(QStringLiteral("server-a"));
    const QString serverBName = connectionName(QStringLiteral("server-b"));
    const QString clientName = connectionName(QStringLiteral("client"));
    auto serverA = QDBusConnection::connectToBus(bus.address(), serverAName);
    auto serverB = QDBusConnection::connectToBus(bus.address(), serverBName);
    auto client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(serverA.isConnected());
    QVERIFY(serverB.isConnected());
    QVERIFY(client.isConnected());

    FakeCompositor first(TestSupport::authorityPayload(
        1, {TestSupport::wireOutput(QStringLiteral("WL-0"), 1),
            TestSupport::wireOutput(QStringLiteral("WL-1"), 2)}));
    QVERIFY(registerCompositor(serverA, &first));

    QtCompositorOutputAuthority authority(client);
    QSignalSpy changes(&authority, &QtCompositorOutputAuthority::stateChanged);
    QVERIFY2(authority.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(authority.frame().has_value(), 5'000);
    QCOMPARE(authority.frame()->outputGeneration, quint64(1));
    QCOMPARE(authority.frame()->outputs.constFirst().outputId,
             QStringLiteral("WL-0"));
    QCOMPARE(authority.frame()->uniqueOwner, serverA.baseService());
    const qsizetype afterInitial = changes.size();

    first.publish(TestSupport::authorityPayload(
        2, {TestSupport::wireOutput(QStringLiteral("WL-1"), 1),
            TestSupport::wireOutput(QStringLiteral("WL-0"), 2)}));
    QTRY_VERIFY_WITH_TIMEOUT(
        authority.frame().has_value()
            && authority.frame()->outputGeneration == 2,
        5'000);
    QCOMPARE(authority.frame()->outputs.constFirst().outputId,
             QStringLiteral("WL-1"));
    // Invalidation withdraws generation 1 before the generation-2 reply is
    // published, producing distinct unavailable and accepted state edges.
    QVERIFY(changes.size() >= afterInitial + 2);

    QVERIFY(serverA.unregisterService(QStringLiteral("org.qindaqt.Compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(!authority.frame().has_value(), 5'000);
    serverA.unregisterObject(QStringLiteral("/org/qindaqt/Compositor"));

    FakeCompositor replacement(TestSupport::authorityPayload(
        1, {TestSupport::wireOutput(QStringLiteral("WL-2"), 1)}));
    QVERIFY(registerCompositor(serverB, &replacement));
    QTRY_VERIFY_WITH_TIMEOUT(authority.frame().has_value(), 5'000);
    QCOMPARE(authority.frame()->outputGeneration, quint64(1));
    QCOMPARE(authority.frame()->outputs.constFirst().outputId,
             QStringLiteral("WL-2"));
    QCOMPARE(authority.frame()->uniqueOwner, serverB.baseService());

    authority.stop();
    serverB.unregisterService(QStringLiteral("org.qindaqt.Compositor"));
    serverB.unregisterObject(QStringLiteral("/org/qindaqt/Compositor"));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverBName);
    QDBusConnection::disconnectFromBus(serverAName);
    client = QDBusConnection(QStringLiteral("released-client"));
    serverB = QDBusConnection(QStringLiteral("released-server-b"));
    serverA = QDBusConnection(QStringLiteral("released-server-a"));
    bus.stop();
}

QTEST_GUILESS_MAIN(QtCompositorOutputAuthorityTests)
#include "tst_qtcompositoroutputauthority.moc"
