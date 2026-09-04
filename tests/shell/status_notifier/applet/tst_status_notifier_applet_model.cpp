// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h>

#include <QtTest>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifierApplet;

namespace
{

TrayItemPresentation makeItem(const QString &identity,
                              const QString &uniqueName,
                              const QString &objectPath,
                              quint64 generation)
{
    TrayItemPresentation item;
    item.owner = OwnerKey { uniqueName, objectPath, generation };
    item.identity = identity;
    item.accessibleName = QStringLiteral("Name %1").arg(identity);
    item.accessibleDescription = QStringLiteral("Description %1").arg(identity);
    item.accessibleStatusText = QStringLiteral("active");
    item.keyboardActions = {
        KeyboardAction { RequestKind::Activate, QStringLiteral("Enter or Space") },
        KeyboardAction { RequestKind::ContextMenu, QStringLiteral("Shift+F10 or Menu key") },
        KeyboardAction { RequestKind::SecondaryActivate, QString() },
    };
    return item;
}

ItemDescriptor makeDescriptor(const QString &identity,
                              const QString &title,
                              ItemStatus status,
                              MenuPayload menu = {})
{
    ItemDescriptor descriptor;
    descriptor.identity = identity;
    descriptor.title = title;
    descriptor.status = status;
    descriptor.menu = std::move(menu);
    return descriptor;
}

MenuEntry menuItem(MenuEntry::Kind kind, qsizetype parentId, const QString &label)
{
    MenuEntry entry;
    entry.kind = kind;
    entry.parentId = parentId;
    entry.label = label;
    return entry;
}

} // namespace

class StatusNotifierAppletModelTests final : public QObject
{
    Q_OBJECT

private slots:
    void loadingAndEmptyPhases();
    void readyPhaseMapsRows();
    void degradedKeepsRowsAndDiagnostic();
    void readDeniedWithholdsEverything();
    void presentationCapIsTruthful();
    void missingDescriptorFailsClosed();
    void projectionIsDeterministic();
    void menuFlatteningPresentsStructure();
    void menuDepthCapTruncatesHostileChains();
    void menuDefenseDropsHostileEntries();
};

void StatusNotifierAppletModelTests::loadingAndEmptyPhases()
{
    TrayPresentation loading;
    loading.state = PresentationState::Loading;
    const auto loadingProjection = StatusNotifierAppletModel::project(loading, {}, true);
    QCOMPARE(loadingProjection.phase, AppletPhase::Loading);
    QVERIFY(loadingProjection.rows.isEmpty());
    QCOMPARE(loadingProjection.phaseReason, QString());

    TrayPresentation empty;
    empty.state = PresentationState::Empty;
    const auto emptyProjection = StatusNotifierAppletModel::project(empty, {}, true);
    QCOMPARE(emptyProjection.phase, AppletPhase::Empty);
    QVERIFY(emptyProjection.rows.isEmpty());
}

void StatusNotifierAppletModelTests::readyPhaseMapsRows()
{
    MenuPayload menu;
    menu.entries = { menuItem(MenuEntry::Kind::Item, -1, QStringLiteral("Open")) };

    TrayPresentation presentation;
    presentation.state = PresentationState::Ready;
    presentation.items = {
        makeItem(QStringLiteral("org.qindaqt.one"), QStringLiteral(":1.10"),
                 QStringLiteral("/StatusNotifierItem"), 7),
        makeItem(QStringLiteral("org.qindaqt.two"), QStringLiteral(":1.11"),
                 QStringLiteral("/"), 8),
    };
    const QList<ItemDescriptor> descriptors = {
        makeDescriptor(QStringLiteral("org.qindaqt.one"), QStringLiteral("One"),
                       ItemStatus::NeedsAttention, menu),
        makeDescriptor(QStringLiteral("org.qindaqt.two"), QStringLiteral("Two"),
                       ItemStatus::Active),
    };

    const auto projection = StatusNotifierAppletModel::project(presentation, descriptors, true);
    QCOMPARE(projection.phase, AppletPhase::Ready);
    QCOMPARE(projection.rows.size(), 2);
    QCOMPARE(projection.presentedCount, 2);
    QCOMPARE(projection.overflowCount, 0);
    QVERIFY(projection.overflowText.isEmpty());

    const auto &first = projection.rows.at(0);
    QCOMPARE(first.uniqueName, QStringLiteral(":1.10"));
    QCOMPARE(first.objectPath, QStringLiteral("/StatusNotifierItem"));
    QCOMPARE(first.generation, 7u);
    QCOMPARE(first.identity, QStringLiteral("org.qindaqt.one"));
    QCOMPARE(first.title, QStringLiteral("One"));
    QCOMPARE(first.accessibleName, QStringLiteral("Name org.qindaqt.one"));
    QCOMPARE(first.accessibleStatusText, QStringLiteral("active"));
    QCOMPARE(first.needsAttention, true);
    QCOMPARE(first.active, false);
    QCOMPARE(first.hasMenu, true);
    QCOMPARE(first.menuEntryCount, 1);
    QCOMPARE(first.keyboardActivateText, QStringLiteral("Enter or Space"));
    QCOMPARE(first.keyboardContextMenuText, QStringLiteral("Shift+F10 or Menu key"));
    // S1 records secondary activation as pointer-only; the row says so.
    QCOMPARE(first.secondaryActivatePointerOnly, true);
    // The pure projection never carries rendered icons.
    QVERIFY(first.iconDataUrl.isEmpty());
    QCOMPARE(first.iconIsPlaceholder, false);

    const auto &second = projection.rows.at(1);
    QCOMPARE(second.title, QStringLiteral("Two"));
    QCOMPARE(second.active, true);
    QCOMPARE(second.needsAttention, false);
    QCOMPARE(second.hasMenu, false);
    QCOMPARE(second.menuEntryCount, 0);
}

void StatusNotifierAppletModelTests::degradedKeepsRowsAndDiagnostic()
{
    TrayPresentation presentation;
    presentation.state = PresentationState::Degraded;
    presentation.diagnostic = QStringLiteral("status-notifier-watcher-unavailable");
    presentation.items = {
        makeItem(QStringLiteral("org.qindaqt.lkg"), QStringLiteral(":1.12"),
                 QStringLiteral("/StatusNotifierItem"), 9),
    };
    const QList<ItemDescriptor> descriptors = {
        makeDescriptor(QStringLiteral("org.qindaqt.lkg"), QStringLiteral("Last known"),
                       ItemStatus::Active),
    };

    const auto projection = StatusNotifierAppletModel::project(presentation, descriptors, true);
    QCOMPARE(projection.phase, AppletPhase::Degraded);
    QCOMPARE(projection.phaseReason, QStringLiteral("status-notifier-watcher-unavailable"));
    // Last-known-good rows stay visible and actionable.
    QCOMPARE(projection.rows.size(), 1);
    QCOMPARE(projection.rows.constFirst().title, QStringLiteral("Last known"));
}

void StatusNotifierAppletModelTests::readDeniedWithholdsEverything()
{
    TrayPresentation presentation;
    presentation.state = PresentationState::Ready;
    presentation.items = {
        makeItem(QStringLiteral("org.qindaqt.one"), QStringLiteral(":1.10"),
                 QStringLiteral("/StatusNotifierItem"), 7),
    };
    const QList<ItemDescriptor> descriptors = {
        makeDescriptor(QStringLiteral("org.qindaqt.one"), QStringLiteral("One"),
                       ItemStatus::Active),
    };

    const auto projection = StatusNotifierAppletModel::project(presentation, descriptors, false);
    QCOMPARE(projection.phase, AppletPhase::Unavailable);
    QCOMPARE(projection.phaseReason, QLatin1String(kReasonStatusItemsReadNotGranted));
    QVERIFY(projection.rows.isEmpty());
    QCOMPARE(projection.presentedCount, 0);
    QCOMPARE(projection.overflowCount, 0);
    QVERIFY(projection.overflowText.isEmpty());
}

void StatusNotifierAppletModelTests::presentationCapIsTruthful()
{
    TrayPresentation presentation;
    presentation.state = PresentationState::Ready;
    QList<ItemDescriptor> descriptors;
    for (int i = 0; i < 30; ++i) {
        const QString identity = QStringLiteral("org.qindaqt.item%1").arg(i);
        presentation.items.append(makeItem(identity, QStringLiteral(":1.%1").arg(100 + i),
                                           QStringLiteral("/StatusNotifierItem"),
                                           quint64(50 + i)));
        descriptors.append(makeDescriptor(identity, QStringLiteral("Item %1").arg(i),
                                          ItemStatus::Active));
    }

    const auto projection = StatusNotifierAppletModel::project(presentation, descriptors, true);
    QCOMPARE(projection.phase, AppletPhase::Ready);
    QCOMPARE(projection.rows.size(), kMaxPresentedItems);
    QCOMPARE(projection.presentedCount, int(kMaxPresentedItems));
    QCOMPARE(projection.overflowCount, 6);
    QCOMPARE(projection.overflowText, QStringLiteral("6 more items"));
    // Presentation order is the S1 stable order, truncated, never resorted.
    QCOMPARE(projection.rows.constFirst().identity, QStringLiteral("org.qindaqt.item0"));
    QCOMPARE(projection.rows.constLast().identity,
             QStringLiteral("org.qindaqt.item%1").arg(kMaxPresentedItems - 1));

    TrayPresentation one25 = presentation;
    one25.items.resize(int(kMaxPresentedItems) + 1);
    StatusNotifierAppletTexts texts;
    const auto singular = StatusNotifierAppletModel::project(one25, descriptors, true, texts);
    QCOMPARE(singular.overflowCount, 1);
    QCOMPARE(singular.overflowText, QStringLiteral("1 more item"));
}

void StatusNotifierAppletModelTests::missingDescriptorFailsClosed()
{
    TrayPresentation presentation;
    presentation.state = PresentationState::Ready;
    presentation.items = {
        makeItem(QStringLiteral("org.qindaqt.ghost"), QStringLiteral(":1.20"),
                 QStringLiteral("/StatusNotifierItem"), 11),
    };

    const auto projection = StatusNotifierAppletModel::project(presentation, {}, true);
    QCOMPARE(projection.rows.size(), 1);
    const auto &row = projection.rows.constFirst();
    QCOMPARE(row.title, QStringLiteral("org.qindaqt.ghost"));
    QCOMPARE(row.hasMenu, false);
    QCOMPARE(row.menuEntryCount, 0);
    QCOMPARE(row.needsAttention, false);
    QCOMPARE(row.active, false);
}

void StatusNotifierAppletModelTests::projectionIsDeterministic()
{
    TrayPresentation presentation;
    presentation.state = PresentationState::Ready;
    presentation.items = {
        makeItem(QStringLiteral("org.qindaqt.a"), QStringLiteral(":1.30"),
                 QStringLiteral("/StatusNotifierItem"), 21),
        makeItem(QStringLiteral("org.qindaqt.b"), QStringLiteral(":1.31"),
                 QStringLiteral("/Two"), 22),
    };
    const QList<ItemDescriptor> descriptors = {
        makeDescriptor(QStringLiteral("org.qindaqt.a"), QStringLiteral("A"),
                       ItemStatus::Active),
        makeDescriptor(QStringLiteral("org.qindaqt.b"), QStringLiteral("B"),
                       ItemStatus::Passive),
    };

    const auto first = StatusNotifierAppletModel::project(presentation, descriptors, true);
    const auto second = StatusNotifierAppletModel::project(presentation, descriptors, true);
    QCOMPARE(first, second);
}

void StatusNotifierAppletModelTests::menuFlatteningPresentsStructure()
{
    MenuPayload menu;
    menu.entries = {
        menuItem(MenuEntry::Kind::Item, -1, QStringLiteral("Open")),          // 0
        menuItem(MenuEntry::Kind::Separator, -1, QString()),                  // 1
        menuItem(MenuEntry::Kind::SubMenu, -1, QStringLiteral("Recent")),     // 2
        menuItem(MenuEntry::Kind::Item, 2, QStringLiteral("Alpha")),          // 3
        menuItem(MenuEntry::Kind::Item, 2, QStringLiteral("Beta")),           // 4
        menuItem(MenuEntry::Kind::SubMenu, 2, QStringLiteral("Deeper")),      // 5
        menuItem(MenuEntry::Kind::Item, 5, QStringLiteral("Leaf")),           // 6
    };
    menu.entries[4].enabled = false;

    const auto rows = StatusNotifierAppletModel::projectMenu(menu);
    QCOMPARE(rows.size(), 7);
    QCOMPARE(rows.at(0).kind, QStringLiteral("item"));
    QCOMPARE(rows.at(0).depth, 0);
    QCOMPARE(rows.at(0).hasChildren, false);
    QCOMPARE(rows.at(1).kind, QStringLiteral("separator"));
    QCOMPARE(rows.at(2).kind, QStringLiteral("submenu"));
    QCOMPARE(rows.at(2).hasChildren, true);
    QCOMPARE(rows.at(3).depth, 1);
    QCOMPARE(rows.at(3).label, QStringLiteral("Alpha"));
    QCOMPARE(rows.at(4).enabled, false);
    QCOMPARE(rows.at(5).kind, QStringLiteral("submenu"));
    QCOMPARE(rows.at(5).hasChildren, true);
    QCOMPARE(rows.at(6).depth, 2);
    QCOMPARE(rows.at(6).label, QStringLiteral("Leaf"));

    // Same input, same flattened output.
    QCOMPARE(StatusNotifierAppletModel::projectMenu(menu), rows);
}

void StatusNotifierAppletModelTests::menuDepthCapTruncatesHostileChains()
{
    // A hand-built 6-deep submenu chain: the S1 admission gate would never
    // admit this, but the applet re-enforces the depth bound itself.
    MenuPayload menu;
    for (int depth = 0; depth < 6; ++depth) {
        menu.entries.append(menuItem(MenuEntry::Kind::SubMenu, depth - 1,
                                     QStringLiteral("Level %1").arg(depth)));
    }
    menu.entries.append(menuItem(MenuEntry::Kind::Item, 5, QStringLiteral("Too deep")));

    const auto rows = StatusNotifierAppletModel::projectMenu(menu, 4);
    QCOMPARE(rows.size(), 4);
    for (int depth = 0; depth < 4; ++depth) {
        QCOMPARE(rows.at(depth).depth, depth);
        QCOMPARE(rows.at(depth).label, QStringLiteral("Level %1").arg(depth));
    }

    QVERIFY(StatusNotifierAppletModel::projectMenu(menu, 0).isEmpty());
}

void StatusNotifierAppletModelTests::menuDefenseDropsHostileEntries()
{
    MenuPayload menu;
    menu.entries = {
        menuItem(MenuEntry::Kind::Item, -1, QStringLiteral("Visible")),       // 0
        menuItem(MenuEntry::Kind::Item, -1, QStringLiteral("Hidden")),        // 1
        menuItem(MenuEntry::Kind::Separator, -1, QString()),                  // 2
        menuItem(MenuEntry::Kind::Item, 2, QStringLiteral("OrphanKind")),     // 3: separator parent
        menuItem(MenuEntry::Kind::Item, 9, QStringLiteral("OrphanForward")),  // 4: forward ref
        menuItem(MenuEntry::Kind::SubMenu, -1, QStringLiteral("HiddenSub")),  // 5
        menuItem(MenuEntry::Kind::Item, 5, QStringLiteral("SubChild")),       // 6
    };
    menu.entries[1].visible = false;
    menu.entries[5].visible = false;

    const auto rows = StatusNotifierAppletModel::projectMenu(menu);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).label, QStringLiteral("Visible"));
    QCOMPARE(rows.at(1).kind, QStringLiteral("separator"));
}

QTEST_GUILESS_MAIN(StatusNotifierAppletModelTests)
#include "tst_status_notifier_applet_model.moc"
