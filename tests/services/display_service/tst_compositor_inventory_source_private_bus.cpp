// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/display_service_ports.h>

#include "support/private_bus_test_support.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtTest/QTest>

using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

constexpr auto kCompositorService = "org.qindaqt.Compositor";
constexpr auto kCompositorPath = "/org/qindaqt/Compositor";

class DelayedCompositor final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
    explicit DelayedCompositor(QDBusConnection connection)
        : m_connection(std::move(connection))
    {
    }

    void setPayload(QByteArray payload) { m_payload = std::move(payload); }
    void setDelayReplies(const bool value) { m_delayReplies = value; }
    [[nodiscard]] int callCount() const noexcept { return m_callCount; }
    [[nodiscard]] qsizetype pendingCount() const noexcept { return m_pending.size(); }

    bool replyOldest(QByteArray payload)
    {
        if (m_pending.isEmpty()) {
            return false;
        }
        const QDBusMessage request = m_pending.takeFirst();
        return m_connection.send(
            request.createReply(QVariantList{QVariant::fromValue(std::move(payload))}));
    }

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray Outputs()
    {
        ++m_callCount;
        if (!m_delayReplies) {
            return m_payload;
        }
        // AGENT-GUARD: Retain the inbound message, not QDBusContext itself;
        // the context expires as soon as this method returns.
        m_pending.push_back(message());
        setDelayedReply(true);
        return {};
    }

Q_SIGNALS:
    Q_SCRIPTABLE void OutputsChanged();

private:
    QDBusConnection m_connection;
    QByteArray m_payload;
    QList<QDBusMessage> m_pending;
    int m_callCount = 0;
    bool m_delayReplies = false;
};

class RecordingObserver final : public InventoryObserver
{
public:
    void inventoryObserved(const InventoryFrame &frame) override
    {
        frames.push_back(frame);
    }
    void inventoryUnavailable() override { ++unavailableCount; }

    QList<InventoryFrame> frames;
    int unavailableCount = 0;
};

bool registerCompositorObject(QDBusConnection &connection,
                              DelayedCompositor *object)
{
    return connection.registerObject(
        QString::fromLatin1(kCompositorPath), object,
        QDBusConnection::ExportScriptableSlots
            | QDBusConnection::ExportScriptableSignals);
}

} // namespace

class CompositorInventorySourcePrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fencesOwnerRepliesCoalescesInvalidationsAndStops();
};

void CompositorInventorySourcePrivateBusTest::
    fencesOwnerRepliesCoalescesInvalidationsAndStops()
{
    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));

    const QString serverAName = privateConnectionName(QStringLiteral("source-a"));
    const QString serverBName = privateConnectionName(QStringLiteral("source-b"));
    const QString clientName = privateConnectionName(QStringLiteral("source-client"));
    QDBusConnection serverA = QDBusConnection::connectToBus(bus.address(), serverAName);
    QDBusConnection serverB = QDBusConnection::connectToBus(bus.address(), serverBName);
    QDBusConnection client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(serverA.isConnected());
    QVERIFY(serverB.isConnected());
    QVERIFY(client.isConnected());

    DelayedCompositor compositorA(serverA);
    DelayedCompositor compositorB(serverB);
    compositorA.setPayload(compositorPayload(1));
    compositorB.setPayload(compositorPayload(1, QStringLiteral("Replacement")));
    QVERIFY(registerCompositorObject(serverA, &compositorA));
    QVERIFY(registerCompositorObject(serverB, &compositorB));
    QVERIFY(serverA.registerService(QString::fromLatin1(kCompositorService)));
    const QString ownerA = serverA.baseService();
    const QString ownerB = serverB.baseService();
    QVERIFY(ownerA != ownerB);

    RecordingObserver observer;
    {
        std::unique_ptr<InventorySource> source =
            makeCompositorInventorySource(client);
        source->setObserver(&observer);
        QCOMPARE(source->start(), InventorySourceStartStatus::Started);
        QTRY_COMPARE_WITH_TIMEOUT(observer.frames.size(), 1, 5'000);
        QCOMPARE(observer.frames.constLast().uniqueOwner, ownerA);
        QCOMPARE(observer.frames.constLast().outputGeneration, quint64(1));

        compositorA.setDelayReplies(true);
        compositorA.setPayload(compositorPayload(2, QStringLiteral("First dirty")));
        Q_EMIT compositorA.OutputsChanged();
        QTRY_COMPARE_WITH_TIMEOUT(compositorA.pendingCount(), qsizetype(1), 5'000);
        const int firstDirtyCall = compositorA.callCount();
        compositorA.setPayload(compositorPayload(3, QStringLiteral("Coalesced")));
        Q_EMIT compositorA.OutputsChanged();
        QTest::qWait(50);
        QCOMPARE(compositorA.callCount(), firstDirtyCall);
        QVERIFY(compositorA.replyOldest(
            compositorPayload(2, QStringLiteral("First dirty"))));
        QTRY_COMPARE_WITH_TIMEOUT(observer.frames.size(), 2, 5'000);
        QTRY_COMPARE_WITH_TIMEOUT(compositorA.pendingCount(), qsizetype(1), 5'000);
        QCOMPARE(compositorA.callCount(), firstDirtyCall + 1);
        QVERIFY(compositorA.replyOldest(
            compositorPayload(3, QStringLiteral("Coalesced"))));
        QTRY_COMPARE_WITH_TIMEOUT(observer.frames.size(), 3, 5'000);
        QCOMPARE(observer.frames.constLast().outputGeneration, quint64(3));

        compositorA.setPayload(compositorPayload(4, QStringLiteral("Stale owner")));
        Q_EMIT compositorA.OutputsChanged();
        QTRY_COMPARE_WITH_TIMEOUT(compositorA.pendingCount(), qsizetype(1), 5'000);
        QVERIFY(serverA.unregisterService(QString::fromLatin1(kCompositorService)));
        QTRY_VERIFY_WITH_TIMEOUT(observer.unavailableCount > 0, 5'000);
        QVERIFY(serverB.registerService(QString::fromLatin1(kCompositorService)));
        QTRY_COMPARE_WITH_TIMEOUT(observer.frames.size(), 4, 5'000);
        QCOMPARE(observer.frames.constLast().uniqueOwner, ownerB);
        QCOMPARE(observer.frames.constLast().outputGeneration, quint64(1));
        const qsizetype acceptedAfterReplacement = observer.frames.size();
        QVERIFY(compositorA.replyOldest(
            compositorPayload(4, QStringLiteral("Stale owner"))));
        QTest::qWait(50);
        QCOMPARE(observer.frames.size(), acceptedAfterReplacement);

        compositorB.setDelayReplies(true);
        compositorB.setPayload(compositorPayload(2, QStringLiteral("After stop")));
        Q_EMIT compositorB.OutputsChanged();
        QTRY_COMPARE_WITH_TIMEOUT(compositorB.pendingCount(), qsizetype(1), 5'000);
        const qsizetype observationsBeforeStop = observer.frames.size();
        const int unavailableBeforeStop = observer.unavailableCount;
        source->stop();
        QVERIFY(compositorB.replyOldest(
            compositorPayload(2, QStringLiteral("After stop"))));
        QTest::qWait(50);
        QCOMPARE(observer.frames.size(), observationsBeforeStop);
        QCOMPARE(observer.unavailableCount, unavailableBeforeStop);
    }

    (void)serverB.unregisterService(QString::fromLatin1(kCompositorService));
    serverA.unregisterObject(QString::fromLatin1(kCompositorPath));
    serverB.unregisterObject(QString::fromLatin1(kCompositorPath));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverBName);
    QDBusConnection::disconnectFromBus(serverAName);
}

QTEST_MAIN(CompositorInventorySourcePrivateBusTest)

#include "tst_compositor_inventory_source_private_bus.moc"
