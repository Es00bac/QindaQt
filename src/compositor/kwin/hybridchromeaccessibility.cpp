// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeaccessibility.h"
#include "hybridchromeaccessibility_p.h"

#include <QAccessible>
#include <QAccessibleStateChangeEvent>
#include <QHash>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using AccessibilityInternal::NodeData;
using AccessibilityInternal::NodeObject;
using AccessibilityInternal::NodeRole;

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

QAccessible::State changedState(const NodeData &before, const NodeData &after)
{
    QAccessible::State state;
    state.selected = before.selected != after.selected;
    state.focused = before.focused != after.focused;
    state.active = before.current != after.current;
    state.disabled = before.enabled != after.enabled;
    state.invisible = before.visible != after.visible;
    return state;
}

bool hasChangedState(const QAccessible::State &state)
{
    return state.selected || state.focused || state.active || state.disabled
        || state.invisible;
}

} // namespace

class HybridChromeAccessibilityAdapter::Private final
    : public AccessibilityInternal::Backend
{
public:
    explicit Private(HybridChromeAccessibleActions accessibleActions)
        : actions(std::move(accessibleActions))
    {
        AccessibilityInternal::retainAccessibleFactory();
    }

    ~Private() override
    {
        clear();
        AccessibilityInternal::releaseAccessibleFactory();
    }

    bool focusNode(const QString &nodeId, QString *error) override
    {
        if (error) {
            error->clear();
        }
        auto *target = nodes.value(nodeId);
        if (!target || target->data.actions.isEmpty() || !target->data.enabled
            || !target->data.visible) {
            return fail(error, QStringLiteral("accessible node is not focusable"));
        }
        if (focusedId == nodeId) {
            return true;
        }
        if (auto *prior = nodes.value(focusedId)) {
            prior->data.focused = false;
            QAccessible::State changed;
            changed.focused = true;
            QAccessibleStateChangeEvent event(prior, changed);
            QAccessible::updateAccessibility(&event);
        }
        focusedId = nodeId;
        target->data.focused = true;
        QAccessibleEvent event(target, QAccessible::Focus);
        QAccessible::updateAccessibility(&event);
        return true;
    }

    bool invokeNode(const QString &nodeId,
                    const QString &actionName,
                    QString *error) const override
    {
        if (error) {
            error->clear();
        }
        const auto *target = nodes.value(nodeId);
        if (!target || target->data.actions.isEmpty() || !target->data.enabled
            || !target->data.visible) {
            return fail(error, QStringLiteral("accessible node is not invokable"));
        }
        if (!actions.dispatch) {
            return fail(error, QStringLiteral("accessible semantic dispatcher is unavailable"));
        }
        const auto match = std::find_if(
            target->data.actions.cbegin(), target->data.actions.cend(),
            [&actionName](const auto &action) { return action.name == actionName; });
        if (match == target->data.actions.cend()) {
            return fail(error, QStringLiteral("accessible node has no action '%1'")
                                   .arg(actionName));
        }
        return actions.dispatch(match->request, error);
    }

    void clear() noexcept
    {
        if (root && root->data.visible) {
            QAccessibleEvent hidden(root.get(), QAccessible::ObjectHide);
            QAccessible::updateAccessibility(&hidden);
        }
        nodes.clear();
        root.reset();
        focusedId.clear();
    }

    static bool sameStructure(const QVector<NodeData> &specs,
                              const QHash<QString, NodeObject *> &objects)
    {
        if (specs.size() != objects.size()) {
            return false;
        }
        return std::all_of(specs.cbegin(), specs.cend(), [&objects](const auto &spec) {
            const auto *current = objects.value(spec.id);
            return current && current->data.parentId == spec.parentId;
        });
    }

    void rebuild(const QVector<NodeData> &specs)
    {
        const QString preservedFocus = focusedId;
        clear();
        if (specs.isEmpty()) {
            return;
        }
        root = std::make_unique<NodeObject>(specs.constFirst(), *this);
        nodes.insert(root->data.id, root.get());
        for (qsizetype index = 1; index < specs.size(); ++index) {
            const auto &spec = specs[index];
            auto *parentNode = nodes.value(spec.parentId);
            auto *object = new NodeObject(spec, *this, parentNode);
            object->semanticParent = parentNode;
            if (parentNode) {
                parentNode->semanticChildren.append(object);
            }
            nodes.insert(spec.id, object);
        }
        focusedId = nodes.contains(preservedFocus) ? preservedFocus : QString{};
        if (focusedId.isEmpty()) {
            for (const auto &spec : specs) {
                if (spec.role == NodeRole::Tab && spec.current) {
                    focusedId = spec.id;
                    break;
                }
            }
        }
        if (auto *focused = nodes.value(focusedId);
            focused && root->data.visible) {
            focused->data.focused = true;
        }
        // Announce children first so the root is complete when AT follows its
        // ObjectCreated event. This virtual tree creates no QWindow or input.
        for (qsizetype index = specs.size(); index-- > 0;) {
            QAccessibleEvent event(nodes.value(specs[index].id),
                                   QAccessible::ObjectCreated);
            QAccessible::updateAccessibility(&event);
        }
        if (root->data.visible) {
            QAccessibleEvent shown(root.get(), QAccessible::ObjectShow);
            QAccessible::updateAccessibility(&shown);
        }
    }

    void updateExisting(const QVector<NodeData> &specs)
    {
        const bool wasVisible = root && root->data.visible;
        QHash<QString, QVector<NodeObject *>> children;
        for (const auto &spec : specs) {
            auto *object = nodes.value(spec.id);
            auto updated = spec;
            updated.focused = spec.visible && spec.id == focusedId;
            const auto before = object->data;
            object->data = std::move(updated);
            if (before.name != object->data.name) {
                QAccessibleEvent event(object, QAccessible::NameChanged);
                QAccessible::updateAccessibility(&event);
            }
            if (before.rect != object->data.rect) {
                QAccessibleEvent event(object, QAccessible::LocationChanged);
                QAccessible::updateAccessibility(&event);
            }
            const auto states = changedState(before, object->data);
            if (hasChangedState(states)) {
                QAccessibleStateChangeEvent event(object, states);
                QAccessible::updateAccessibility(&event);
            }
            if (!spec.parentId.isEmpty()) {
                children[spec.parentId].append(object);
            }
        }
        for (auto iterator = nodes.cbegin(); iterator != nodes.cend(); ++iterator) {
            auto *object = iterator.value();
            const auto reordered = object->semanticChildren != children.value(iterator.key());
            object->semanticChildren = children.value(iterator.key());
            if (reordered) {
                QAccessibleEvent event(object, QAccessible::ObjectReorder);
                QAccessible::updateAccessibility(&event);
            }
        }
        const bool visible = root && root->data.visible;
        if (visible != wasVisible) {
            QAccessibleEvent event(root.get(), visible ? QAccessible::ObjectShow
                                                       : QAccessible::ObjectHide);
            QAccessible::updateAccessibility(&event);
        }
    }

    HybridChromeAccessibleActions actions;
    QString focusedId;
    std::unique_ptr<NodeObject> root;
    QHash<QString, NodeObject *> nodes;
};

HybridChromeAccessibilityAdapter::HybridChromeAccessibilityAdapter(
    HybridChromeAccessibleActions actions)
    : d(std::make_unique<Private>(std::move(actions)))
{
}

HybridChromeAccessibilityAdapter::~HybridChromeAccessibilityAdapter() = default;

bool HybridChromeAccessibilityAdapter::updatePlan(
    const HybridChrome::ChromeRenderPlan &plan, QString *error)
{
    return updatePlan(plan, {}, error);
}

bool HybridChromeAccessibilityAdapter::updatePlan(
    const HybridChrome::ChromeRenderPlan &plan,
    const QMap<QString, QString> &tabRepresentatives,
    QString *error)
{
    return updatePlan(plan, tabRepresentatives, true, error);
}

bool HybridChromeAccessibilityAdapter::updatePlan(
    const HybridChrome::ChromeRenderPlan &plan,
    const QMap<QString, QString> &tabRepresentatives,
    bool visible,
    QString *error)
{
    QString localError;
    auto *const destination = error ? error : &localError;
    const auto specs = AccessibilityInternal::buildNodeSpecs(
        plan, tabRepresentatives, bool(d->actions.dispatch), visible,
        destination);
    if (specs.isEmpty()) {
        return false;
    }
    if (d->sameStructure(specs, d->nodes)) {
        d->updateExisting(specs);
    } else {
        d->rebuild(specs);
    }
    return true;
}

void HybridChromeAccessibilityAdapter::clear() noexcept { d->clear(); }

QString HybridChromeAccessibilityAdapter::rootNodeId() const
{
    return d->root ? d->root->data.id : QString{};
}

QStringList HybridChromeAccessibilityAdapter::nodeIds() const
{
    auto result = d->nodes.keys();
    result.sort();
    return result;
}

QString HybridChromeAccessibilityAdapter::focusedNodeId() const
{
    return d->focusedId;
}

bool HybridChromeAccessibilityAdapter::setFocusedNode(
    const QString &nodeId, QString *error)
{
    return d->focusNode(nodeId, error);
}

bool HybridChromeAccessibilityAdapter::invoke(
    const QString &nodeId, QString *error) const
{
    return invoke(nodeId, QAccessibleActionInterface::pressAction(), error);
}

bool HybridChromeAccessibilityAdapter::invoke(
    const QString &nodeId, const QString &actionName, QString *error) const
{
    return d->invokeNode(nodeId, actionName, error);
}

QAccessibleInterface *HybridChromeAccessibilityAdapter::interfaceForNode(
    const QString &nodeId) const
{
    auto *object = d->nodes.value(nodeId);
    return object ? QAccessible::queryAccessibleInterface(object) : nullptr;
}

QString HybridChromeAccessibilityAdapter::groupNodeId(const QString &containerId)
{
    return QStringLiteral("group:%1").arg(containerId);
}

QString HybridChromeAccessibilityAdapter::tabListNodeId(const QString &containerId)
{
    return QStringLiteral("group:%1/tabs").arg(containerId);
}

QString HybridChromeAccessibilityAdapter::tabNodeId(
    const QString &containerId, const QString &pageId)
{
    return QStringLiteral("group:%1/page:%2").arg(containerId, pageId);
}

QString HybridChromeAccessibilityAdapter::actionNodeId(
    const QString &containerId, HybridChrome::WindowAction action)
{
    return QStringLiteral("group:%1/action:%2")
        .arg(containerId, AccessibilityInternal::actionToken(action));
}

QString HybridChromeAccessibilityAdapter::dockPageActionName()
{
    return QStringLiteral("qindaqtDockPage");
}

QString HybridChromeAccessibilityAdapter::reorderPagePreviousActionName()
{
    return QStringLiteral("qindaqtReorderPagePrevious");
}

QString HybridChromeAccessibilityAdapter::reorderPageNextActionName()
{
    return QStringLiteral("qindaqtReorderPageNext");
}

} // namespace QindaQt::Compositor::KWinIntegration
