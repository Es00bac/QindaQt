// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtaskidentitypolicy.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

Core::WindowContainer tabbedSplitContainer()
{
    Core::WindowContainer container(QStringLiteral("group"));
    QString error;
    // AGENT-GUARD: Fixture mutations must execute in Release builds. Never
    // place a state-changing expression inside Q_ASSERT, which compiles out.
    if (!container.addPage(QStringLiteral("page-active"),
                           QStringLiteral("leaf-a"),
                           QStringLiteral("a"), &error)) {
        qFatal("Could not create active task-identity page: %s", qPrintable(error));
    }
    if (!container.splitWindow({
        .targetWindowId = QStringLiteral("a"),
        .newWindowId = QStringLiteral("b"),
        .newLeafNodeId = QStringLiteral("leaf-b"),
        .splitNodeId = QStringLiteral("split-ab"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .position = Core::InsertPosition::Second,
    }, &error)) {
        qFatal("Could not split active task-identity page: %s", qPrintable(error));
    }
    if (!container.addPage(QStringLiteral("page-inactive"),
                           QStringLiteral("leaf-c"),
                           QStringLiteral("c"), &error)) {
        qFatal("Could not create inactive task-identity page: %s", qPrintable(error));
    }
    return container;
}

} // namespace

class HybridTaskIdentityPolicyTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesOnlyPreferredActivePageMember();
    void fallsBackDeterministicallyAndIgnoresExternalWindow();
    void pageActivationReassignsPrimary();
    void routesOnlyOwnedLifecycleEvents();
    void rejectsContainerWithoutActivePage();
};

void HybridTaskIdentityPolicyTest::exposesOnlyPreferredActivePageMember()
{
    const auto plan = HybridTaskIdentityPolicy::planContainer(
        tabbedSplitContainer(), QStringLiteral("b"));
    QVERIFY(plan.has_value());
    QCOMPARE(plan->primaryWindowId, QStringLiteral("b"));
    QCOMPARE(plan->members.size(), 3);

    const auto *first = plan->member(QStringLiteral("a"));
    const auto *primary = plan->member(QStringLiteral("b"));
    const auto *inactive = plan->member(QStringLiteral("c"));
    QVERIFY(first && primary && inactive);
    QVERIFY(first->activePage);
    QVERIFY(first->skipTaskbar);
    QVERIFY(first->skipSwitcher);
    QVERIFY(primary->primary);
    QVERIFY(!primary->skipTaskbar);
    QVERIFY(!primary->skipSwitcher);
    QVERIFY(!inactive->activePage);
    QVERIFY(inactive->skipTaskbar);
    QVERIFY(inactive->skipSwitcher);
}

void HybridTaskIdentityPolicyTest::fallsBackDeterministicallyAndIgnoresExternalWindow()
{
    const auto plan = HybridTaskIdentityPolicy::planContainer(
        tabbedSplitContainer(), QStringLiteral("dialog-not-in-topology"));
    QVERIFY(plan.has_value());
    QCOMPARE(plan->primaryWindowId, QStringLiteral("a"));
    QVERIFY(!plan->member(QStringLiteral("dialog-not-in-topology")));

    qsizetype exposed = 0;
    for (const auto &member : plan->members) {
        exposed += (!member.skipTaskbar && !member.skipSwitcher) ? 1 : 0;
    }
    QCOMPARE(exposed, qsizetype(1));
}

void HybridTaskIdentityPolicyTest::pageActivationReassignsPrimary()
{
    auto container = tabbedSplitContainer();
    QString error;
    QVERIFY2(container.activatePage(QStringLiteral("page-inactive"), &error),
             qPrintable(error));

    const auto plan = HybridTaskIdentityPolicy::planContainer(
        container, QStringLiteral("b"), &error);
    QVERIFY2(plan.has_value(), qPrintable(error));
    QCOMPARE(plan->primaryWindowId, QStringLiteral("c"));
    QVERIFY(plan->member(QStringLiteral("a"))->skipTaskbar);
    QVERIFY(plan->member(QStringLiteral("b"))->skipSwitcher);
    QVERIFY(!plan->member(QStringLiteral("c"))->skipTaskbar);
}

void HybridTaskIdentityPolicyTest::routesOnlyOwnedLifecycleEvents()
{
    const auto plan = HybridTaskIdentityPolicy::planContainer(
        tabbedSplitContainer(), QStringLiteral("b"));
    QVERIFY(plan.has_value());
    const auto *visible = plan->member(QStringLiteral("a"));
    const auto *inactive = plan->member(QStringLiteral("c"));

    const auto minimize = HybridTaskIdentityPolicy::decide(
        visible, TaskIdentityEvent::Minimized);
    QCOMPARE(minimize.action, TaskIdentityAction::MinimizeContainer);
    QCOMPARE(minimize.containerId, QStringLiteral("group"));
    QVERIFY(!minimize.hideBeforeAction);

    QVERIFY(!HybridTaskIdentityPolicy::decide(
        visible, TaskIdentityEvent::Activated).hasAction());
    QVERIFY(!HybridTaskIdentityPolicy::decide(
        visible, TaskIdentityEvent::Unminimized).hasAction());
    QVERIFY(!HybridTaskIdentityPolicy::decide(
        inactive, TaskIdentityEvent::Minimized).hasAction());
    QVERIFY(!HybridTaskIdentityPolicy::decide(
        nullptr, TaskIdentityEvent::Activated).hasAction());

    for (const auto event : {TaskIdentityEvent::Activated,
                             TaskIdentityEvent::Unminimized}) {
        const auto activate = HybridTaskIdentityPolicy::decide(inactive, event);
        QCOMPARE(activate.action, TaskIdentityAction::ActivatePage);
        QCOMPARE(activate.containerId, QStringLiteral("group"));
        QCOMPARE(activate.pageId, QStringLiteral("page-inactive"));
        QCOMPARE(activate.windowId, QStringLiteral("c"));
        QVERIFY(activate.hideBeforeAction);
    }
}

void HybridTaskIdentityPolicyTest::rejectsContainerWithoutActivePage()
{
    QString error;
    QVERIFY(!HybridTaskIdentityPolicy::planContainer(
        Core::WindowContainer(QStringLiteral("empty")), {}, &error));
    QVERIFY(error.contains(QStringLiteral("active page")));
}

QTEST_GUILESS_MAIN(HybridTaskIdentityPolicyTest)
#include "tst_hybridtaskidentitypolicy.moc"
