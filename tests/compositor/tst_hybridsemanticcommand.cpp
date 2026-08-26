// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridsemanticcommand.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

Hybrid::WindowTopology topologyWithPages()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    const bool built = container.addPage(QStringLiteral("page-a"),
                                         QStringLiteral("leaf-a"),
                                         QStringLiteral("window-a"), &error)
        && container.splitWindow({QStringLiteral("window-a"),
                                  QStringLiteral("window-b"),
                                  QStringLiteral("leaf-b"),
                                  QStringLiteral("split-ab"),
                                  Core::SplitOrientation::Horizontal,
                                  0.5,
                                  Core::InsertPosition::Second}, &error)
        && container.addPage(QStringLiteral("page-c"),
                             QStringLiteral("leaf-c"),
                             QStringLiteral("window-c"), &error)
        && container.addPage(QStringLiteral("page-d"),
                             QStringLiteral("leaf-d"),
                             QStringLiteral("window-d"), &error);
    if (!built) {
        qFatal("failed to build semantic-command fixture: %s", qPrintable(error));
    }
    auto topology = Hybrid::WindowTopology::create(
        {QStringLiteral("independent")}, {container}, 8, &error);
    if (!topology) {
        qFatal("failed to build semantic-command topology: %s", qPrintable(error));
    }
    return *topology;
}

} // namespace

class HybridSemanticCommandTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesActivePageDockWithoutCoordinates();
    void cyclesPagesInStableLogicalOrder();
    void reordersPagesInStableLogicalOrder();
    void resolvesEveryGroupWindowAction();
    void rejectsMissingIndependentAndSinglePageContexts();
    void dispatcherRoutesTypedRequestsAndReportsMissingHandlers();
};

void HybridSemanticCommandTest::resolvesActivePageDockWithoutCoordinates()
{
    const auto topology = topologyWithPages();
    QString error;
    auto request = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-b"),
        HybridSemanticCommand::BeginPageDock, &error);
    QVERIFY2(request, qPrintable(error));
    QCOMPARE(request->kind, HybridSemanticRequestKind::BeginPageDock);
    QCOMPARE(request->containerId, QStringLiteral("group"));
    QCOMPARE(request->pageId, QStringLiteral("page-a"));
    QCOMPARE(request->dockSource.kind, HybridInput::HitKind::Tab);
    QCOMPARE(request->dockSource.memberId, QStringLiteral("window-b"));
    QCOMPARE(request->dockSource.pageId, QStringLiteral("page-a"));
    QVERIFY(request->dockSource.isValid());

    // An externally activated inactive-page member must not make that stale
    // page the keyboard source; topology's active page remains authoritative.
    request = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-c"),
        HybridSemanticCommand::BeginPageDock, &error);
    QVERIFY2(request, qPrintable(error));
    QCOMPARE(request->dockSource.memberId, QStringLiteral("window-a"));
    QCOMPARE(request->dockSource.pageId, QStringLiteral("page-a"));
}

void HybridSemanticCommandTest::cyclesPagesInStableLogicalOrder()
{
    const auto topology = topologyWithPages();
    QString error;
    const auto next = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ActivateNextPage, &error);
    QVERIFY2(next, qPrintable(error));
    QCOMPARE(next->kind, HybridSemanticRequestKind::ActivatePage);
    QCOMPARE(next->pageId, QStringLiteral("page-c"));

    const auto previous = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ActivatePreviousPage, &error);
    QVERIFY2(previous, qPrintable(error));
    QCOMPARE(previous->pageId, QStringLiteral("page-d"));
}

void HybridSemanticCommandTest::reordersPagesInStableLogicalOrder()
{
    const auto topology = topologyWithPages();
    QString error;
    const auto next = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ReorderPageNext, &error);
    QVERIFY2(next, qPrintable(error));
    QCOMPARE(next->kind, HybridSemanticRequestKind::ReorderPage);
    QCOMPARE(next->pageId, QStringLiteral("page-a"));
    QCOMPARE(next->destinationPageIndex, 1);

    const auto previous = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ReorderPagePrevious, &error);
    QVERIFY2(previous, qPrintable(error));
    QCOMPARE(previous->pageId, QStringLiteral("page-a"));
    QCOMPARE(previous->destinationPageIndex, 2);

    const auto direct = HybridSemanticCommandResolver::reorderPage(
        topology, QStringLiteral("group"), QStringLiteral("page-c"), 2, &error);
    QVERIFY2(direct, qPrintable(error));
    QCOMPARE(direct->pageId, QStringLiteral("page-c"));
    QCOMPARE(direct->destinationPageIndex, 2);
    QVERIFY(!HybridSemanticCommandResolver::reorderPage(
        topology, QStringLiteral("group"), QStringLiteral("page-c"), 1, &error));
    QVERIFY(error.contains(QStringLiteral("already")));
}

void HybridSemanticCommandTest::resolvesEveryGroupWindowAction()
{
    const auto topology = topologyWithPages();
    const QVector<std::pair<HybridSemanticCommand, HybridChrome::WindowAction>> cases{
        {HybridSemanticCommand::CloseGroup, HybridChrome::WindowAction::Close},
        {HybridSemanticCommand::MinimizeGroup, HybridChrome::WindowAction::Minimize},
        {HybridSemanticCommand::MaximizeGroup, HybridChrome::WindowAction::Maximize},
        {HybridSemanticCommand::RestoreGroup, HybridChrome::WindowAction::Restore},
    };
    for (const auto &[command, action] : cases) {
        QString error;
        const auto request = HybridSemanticCommandResolver::resolveActive(
            topology, QStringLiteral("window-a"), command, &error);
        QVERIFY2(request, qPrintable(error));
        QCOMPARE(request->kind, HybridSemanticRequestKind::GroupWindowAction);
        QCOMPARE(request->containerId, QStringLiteral("group"));
        QCOMPARE(request->windowAction, std::optional(action));
        QVERIFY(request->isValid());
    }
}

void HybridSemanticCommandTest::rejectsMissingIndependentAndSinglePageContexts()
{
    const auto topology = topologyWithPages();
    QString error;
    QVERIFY(!HybridSemanticCommandResolver::resolveActive(
        topology, {}, HybridSemanticCommand::CloseGroup, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("independent"),
        HybridSemanticCommand::CloseGroup, &error));
    QVERIFY(error.contains(QStringLiteral("grouped")));
    QVERIFY(!HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("missing"),
        HybridSemanticCommand::BeginPageDock, &error));

    Core::WindowContainer one(QStringLiteral("one"));
    QVERIFY(one.addPage(QStringLiteral("only"), QStringLiteral("leaf"),
                        QStringLiteral("solo"), &error));
    QVERIFY(one.splitWindow({
        .targetWindowId = QStringLiteral("solo"),
        .newWindowId = QStringLiteral("peer"),
        .newLeafNodeId = QStringLiteral("peer-leaf"),
        .splitNodeId = QStringLiteral("only-split"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .position = Core::InsertPosition::Second,
    }, &error));
    const auto onePage = Hybrid::WindowTopology::create({}, {one}, 1, &error);
    QVERIFY2(onePage, qPrintable(error));
    QVERIFY(!HybridSemanticCommandResolver::resolveActive(
        *onePage, QStringLiteral("solo"),
        HybridSemanticCommand::ActivateNextPage, &error));
    QVERIFY(error.contains(QStringLiteral("alternate")));
}

void HybridSemanticCommandTest::dispatcherRoutesTypedRequestsAndReportsMissingHandlers()
{
    const auto topology = topologyWithPages();
    QStringList calls;
    HybridSemanticCommandDispatcher dispatcher({
        .beginPageDock = [&](const HybridInput::HitTarget &source, QString *) {
            calls.append(QStringLiteral("dock:%1").arg(source.pageId));
            return true;
        },
        .activatePage = [&](const QString &containerId, const QString &pageId,
                            QString *) {
            calls.append(QStringLiteral("page:%1:%2").arg(containerId, pageId));
            return true;
        },
        .reorderPage = [&](const QString &containerId, const QString &pageId,
                           qsizetype destination, QString *) {
            calls.append(QStringLiteral("reorder:%1:%2:%3")
                             .arg(containerId, pageId).arg(destination));
            return true;
        },
        .groupWindowAction = [&](const QString &containerId,
                                 HybridChrome::WindowAction action, QString *) {
            calls.append(QStringLiteral("action:%1:%2")
                             .arg(containerId).arg(static_cast<int>(action)));
            return true;
        },
    });

    QString error;
    const auto dock = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::BeginPageDock, &error);
    const auto page = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ActivateNextPage, &error);
    const auto action = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::MinimizeGroup, &error);
    const auto reorder = HybridSemanticCommandResolver::resolveActive(
        topology, QStringLiteral("window-a"),
        HybridSemanticCommand::ReorderPageNext, &error);
    QVERIFY(dock && page && reorder && action);
    QVERIFY2(dispatcher.dispatch(*dock, &error), qPrintable(error));
    QVERIFY2(dispatcher.dispatch(*page, &error), qPrintable(error));
    QVERIFY2(dispatcher.dispatch(*reorder, &error), qPrintable(error));
    QVERIFY2(dispatcher.dispatch(*action, &error), qPrintable(error));
    QCOMPARE(calls, QStringList({QStringLiteral("dock:page-a"),
                                 QStringLiteral("page:group:page-c"),
                                 QStringLiteral("reorder:group:page-a:1"),
                                 QStringLiteral("action:group:1")}));

    HybridSemanticCommandDispatcher unavailable({});
    QVERIFY(!unavailable.dispatch(*dock, &error));
    QVERIFY(error.contains(QStringLiteral("unavailable")));
}

QTEST_GUILESS_MAIN(HybridSemanticCommandTest)
#include "tst_hybridsemanticcommand.moc"
