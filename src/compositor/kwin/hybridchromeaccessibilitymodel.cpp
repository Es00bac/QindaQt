// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeaccessibility.h"
#include "hybridchromeaccessibility_p.h"

#include <QAccessible>
#include <QSet>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

QString actionName(HybridChrome::WindowAction action)
{
    using enum HybridChrome::WindowAction;
    switch (action) {
    case Close:
        return QStringLiteral("Close window group");
    case Minimize:
        return QStringLiteral("Minimize window group");
    case Maximize:
        return QStringLiteral("Maximize window group");
    case Restore:
        return QStringLiteral("Restore window group");
    }
    return QStringLiteral("Window group action");
}

void appendTabNodes(const HybridChrome::ChromeRenderPlan &plan,
                    const QString &groupId,
                    const QMap<QString, QString> &tabRepresentatives,
                    bool actionsAvailable,
                    bool visible,
                    QVector<NodeData> *specs)
{
    const QString listId = HybridChromeAccessibilityAdapter::tabListNodeId(
        plan.containerId);
    specs->append({
        .id = listId,
        .parentId = groupId,
        .name = QStringLiteral("Window group pages"),
        .description = QStringLiteral("Pages in stable logical order"),
        .rect = plan.tabStrip.toAlignedRect(),
        .role = NodeRole::TabList,
        .current = false,
        .selected = false,
        .focused = false,
        .enabled = visible,
        .visible = visible,
        .actions = {},
    });
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        const auto &tab = plan.tabs[index];
        QVector<NodeData::Action> nodeActions;
        if (actionsAvailable && visible) {
            nodeActions.append({
                QAccessibleActionInterface::pressAction(),
                {.kind = HybridSemanticRequestKind::ActivatePage,
                 .containerId = plan.containerId,
                 .pageId = tab.tabId,
                 .destinationPageIndex = -1,
                 .dockSource = {},
                 .windowAction = std::nullopt},
            });
            HybridInput::HitTarget source{
                HybridInput::HitKind::Tab, plan.containerId,
                tabRepresentatives.value(tab.tabId), {}};
            source.pageId = tab.tabId;
            nodeActions.append({
                HybridChromeAccessibilityAdapter::dockPageActionName(),
                {.kind = HybridSemanticRequestKind::BeginPageDock,
                 .containerId = plan.containerId,
                 .pageId = tab.tabId,
                 .destinationPageIndex = -1,
                 .dockSource = std::move(source),
                 .windowAction = std::nullopt},
            });
            if (index > 0) {
                nodeActions.append({
                    HybridChromeAccessibilityAdapter::reorderPagePreviousActionName(),
                    {.kind = HybridSemanticRequestKind::ReorderPage,
                     .containerId = plan.containerId,
                     .pageId = tab.tabId,
                     .destinationPageIndex = index - 1,
                     .dockSource = {},
                     .windowAction = std::nullopt},
                });
            }
            if (index + 1 < plan.tabs.size()) {
                nodeActions.append({
                    HybridChromeAccessibilityAdapter::reorderPageNextActionName(),
                    {.kind = HybridSemanticRequestKind::ReorderPage,
                     .containerId = plan.containerId,
                     .pageId = tab.tabId,
                     .destinationPageIndex = index + 1,
                     .dockSource = {},
                     .windowAction = std::nullopt},
                });
            }
        }
        specs->append({
            .id = HybridChromeAccessibilityAdapter::tabNodeId(
                plan.containerId, tab.tabId),
            .parentId = listId,
            .name = tab.title.isEmpty() ? QStringLiteral("Untitled page") : tab.title,
            .description = tab.active ? QStringLiteral("Current window group page")
                                      : QStringLiteral("Window group page"),
            .rect = tab.rect.toAlignedRect(),
            .role = NodeRole::Tab,
            .current = tab.active,
            .selected = tab.active,
            .focused = false,
            .enabled = actionsAvailable && visible,
            .visible = visible,
            .actions = std::move(nodeActions),
        });
    }
}

} // namespace

QString actionToken(HybridChrome::WindowAction action)
{
    using enum HybridChrome::WindowAction;
    switch (action) {
    case Close:
        return QStringLiteral("close");
    case Minimize:
        return QStringLiteral("minimize");
    case Maximize:
        return QStringLiteral("maximize");
    case Restore:
        return QStringLiteral("restore");
    }
    return QStringLiteral("unknown");
}

QVector<NodeData> buildNodeSpecs(const HybridChrome::ChromeRenderPlan &plan,
                                 const QMap<QString, QString> &tabRepresentatives,
                                 bool actionsAvailable,
                                 bool visible,
                                 QString *error)
{
    if (error) {
        error->clear();
    }
    if (plan.containerId.isEmpty() || !plan.outerFrame.isValid()) {
        fail(error, QStringLiteral("accessible chrome needs a container and valid frame"));
        return {};
    }
    QSet<HybridChrome::WindowAction> actions;
    QSet<QString> pages;
    qsizetype activeTabs = 0;
    for (const auto &button : plan.buttons) {
        if (!button.rect.isValid() || actions.contains(button.action)) {
            fail(error, QStringLiteral("accessible chrome has duplicate or invalid controls"));
            return {};
        }
        actions.insert(button.action);
    }
    for (const auto &tab : plan.tabs) {
        if (tab.tabId.isEmpty() || !tab.rect.isValid() || pages.contains(tab.tabId)) {
            fail(error, QStringLiteral("accessible chrome has duplicate or invalid tabs"));
            return {};
        }
        pages.insert(tab.tabId);
        activeTabs += tab.active ? 1 : 0;
    }
    if ((!plan.tabs.isEmpty() && activeTabs != 1)
        || (plan.tabs.isEmpty() && activeTabs != 0)) {
        fail(error, QStringLiteral("accessible chrome needs exactly one current tab"));
        return {};
    }
    if (actionsAvailable && !plan.tabs.isEmpty()) {
        if (tabRepresentatives.size() != plan.tabs.size()) {
            fail(error, QStringLiteral("accessible chrome needs one representative per tab"));
            return {};
        }
        for (const auto &tab : plan.tabs) {
            if (tabRepresentatives.value(tab.tabId).isEmpty()) {
                fail(error, QStringLiteral("accessible tab '%1' has no member representative")
                                .arg(tab.tabId));
                return {};
            }
        }
    }

    QVector<NodeData> specs;
    const QString groupId = HybridChromeAccessibilityAdapter::groupNodeId(
        plan.containerId);
    QString currentTitle;
    for (const auto &tab : plan.tabs) {
        if (tab.active) {
            currentTitle = tab.title;
            break;
        }
    }
    specs.append({
        .id = groupId,
        .parentId = {},
        .name = currentTitle.isEmpty()
            ? QStringLiteral("QindaQt window group")
            : QStringLiteral("QindaQt window group: %1").arg(currentTitle),
        .description = QStringLiteral("Collapsed tiled window group"),
        .rect = plan.outerFrame.toAlignedRect(),
        .role = NodeRole::Group,
        .current = false,
        .selected = false,
        .focused = false,
        .enabled = visible,
        .visible = visible,
        .actions = {},
    });
    for (const auto &button : plan.buttons) {
        HybridSemanticRequest request{
            .kind = HybridSemanticRequestKind::GroupWindowAction,
            .containerId = plan.containerId,
            .pageId = {},
            .destinationPageIndex = -1,
            .dockSource = {},
            .windowAction = button.action,
        };
        QVector<NodeData::Action> nodeActions;
        if (actionsAvailable && visible) {
            nodeActions.append({QAccessibleActionInterface::pressAction(),
                                std::move(request)});
        }
        specs.append({
            .id = HybridChromeAccessibilityAdapter::actionNodeId(
                plan.containerId, button.action),
            .parentId = groupId,
            .name = actionName(button.action),
            .description = QStringLiteral("Applies to every window in this group"),
            .rect = button.rect.toAlignedRect(),
            .role = NodeRole::Button,
            .current = false,
            .selected = false,
            .focused = false,
            .enabled = actionsAvailable && visible,
            .visible = visible,
            .actions = std::move(nodeActions),
        });
    }
    appendTabNodes(plan, groupId, tabRepresentatives, actionsAvailable,
                   visible, &specs);
    return specs;
}

} // namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal
