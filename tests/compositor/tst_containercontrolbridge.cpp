// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/containercontrolbridge.h"
#include "qindaqt/compositor/controlcodec.h"
#include "qindaqt/compositor/controlendpoint.h"

#include "testfixtures.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Compositor;

class ContainerControlBridgeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void commitsBatchesAndPublishesOneRevision();
    void commitsReorganizationOperations();
    void unwrapsSingletonAfterDetach();
    void rollsBackRejectedStagingSplit();
    void rejectsInvalidBatchWithoutPreparingScene();
    void rejectsStaleAndUnsupportedRequests();
    void preservesModelWhenSceneTransitionFails();
    void endpointRejectsMalformedJsonAndCommitsValidJson();
};

QJsonObject splitOperation()
{
    return {{QStringLiteral("type"), QStringLiteral("split-window")},
            {QStringLiteral("targetWindowId"), QStringLiteral("window-a")},
            {QStringLiteral("newWindowId"), QStringLiteral("window-b")},
            {QStringLiteral("newLeafNodeId"), QStringLiteral("leaf-b")},
            {QStringLiteral("splitNodeId"), QStringLiteral("split-ab")},
            {QStringLiteral("orientation"), QStringLiteral("horizontal")},
            {QStringLiteral("ratio"), 0.4},
            {QStringLiteral("position"), QStringLiteral("second")}};
}

void ContainerControlBridgeTest::commitsBatchesAndPublishesOneRevision()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QString error;
    QVERIFY2(bridge.registerContainer(Test::seedContainer(), &error), qPrintable(error));
    QSignalSpy committed(&bridge, &ContainerControlBridge::containerCommitted);

    const auto reply = bridge.submit(Test::request(
        0,
        {splitOperation(),
         QJsonObject{{QStringLiteral("type"), QStringLiteral("set-split-ratio")},
                     {QStringLiteral("splitNodeId"), QStringLiteral("split-ab")},
                     {QStringLiteral("ratio"), 0.7}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("add-page")},
                     {QStringLiteral("pageId"), QStringLiteral("page-c")},
                     {QStringLiteral("leafNodeId"), QStringLiteral("leaf-c")},
                     {QStringLiteral("windowId"), QStringLiteral("window-c")}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("activate-page")},
                     {QStringLiteral("pageId"), QStringLiteral("page-c")}}}));

    QVERIFY(reply.committed());
    QCOMPARE(reply.revision, quint64(1));
    QCOMPARE(adapter.prepareCalls, 1);
    QCOMPARE(adapter.commitCalls, 1);
    QCOMPARE(committed.size(), 1);
    QCOMPARE(bridge.revision(QStringLiteral("container-a")), std::optional<quint64>(1));
    const auto parsed = QindaQt::Core::WindowContainer::fromJson(reply.snapshot, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->activePageId(), QStringLiteral("page-c"));
    QCOMPARE(parsed->findNode(QStringLiteral("split-ab"))->ratio().value(), 0.7);
}

void ContainerControlBridgeTest::commitsReorganizationOperations()
{
    auto container = Test::seedContainer();
    QString error;
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-ab"),
                                   QindaQt::Core::SplitOrientation::Horizontal,
                                   0.5,
                                   QindaQt::Core::InsertPosition::Second},
                                  &error));
    QVERIFY(container.addPage(QStringLiteral("page-c"), QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"), &error));

    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QVERIFY(bridge.registerContainer(std::move(container), &error));
    const auto reply = bridge.submit(Test::request(
        0,
        {QJsonObject{{QStringLiteral("type"), QStringLiteral("move-page")},
                     {QStringLiteral("pageId"), QStringLiteral("page-c")},
                     {QStringLiteral("destinationIndex"), 0}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("swap-windows")},
                     {QStringLiteral("firstWindowId"), QStringLiteral("window-a")},
                     {QStringLiteral("secondWindowId"), QStringLiteral("window-b")}},
         QJsonObject{{QStringLiteral("type"), QStringLiteral("detach-window")},
                     {QStringLiteral("windowId"), QStringLiteral("window-c")}}}));

    QVERIFY(reply.committed());
    const auto parsed = QindaQt::Core::WindowContainer::fromJson(reply.snapshot, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->pages().size(), 1);
    QCOMPARE(parsed->findNode(QStringLiteral("leaf-a"))->windowId(), QStringLiteral("window-b"));
    QCOMPARE(parsed->findNode(QStringLiteral("leaf-b"))->windowId(), QStringLiteral("window-a"));
    QVERIFY(!parsed->findWindow(QStringLiteral("window-c")));
}

void ContainerControlBridgeTest::unwrapsSingletonAfterDetach()
{
    auto container = Test::seedContainer();
    QString error;
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-ab"),
                                   QindaQt::Core::SplitOrientation::Horizontal,
                                   0.5,
                                   QindaQt::Core::InsertPosition::Second},
                                  &error));
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QVERIFY(bridge.registerContainer(std::move(container), &error));
    QSignalSpy committed(&bridge, &ContainerControlBridge::containerCommitted);

    const auto reply = bridge.submit(Test::request(
        0, {QJsonObject{{QStringLiteral("type"), QStringLiteral("detach-window")},
                        {QStringLiteral("windowId"), QStringLiteral("window-b")}}}));

    QVERIFY(reply.committed());
    QCOMPARE(reply.revision, quint64(1));
    QVERIFY(reply.snapshot.value(QStringLiteral("pages")).toArray().isEmpty());
    QVERIFY(!bridge.contains(QStringLiteral("container-a")));
    QCOMPARE(adapter.commitCalls, 1);
    QCOMPARE(committed.size(), 1);
    QCOMPARE(committed.constFirst().at(1).toULongLong(), quint64(1));
    QCOMPARE(committed.constFirst().at(2).toJsonObject()
                 .value(QStringLiteral("pages")).toArray().size(),
             0);
}

void ContainerControlBridgeTest::rollsBackRejectedStagingSplit()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    adapter.commitSucceeds = false;

    const auto reply = bridge.submitStagedSplit(
        Test::seedContainer(), Test::request(0, {splitOperation()}));

    QCOMPARE(reply.status, ReplyStatus::Rejected);
    QCOMPARE(reply.failure.code, QStringLiteral("scene-commit-failed"));
    QCOMPARE(adapter.prepareCalls, 1);
    QCOMPARE(adapter.commitCalls, 1);
    QVERIFY(!bridge.contains(QStringLiteral("container-a")));
    QVERIFY(!bridge.snapshot(QStringLiteral("container-a")));

    Test::RecordingSceneAdapter succeedingAdapter;
    ContainerControlBridge succeedingBridge(succeedingAdapter);
    const auto committed = succeedingBridge.submitStagedSplit(
        Test::seedContainer(), Test::request(0, {splitOperation()}));
    QVERIFY(committed.committed());
    QCOMPARE(committed.revision, quint64(1));
    QVERIFY(succeedingBridge.contains(QStringLiteral("container-a")));
}

void ContainerControlBridgeTest::rejectsInvalidBatchWithoutPreparingScene()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QString error;
    QVERIFY(bridge.registerContainer(Test::seedContainer(), &error));
    const auto before = bridge.snapshot(QStringLiteral("container-a"));

    const auto reply = bridge.submit(Test::request(
        0,
        {splitOperation(),
         QJsonObject{{QStringLiteral("type"), QStringLiteral("set-split-ratio")},
                     {QStringLiteral("splitNodeId"), QStringLiteral("missing")},
                     {QStringLiteral("ratio"), 0.5}}}));

    QCOMPARE(reply.status, ReplyStatus::Rejected);
    QCOMPARE(reply.failure.operationIndex, 1);
    QCOMPARE(reply.failure.code, QStringLiteral("mutation-rejected"));
    QCOMPARE(adapter.prepareCalls, 0);
    QCOMPARE(bridge.revision(QStringLiteral("container-a")), std::optional<quint64>(0));
    QCOMPARE(bridge.snapshot(QStringLiteral("container-a")), before);
}

void ContainerControlBridgeTest::rejectsStaleAndUnsupportedRequests()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QString error;
    QVERIFY(bridge.registerContainer(Test::seedContainer(), &error));

    auto stale = Test::request(9, {splitOperation()});
    QCOMPARE(bridge.submit(stale).status, ReplyStatus::Conflict);
    stale.protocol = {2, 0};
    const auto unsupported = bridge.submit(stale);
    QCOMPARE(unsupported.status, ReplyStatus::Rejected);
    QCOMPARE(unsupported.failure.code, QStringLiteral("unsupported-protocol"));
    auto empty = Test::request(0, {});
    QCOMPARE(bridge.submit(empty).failure.code, QStringLiteral("malformed-request"));
    QCOMPARE(adapter.prepareCalls, 0);
}

void ContainerControlBridgeTest::preservesModelWhenSceneTransitionFails()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    QString error;
    QVERIFY(bridge.registerContainer(Test::seedContainer(), &error));
    const auto before = bridge.snapshot(QStringLiteral("container-a"));

    adapter.prepareSucceeds = false;
    auto reply = bridge.submit(Test::request(0, {splitOperation()}));
    QCOMPARE(reply.failure.code, QStringLiteral("scene-prepare-failed"));
    QCOMPARE(bridge.snapshot(QStringLiteral("container-a")), before);

    adapter.prepareSucceeds = true;
    adapter.commitSucceeds = false;
    reply = bridge.submit(Test::request(0, {splitOperation()}));
    QCOMPARE(reply.failure.code, QStringLiteral("scene-commit-failed"));
    QCOMPARE(bridge.snapshot(QStringLiteral("container-a")), before);
    QCOMPARE(bridge.revision(QStringLiteral("container-a")), std::optional<quint64>(0));
}

void ContainerControlBridgeTest::endpointRejectsMalformedJsonAndCommitsValidJson()
{
    Test::RecordingSceneAdapter adapter;
    ContainerControlBridge bridge(adapter);
    ControlEndpoint endpoint(bridge);
    QSignalSpy endpointEvents(&endpoint, &ControlEndpoint::ContainerCommitted);
    QString error;
    QVERIFY(bridge.registerContainer(Test::seedContainer(), &error));

    auto response = QJsonDocument::fromJson(endpoint.Submit(QByteArrayLiteral("not-json"))).object();
    QCOMPARE(response.value(QStringLiteral("status")).toString(), QStringLiteral("rejected"));
    QCOMPARE(response.value(QStringLiteral("failure")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("malformed-json"));

    response = QJsonDocument::fromJson(endpoint.Submit(QByteArray(256 * 1024 + 1, 'x'))).object();
    QCOMPARE(response.value(QStringLiteral("failure")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("request-too-large"));

    const QJsonArray operations{splitOperation()};
    const auto request = ControlCodec::compactJson(Test::requestJson(0, operations));
    response = QJsonDocument::fromJson(endpoint.Submit(request)).object();
    QCOMPARE(response.value(QStringLiteral("status")).toString(), QStringLiteral("committed"));
    QCOMPARE(response.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
    QVERIFY(!endpoint.Capabilities().isEmpty());
    QCOMPARE(endpointEvents.size(), 1);
    response = QJsonDocument::fromJson(endpoint.Snapshot(QStringLiteral("container-a"))).object();
    QCOMPARE(response.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(response.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
}

QTEST_GUILESS_MAIN(ContainerControlBridgeTest)
#include "tst_containercontrolbridge.moc"
