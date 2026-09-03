// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include "status_notifier_fake_item_test_support.h"
#include "status_notifier_private_bus_test_support.h"

#include <QDBusConnection>
#include <QStandardPaths>
#include <QtTest>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace
{

OwnerKey fakeKey(const QDBusConnection &itemConnection, quint64 generation = 7)
{
    OwnerKey key;
    key.uniqueName = itemConnection.baseService();
    key.objectPath = QStringLiteral("/StatusNotifierItem");
    key.generation = generation;
    return key;
}

class FetchRecorder final : public QObject
{
public:
    QList<ItemDescriptorFetch> results;

    void attach(StatusNotifierItemClient *client)
    {
        connect(client, &StatusNotifierItemClient::descriptorFetched, this,
                [this](const ItemDescriptorFetch &result) { results.append(result); });
    }
};

} // namespace

class StatusNotifierItemClientTests final : public QObject
{
    Q_OBJECT

private slots:
    void decodesFullDescriptorAndWireDetails();
    void refetchesWhenNewSignalsArrive();
    void ignoresUnknownAndMissingProperties();
    void rejectsOversizedPixmaps();
    void rejectsAbsurdPixmapCounts();
    void rejectsControlCharactersAndBadEnums();
    void dropsLateRepliesBehindTheFence();
    void reportsUnansweredFetchesAsNotReceived();
    void dispatchesRecordedIntents();
    void recordsMenuPathWithoutDescriptorMenu();
};

void StatusNotifierItemClientTests::decodesFullDescriptorAndWireDetails()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-a"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-a"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->category = QStringLiteral("Communications");
    item->status = QStringLiteral("NeedsAttention");
    item->title = QStringLiteral("Messages waiting");
    item->iconPixmap = {FakeStatusNotifierItem::pixmap(2, 2, 0xFF336699)};
    item->attentionIconName = QStringLiteral("attention-icon");
    item->attentionPixmap = {FakeStatusNotifierItem::pixmap(1, 1, 0xFF000000)};
    item->attentionMovieName = QStringLiteral("attention-movie");
    // Copy-assign: Q_GADGET structs are not aggregate-initializable.
    FakeToolTipWire toolTip = item->toolTip;
    toolTip.iconName = QStringLiteral("tooltip-icon");
    toolTip.pixmaps = {FakeStatusNotifierItem::pixmap(1, 1, 0xFFFFFFFF)};
    toolTip.title = QStringLiteral("Tooltip title");
    toolTip.description = QStringLiteral("Tooltip body");
    item->toolTip = toolTip;
    item->itemIsMenu = true;
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);

    const ItemDescriptorFetch &result = recorder.results.constFirst();
    QVERIFY(result.replyReceived);
    QCOMPARE(result.generation, 7);
    QVERIFY2(result.validation.accepted,
             qPrintable(result.validation.reasonCode));
    QCOMPARE(result.descriptor.category, ItemCategory::Communications);
    QCOMPARE(result.descriptor.identity, QStringLiteral("org.qindaqt.fake"));
    QCOMPARE(result.descriptor.title, QStringLiteral("Messages waiting"));
    QCOMPARE(result.descriptor.status, ItemStatus::NeedsAttention);
    QCOMPARE(result.descriptor.icon.iconName, QStringLiteral("fake-icon"));
    QCOMPARE(result.descriptor.icon.pixmaps.size(), 1);
    QCOMPARE(result.descriptor.icon.pixmaps.constFirst().width, 2u);
    QCOMPARE(result.descriptor.icon.pixmaps.constFirst().argb.size(), 16);
    QCOMPARE(result.descriptor.icon.attentionIconName, QStringLiteral("attention-icon"));
    QCOMPARE(result.descriptor.icon.attentionMovieName, QStringLiteral("attention-movie"));
    QCOMPARE(result.descriptor.toolTip.title, QStringLiteral("Tooltip title"));
    QCOMPARE(result.descriptor.toolTip.description, QStringLiteral("Tooltip body"));
    QCOMPARE(result.wire.windowId, 42u);
    QCOMPARE(result.wire.overlayIconName, QStringLiteral("fake-overlay"));
    QVERIFY(result.wire.itemIsMenu);
    QCOMPARE(result.wire.menuObjectPath, QStringLiteral("/Menu"));

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-a"));
}

void StatusNotifierItemClientTests::refetchesWhenNewSignalsArrive()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-b"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-b"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);

    item->title = QStringLiteral("Updated title");
    emit item->NewTitle();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 2, 5'000);
    QCOMPARE(recorder.results.at(1).descriptor.title, QStringLiteral("Updated title"));

    item->status = QStringLiteral("Passive");
    emit item->NewStatus(QStringLiteral("Passive"));
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 3, 5'000);
    QCOMPARE(recorder.results.at(2).descriptor.status, ItemStatus::Passive);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-b"));
}

void StatusNotifierItemClientTests::ignoresUnknownAndMissingProperties()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-c"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-c"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->title.clear();
    item->iconName.clear();
    // Serve a deliberately malformed IconPixmap (array of strings, not
    // a(iiay)) through the override map to exercise the client's fail-closed
    // decode path.
    item->setProperty(
        "wireOverrides",
        QVariantMap{{QStringLiteral("IconPixmap"),
                     QVariantList{QStringLiteral("not"), QStringLiteral("a"),
                                  QStringLiteral("pixmap")}}});
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);

    const ItemDescriptorFetch &result = recorder.results.constFirst();
    QVERIFY(result.replyReceived);
    // A wrongly-typed pixmap list fails the descriptor closed (hostile inner
    // shape), while the unknown extension property never reaches any field.
    QVERIFY(!result.validation.accepted);
    QCOMPARE(result.validation.error, ValidationError::InvalidIcon);
    QVERIFY(result.descriptor.icon.pixmaps.isEmpty());
    QVERIFY(result.descriptor.identity ==
            QStringLiteral("org.qindaqt.fake")); // Id still decodes.
    QVERIFY(!result.descriptor.title.contains(QStringLiteral("ignored")));

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-c"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-c"));
}

void StatusNotifierItemClientTests::rejectsOversizedPixmaps()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-d"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-d"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    // Dimension beyond the shared 512-pixel bound; the rest of the descriptor
    // stays valid so the dimension bound is the first and only failure.
    item->iconPixmap = {FakeStatusNotifierItem::pixmap(int(kMaxIconPixmapDimension) + 1,
                                                        1, 0xFF000000)};
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);
    QVERIFY(!recorder.results.constFirst().validation.accepted);
    QCOMPARE(recorder.results.constFirst().validation.error, ValidationError::InvalidIcon);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-d"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-d"));
}

void StatusNotifierItemClientTests::rejectsAbsurdPixmapCounts()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-e"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-e"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    for (int index = 0; index <= kMaxIconPixmaps; ++index) {
        item->iconPixmap.append(FakeStatusNotifierItem::pixmap(1, 1, 0xFF000000));
    }
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);
    QVERIFY(!recorder.results.constFirst().validation.accepted);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-e"));
}

void StatusNotifierItemClientTests::rejectsControlCharactersAndBadEnums()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-f"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-f"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->title = QStringLiteral("bad\x01title");
    item->status = QStringLiteral("Exploding");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);
    const ItemDescriptorFetch &result = recorder.results.constFirst();
    QVERIFY(result.replyReceived);
    QVERIFY(!result.validation.accepted);
    // The first decode failure wins; both hostile facts fail the descriptor.
    QVERIFY(result.validation.error == ValidationError::InvalidCategory
            || result.validation.error == ValidationError::InvalidStatus
            || result.validation.error == ValidationError::InvalidTitle);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-f"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-f"));
}

void StatusNotifierItemClientTests::dropsLateRepliesBehindTheFence()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-g"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-g"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    bool fenceOpen = true;
    StatusNotifierItemClient client(
        readerConnection, fakeKey(itemConnection),
        [&fenceOpen](quint64 generation) { return fenceOpen && generation == 7; }, 2'000);
    FetchRecorder recorder;
    recorder.attach(&client);

    // Start the fetch, then close the fence in the same event-loop turn: the
    // in-flight reply arrives later and must be dropped, not emitted.
    client.fetchDescriptor();
    fenceOpen = false;
    QTest::qWait(300);
    QCOMPARE(recorder.results.size(), 0);

    // A fenced client also refuses to start new fetches.
    client.fetchDescriptor();
    QTest::qWait(300);
    QCOMPARE(recorder.results.size(), 0);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-g"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-g"));
}

void StatusNotifierItemClientTests::reportsUnansweredFetchesAsNotReceived()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-h"));

    // No item object exists at this path; the fetch must time out and report
    // replyReceived == false so the caller can still observe the key.
    OwnerKey key;
    key.uniqueName = QStringLiteral(":1.424242");
    key.objectPath = QStringLiteral("/StatusNotifierItem");
    key.generation = 7;
    StatusNotifierItemClient client(readerConnection, key,
                                    [](quint64 generation) { return generation == 7; },
                                    200);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);
    QVERIFY(!recorder.results.constFirst().replyReceived);
    QCOMPARE(recorder.results.constFirst().key.uniqueName,
             QStringLiteral(":1.424242"));

    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-h"));
}

void StatusNotifierItemClientTests::dispatchesRecordedIntents()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-i"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-i"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    client.activate(3, 7);
    client.secondaryActivate(11, 13);
    client.contextMenu(17, 19);
    QVERIFY(client.scroll(-4, QStringLiteral("vertical")));
    QVERIFY(!client.scroll(1, QStringLiteral("diagonal")));

    QTRY_COMPARE_WITH_TIMEOUT(item->recordedIntents.size(), 4, 5'000);
    const QList<QVariant> activateArgs{3, QVariant(7u)};
    const QList<QVariant> scrollArgs{-4, QVariant(QStringLiteral("vertical"))};
    QCOMPARE(item->recordedIntents.at(0).member, QStringLiteral("Activate"));
    QCOMPARE(item->recordedIntents.at(0).arguments, activateArgs);
    QCOMPARE(item->recordedIntents.at(1).member, QStringLiteral("SecondaryActivate"));
    QCOMPARE(item->recordedIntents.at(2).member, QStringLiteral("ContextMenu"));
    QCOMPARE(item->recordedIntents.at(3).member, QStringLiteral("Scroll"));
    QCOMPARE(item->recordedIntents.at(3).arguments, scrollArgs);

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-i"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-i"));
}

void StatusNotifierItemClientTests::recordsMenuPathWithoutDescriptorMenu()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-item-j"));
    auto readerConnection = connectToPrivateBus(bus.address(), QStringLiteral("client-reader-j"));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->menu = QDBusObjectPath(QStringLiteral("/DBusMenu"));
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));

    StatusNotifierItemClient client(readerConnection, fakeKey(itemConnection),
                                    [](quint64 generation) { return generation == 7; },
                                    2'000);
    FetchRecorder recorder;
    recorder.attach(&client);
    client.fetchDescriptor();
    QTRY_COMPARE_WITH_TIMEOUT(recorder.results.size(), 1, 5'000);
    QCOMPARE(recorder.results.constFirst().wire.menuObjectPath,
             QStringLiteral("/DBusMenu"));
    QVERIFY(recorder.results.constFirst().descriptor.menu.entries.isEmpty());

    QDBusConnection::disconnectFromBus(QStringLiteral("client-item-j"));
    QDBusConnection::disconnectFromBus(QStringLiteral("client-reader-j"));
}

QTEST_GUILESS_MAIN(StatusNotifierItemClientTests)
#include "tst_status_notifier_item_client.moc"
