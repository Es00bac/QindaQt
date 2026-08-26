// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_visibility_client/qt_compositor_visibility_transport.h"

#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

using namespace QindaQt::ShellVisibilityClient;

namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/Compositor";

class PrivateSessionBus final {
public:
    ~PrivateSessionBus()
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.terminate();
            if (!m_process.waitForFinished(1'000)) {
                m_process.kill();
                m_process.waitForFinished(1'000);
            }
        }
    }

    bool start(QString *error)
    {
        m_process.start(QStringLiteral("dbus-daemon"),
                        {QStringLiteral("--session"),
                         QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private dbus-daemon did not publish an address");
            return false;
        }
        return true;
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
    explicit FakeCompositor(QByteArray payload, QObject *parent = nullptr)
        : QObject(parent)
        , m_payload(std::move(payload))
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray ShellVisibilitySnapshot() const { return m_payload; }

Q_SIGNALS:
    Q_SCRIPTABLE void ShellVisibilityChanged();

private:
    QByteArray m_payload;
};

QString connectionName(const QString &role)
{
    return QStringLiteral("qindaqt-visibility-transport-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

bool registerCompositor(QDBusConnection &connection, FakeCompositor *object)
{
    return connection.registerObject(
               QString::fromLatin1(ObjectPath), object,
               QDBusConnection::ExportScriptableSlots |
                   QDBusConnection::ExportScriptableSignals) &&
        connection.registerService(QString::fromLatin1(ServiceName));
}

} // namespace

class QtCompositorVisibilityTransportTests final : public QObject {
    Q_OBJECT

private slots:
    void bindsReadsAndSignalsToTheExactUniqueOwner();
};

void QtCompositorVisibilityTransportTests::bindsReadsAndSignalsToTheExactUniqueOwner()
{
    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));

    const QString serverAName = connectionName(QStringLiteral("server-a"));
    const QString serverBName = connectionName(QStringLiteral("server-b"));
    const QString clientName = connectionName(QStringLiteral("client"));
    auto serverA = QDBusConnection::connectToBus(bus.address(), serverAName);
    auto serverB = QDBusConnection::connectToBus(bus.address(), serverBName);
    auto client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(serverA.isConnected());
    QVERIFY(serverB.isConnected());
    QVERIFY(client.isConnected());

    FakeCompositor compositorA(QByteArrayLiteral("snapshot-a"));
    FakeCompositor compositorB(QByteArrayLiteral("snapshot-b"));
    QVERIFY(registerCompositor(serverA, &compositorA));
    const QString ownerA = serverA.baseService();
    const QString ownerB = serverB.baseService();
    QVERIFY(ownerA.startsWith(QLatin1Char(':')));
    QVERIFY(ownerB.startsWith(QLatin1Char(':')));
    QVERIFY(ownerA != ownerB);

    {
        QtCompositorVisibilityTransport transport(client);
        QSignalSpy ownerSpy(&transport,
                            &CompositorVisibilityTransport::serviceOwnerChanged);
        QSignalSpy invalidationSpy(
            &transport, &CompositorVisibilityTransport::snapshotInvalidated);
        QSignalSpy snapshotSpy(&transport,
                               &CompositorVisibilityTransport::snapshotReceived);
        QSignalSpy failureSpy(&transport,
                              &CompositorVisibilityTransport::snapshotFailed);
        QString startError;
        QVERIFY2(transport.start(&startError), qPrintable(startError));
        QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 1, 5'000);
        QCOMPARE(ownerSpy.constFirst().constFirst().toString(), ownerA);

        transport.requestSnapshot(7, ownerA);
        QTRY_COMPARE_WITH_TIMEOUT(snapshotSpy.size(), 1, 5'000);
        QCOMPARE(snapshotSpy.constFirst().at(0).toULongLong(), quint64(7));
        QCOMPARE(snapshotSpy.constFirst().at(1).toString(), ownerA);
        QCOMPARE(snapshotSpy.constFirst().at(2).toByteArray(),
                 QByteArrayLiteral("snapshot-a"));
        QCOMPARE(failureSpy.size(), 0);

        Q_EMIT compositorA.ShellVisibilityChanged();
        QTRY_COMPARE_WITH_TIMEOUT(invalidationSpy.size(), 1, 5'000);
        QCOMPARE(invalidationSpy.constFirst().constFirst().toString(), ownerA);

        QVERIFY(serverA.unregisterService(QString::fromLatin1(ServiceName)));
        QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 2, 5'000);
        QVERIFY(ownerSpy.at(1).constFirst().toString().isEmpty());
        Q_EMIT compositorA.ShellVisibilityChanged();
        QTest::qWait(50);
        QCOMPARE(invalidationSpy.size(), 1);

        QVERIFY(registerCompositor(serverB, &compositorB));
        QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 3, 5'000);
        QCOMPARE(ownerSpy.at(2).constFirst().toString(), ownerB);

        transport.requestSnapshot(8, ownerB);
        QTRY_COMPARE_WITH_TIMEOUT(snapshotSpy.size(), 2, 5'000);
        QCOMPARE(snapshotSpy.at(1).at(1).toString(), ownerB);
        QCOMPARE(snapshotSpy.at(1).at(2).toByteArray(),
                 QByteArrayLiteral("snapshot-b"));

        Q_EMIT compositorA.ShellVisibilityChanged();
        QTest::qWait(50);
        QCOMPARE(invalidationSpy.size(), 1);
        Q_EMIT compositorB.ShellVisibilityChanged();
        QTRY_COMPARE_WITH_TIMEOUT(invalidationSpy.size(), 2, 5'000);
        QCOMPARE(invalidationSpy.at(1).constFirst().toString(), ownerB);

        transport.stop();
        QVERIFY(serverB.unregisterService(QString::fromLatin1(ServiceName)));
    }

    serverA.unregisterObject(QString::fromLatin1(ObjectPath));
    serverB.unregisterObject(QString::fromLatin1(ObjectPath));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverBName);
    QDBusConnection::disconnectFromBus(serverAName);
}

QTEST_GUILESS_MAIN(QtCompositorVisibilityTransportTests)
#include "tst_qt_compositor_visibility_transport.moc"
