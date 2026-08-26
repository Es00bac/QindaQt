// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeaccessibility.h"
#include "hybridchromeaccessibilityregistry.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QAccessible>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

QVector<QAccessible::Event> *capturedEvents = nullptr;

void captureAccessibilityEvent(QAccessibleEvent *event)
{
    if (capturedEvents && event) {
        capturedEvents->append(event->type());
    }
}

class AccessibilityEventCapture final
{
public:
    explicit AccessibilityEventCapture(QVector<QAccessible::Event> *events)
        : m_previous(QAccessible::installUpdateHandler(captureAccessibilityEvent))
    {
        capturedEvents = events;
    }

    ~AccessibilityEventCapture()
    {
        capturedEvents = nullptr;
        QAccessible::installUpdateHandler(m_previous);
    }

private:
    QAccessible::UpdateHandler m_previous = nullptr;
};

HybridChrome::ChromeRenderPlan planWithActivePage(const QString &activePage)
{
    HybridChrome::ChromeLayoutRequest request;
    request.containerId = QStringLiteral("group");
    request.outerRect = QRectF(100, 80, 1000, 700);
    request.style = HybridChrome::ChromeStyle::qindaMacOS({});
    request.tabs = {
        {QStringLiteral("page-a"), QStringLiteral("Editor"),
         activePage == QStringLiteral("page-a")},
        {QStringLiteral("page-b"), QStringLiteral("Terminal"),
         activePage == QStringLiteral("page-b")},
    };
    request.members = {
        {QStringLiteral("window-a"), QStringLiteral("Editor"),
         QRectF(110, 158, 980, 612)},
    };
    QString error;
    auto plan = HybridChrome::ChromeLayoutEngine::build(request, &error);
    if (!plan) {
        qFatal("failed to build accessibility fixture: %s", qPrintable(error));
    }
    return *plan;
}

QMap<QString, QString> pageRepresentatives()
{
    return {{QStringLiteral("page-a"), QStringLiteral("window-a")},
            {QStringLiteral("page-b"), QStringLiteral("window-b")}};
}

} // namespace

class HybridChromeAccessibilityTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesNavigableRolesNamesAndCurrentState();
    void invokesTabsAndGroupControlsWithoutCoordinates();
    void preservesSemanticFocusAcrossPlanUpdates();
    void hiddenChromeIsInvisibleDisabledAndActionlessUntilShown();
    void registryOwnsOneStableRootPerLiveContainer();
    void rejectsInvalidPlansAndDisablesUnavailableActions();
};

void HybridChromeAccessibilityTest::hiddenChromeIsInvisibleDisabledAndActionlessUntilShown()
{
    QVector<HybridSemanticRequest> requests;
    HybridChromeAccessibilityAdapter adapter({
        .dispatch = [&](const HybridSemanticRequest &request, QString *) {
            requests.append(request);
            return true;
        },
    });
    const auto plan = planWithActivePage(QStringLiteral("page-a"));
    const auto representatives = pageRepresentatives();
    QString error;
    QVERIFY(adapter.updatePlan(plan, representatives, &error));
    const auto tabId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-a"));

    QVector<QAccessible::Event> events;
    AccessibilityEventCapture capture(&events);
    QVERIFY(adapter.updatePlan(plan, representatives, false, &error));
    auto *root = adapter.interfaceForNode(adapter.rootNodeId());
    auto *tab = adapter.interfaceForNode(tabId);
    QVERIFY(root && tab);
    QVERIFY(root->state().invisible);
    QVERIFY(root->state().disabled);
    QVERIFY(tab->state().invisible);
    QVERIFY(tab->state().disabled);
    QVERIFY(!tab->state().focusable);
    QCOMPARE(root->rect(), QRect{});
    QVERIFY(tab->actionInterface()->actionNames().isEmpty());
    QVERIFY(!adapter.invoke(tabId, &error));
    QVERIFY(error.contains(QStringLiteral("not invokable")));
    QVERIFY(requests.isEmpty());
    QVERIFY(events.contains(QAccessible::ObjectHide));

    events.clear();
    QVERIFY(adapter.updatePlan(plan, representatives, true, &error));
    root = adapter.interfaceForNode(adapter.rootNodeId());
    tab = adapter.interfaceForNode(tabId);
    QVERIFY(root && tab);
    QVERIFY(!root->state().invisible);
    QVERIFY(!root->state().disabled);
    QVERIFY(!tab->state().invisible);
    QVERIFY(!tab->state().disabled);
    QVERIFY(tab->state().focusable);
    QVERIFY(tab->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
    QVERIFY(events.contains(QAccessible::ObjectShow));
    tab->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    QCOMPARE(requests.size(), 1);
}

void HybridChromeAccessibilityTest::exposesNavigableRolesNamesAndCurrentState()
{
    HybridChromeAccessibilityAdapter adapter({
        .dispatch = [](const HybridSemanticRequest &, QString *) { return true; },
    });
    QString error;
    QVERIFY2(adapter.updatePlan(planWithActivePage(QStringLiteral("page-a")),
                                pageRepresentatives(), &error),
             qPrintable(error));

    auto *root = adapter.interfaceForNode(adapter.rootNodeId());
    QVERIFY(root);
    QCOMPARE(root->role(), QAccessible::Grouping);
    QVERIFY(root->text(QAccessible::Name).contains(QStringLiteral("Editor")));
    QCOMPARE(root->childCount(), 4); // three traffic lights plus the tab list

    auto *tabList = adapter.interfaceForNode(
        HybridChromeAccessibilityAdapter::tabListNodeId(QStringLiteral("group")));
    QVERIFY(tabList);
    QCOMPARE(tabList->role(), QAccessible::PageTabList);
    QCOMPARE(tabList->childCount(), 2);

    const auto firstId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-a"));
    const auto secondId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-b"));
    auto *first = adapter.interfaceForNode(firstId);
    auto *second = adapter.interfaceForNode(secondId);
    QVERIFY(first && second);
    QCOMPARE(first->role(), QAccessible::PageTab);
    QCOMPARE(first->text(QAccessible::Name), QStringLiteral("Editor"));
    QVERIFY(first->state().selected);
    QVERIFY(first->state().active);
    QVERIFY(first->state().focused);
    QCOMPARE(first->text(QAccessible::Value), QStringLiteral("current"));
    QVERIFY(!second->state().selected);
    QVERIFY(!second->state().active);

    // Qinda macOS paints tabs right-to-left, but assistive traversal follows the
    // stable logical vector exactly like keyboard page cycling and persistence.
    QCOMPARE(tabList->child(0)->text(QAccessible::Identifier), firstId);
    QCOMPARE(tabList->child(1)->text(QAccessible::Identifier), secondId);
    QCOMPARE(tabList->indexOfChild(first), 0);
    QCOMPARE(tabList->indexOfChild(second), 1);
    QCOMPARE(root->focusChild()->text(QAccessible::Identifier), firstId);
}

void HybridChromeAccessibilityTest::invokesTabsAndGroupControlsWithoutCoordinates()
{
    QVector<HybridSemanticRequest> requests;
    HybridChromeAccessibilityAdapter adapter({
        .dispatch = [&](const HybridSemanticRequest &request, QString *) {
            requests.append(request);
            return true;
        },
    });
    QString error;
    QVERIFY2(adapter.updatePlan(planWithActivePage(QStringLiteral("page-a")),
                                pageRepresentatives(), &error),
             qPrintable(error));

    const auto tabId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-b"));
    auto *tab = adapter.interfaceForNode(tabId);
    QVERIFY(tab);
    auto *tabActions = tab->actionInterface();
    QVERIFY(tabActions);
    QVERIFY(tabActions->actionNames().contains(QAccessibleActionInterface::pressAction()));
    QVERIFY(tabActions->actionNames().contains(
        HybridChromeAccessibilityAdapter::dockPageActionName()));
    QVERIFY(tabActions->actionNames().contains(
        HybridChromeAccessibilityAdapter::reorderPagePreviousActionName()));
    QVERIFY(!tabActions->localizedActionName(
                HybridChromeAccessibilityAdapter::dockPageActionName()).isEmpty());
    QVERIFY(tabActions->localizedActionDescription(
                HybridChromeAccessibilityAdapter::dockPageActionName())
                .contains(QStringLiteral("detach")));
    tabActions->doAction(QAccessibleActionInterface::pressAction());
    QCOMPARE(requests.size(), 1);
    QCOMPARE(requests.constLast().kind, HybridSemanticRequestKind::ActivatePage);
    QCOMPARE(requests.constLast().pageId, QStringLiteral("page-b"));

    tabActions->doAction(HybridChromeAccessibilityAdapter::dockPageActionName());
    QCOMPARE(requests.size(), 2);
    QCOMPARE(requests.constLast().kind,
             HybridSemanticRequestKind::BeginPageDock);
    QCOMPARE(requests.constLast().dockSource.kind, HybridInput::HitKind::Tab);
    QCOMPARE(requests.constLast().dockSource.memberId, QStringLiteral("window-b"));

    tabActions->doAction(
        HybridChromeAccessibilityAdapter::reorderPagePreviousActionName());
    QCOMPARE(requests.size(), 3);
    QCOMPARE(requests.constLast().kind, HybridSemanticRequestKind::ReorderPage);
    QCOMPARE(requests.constLast().pageId, QStringLiteral("page-b"));
    QCOMPARE(requests.constLast().destinationPageIndex, 0);

    const auto firstTabId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-a"));
    auto *firstTab = adapter.interfaceForNode(firstTabId);
    QVERIFY(firstTab);
    QVERIFY(firstTab->actionInterface()->actionNames().contains(
        HybridChromeAccessibilityAdapter::reorderPageNextActionName()));
    firstTab->actionInterface()->doAction(
        HybridChromeAccessibilityAdapter::reorderPageNextActionName());
    QCOMPARE(requests.size(), 4);
    QCOMPARE(requests.constLast().kind, HybridSemanticRequestKind::ReorderPage);
    QCOMPARE(requests.constLast().pageId, QStringLiteral("page-a"));
    QCOMPARE(requests.constLast().destinationPageIndex, 1);

    const auto closeId = HybridChromeAccessibilityAdapter::actionNodeId(
        QStringLiteral("group"), HybridChrome::WindowAction::Close);
    auto *close = adapter.interfaceForNode(closeId);
    QVERIFY(close);
    QCOMPARE(close->role(), QAccessible::Button);
    QVERIFY(close->text(QAccessible::Name).contains(QStringLiteral("Close")));
    close->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    QCOMPARE(requests.size(), 5);
    QCOMPARE(requests.constLast().kind,
             HybridSemanticRequestKind::GroupWindowAction);
    QCOMPARE(requests.constLast().windowAction,
             std::optional(HybridChrome::WindowAction::Close));
}

void HybridChromeAccessibilityTest::preservesSemanticFocusAcrossPlanUpdates()
{
    HybridChromeAccessibilityAdapter adapter({
        .dispatch = [](const HybridSemanticRequest &, QString *) { return true; },
    });
    QString error;
    QVERIFY(adapter.updatePlan(planWithActivePage(QStringLiteral("page-a")),
                               pageRepresentatives(), &error));
    const auto secondId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-b"));
    auto *second = adapter.interfaceForNode(secondId);
    QVERIFY(second);
    second->actionInterface()->doAction(QAccessibleActionInterface::setFocusAction());
    QCOMPARE(adapter.focusedNodeId(), secondId);
    QVERIFY(second->state().focused);

    QVERIFY(adapter.updatePlan(planWithActivePage(QStringLiteral("page-b")),
                               pageRepresentatives(), &error));
    second = adapter.interfaceForNode(secondId);
    QVERIFY(second);
    QCOMPARE(adapter.focusedNodeId(), secondId);
    QVERIFY(second->state().focused);
    QVERIFY(second->state().selected);
    QVERIFY(second->state().active);
}

void HybridChromeAccessibilityTest::registryOwnsOneStableRootPerLiveContainer()
{
    HybridChromeAccessibilityRegistry registry({
        .dispatch = [](const HybridSemanticRequest &, QString *) { return true; },
    });
    QMap<QString, HybridChrome::ChromeRenderPlan> plans;
    plans.insert(QStringLiteral("group"),
                 planWithActivePage(QStringLiteral("page-a")));
    auto other = planWithActivePage(QStringLiteral("page-b"));
    other.containerId = QStringLiteral("other");
    plans.insert(other.containerId, other);
    const QMap<QString, QMap<QString, QString>> representatives{
        {QStringLiteral("group"), pageRepresentatives()},
        {QStringLiteral("other"), pageRepresentatives()},
    };
    const auto planLookup = [&plans](const QString &containerId)
        -> std::optional<HybridChrome::ChromeRenderPlan> {
        const auto match = plans.constFind(containerId);
        return match == plans.cend()
            ? std::nullopt
            : std::optional<HybridChrome::ChromeRenderPlan>(*match);
    };
    const auto representativeLookup = [&representatives](const QString &containerId) {
        return representatives.value(containerId);
    };
    QMap<QString, bool> visibility{{QStringLiteral("group"), true},
                                   {QStringLiteral("other"), false}};
    const auto visibilityLookup = [&visibility](const QString &containerId) {
        return visibility.value(containerId, false);
    };

    QString error;
    QVERIFY2(registry.synchronize({QStringLiteral("group"), QStringLiteral("other")},
                                  planLookup, representativeLookup,
                                  visibilityLookup, &error),
             qPrintable(error));
    QCOMPARE(registry.containerIds(),
             QStringList({QStringLiteral("group"), QStringLiteral("other")}));
    QVERIFY(registry.adapter(QStringLiteral("group")));
    QVERIFY(registry.adapter(QStringLiteral("other")));
    QVERIFY(registry.adapter(QStringLiteral("other"))
                ->interfaceForNode(registry.adapter(QStringLiteral("other"))->rootNodeId())
                ->state().invisible);

    const auto focusedId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-b"));
    QVERIFY(registry.adapter(QStringLiteral("group"))->setFocusedNode(focusedId, &error));
    plans[QStringLiteral("group")] = planWithActivePage(QStringLiteral("page-b"));
    QVERIFY(registry.synchronize({QStringLiteral("group")}, planLookup,
                                 representativeLookup, visibilityLookup, &error));
    QCOMPARE(registry.containerIds(), QStringList({QStringLiteral("group")}));
    QVERIFY(!registry.adapter(QStringLiteral("other")));
    QCOMPARE(registry.adapter(QStringLiteral("group"))->focusedNodeId(), focusedId);

    QVERIFY(!registry.synchronize({QStringLiteral("missing")}, planLookup,
                                  representativeLookup, visibilityLookup, &error));
    QVERIFY(error.contains(QStringLiteral("stale")));
    // Whole-snapshot preflight rejects without deleting the published root.
    QCOMPARE(registry.containerIds(), QStringList({QStringLiteral("group")}));
    registry.clear();
    QVERIFY(registry.containerIds().isEmpty());
}

void HybridChromeAccessibilityTest::rejectsInvalidPlansAndDisablesUnavailableActions()
{
    HybridChromeAccessibilityAdapter adapter;
    QString error;
    auto plan = planWithActivePage(QStringLiteral("page-a"));
    QVERIFY(adapter.updatePlan(plan, &error));
    const auto tabId = HybridChromeAccessibilityAdapter::tabNodeId(
        QStringLiteral("group"), QStringLiteral("page-a"));
    auto *tab = adapter.interfaceForNode(tabId);
    QVERIFY(tab);
    QVERIFY(tab->state().disabled);
    QVERIFY(tab->actionInterface()->actionNames().isEmpty());
    QVERIFY(!adapter.invoke(tabId, &error));
    QVERIFY(error.contains(QStringLiteral("not invokable")));

    plan.tabs[1].active = true;
    QVERIFY(!adapter.updatePlan(plan, &error));
    QVERIFY(error.contains(QStringLiteral("exactly one")));
    QVERIFY(adapter.interfaceForNode(tabId)); // rejected update is non-mutating

    adapter.clear();
    QVERIFY(adapter.rootNodeId().isEmpty());
    QVERIFY(adapter.nodeIds().isEmpty());
}

QTEST_MAIN(HybridChromeAccessibilityTest)
#include "tst_hybridchromeaccessibility.moc"
