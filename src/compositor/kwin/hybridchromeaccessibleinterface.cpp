// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeaccessibility.h"
#include "hybridchromeaccessibility_p.h"

#include <QAccessible>
#include <QAccessibleObject>

namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal {
namespace {

QAccessible::Role accessibleRole(NodeRole role)
{
    switch (role) {
    case NodeRole::Group:
        return QAccessible::Grouping;
    case NodeRole::TabList:
        return QAccessible::PageTabList;
    case NodeRole::Tab:
        return QAccessible::PageTab;
    case NodeRole::Button:
        return QAccessible::Button;
    }
    return QAccessible::NoRole;
}

class NodeInterface final : public QAccessibleObject,
                            public QAccessibleActionInterface
{
public:
    explicit NodeInterface(NodeObject *node)
        : QAccessibleObject(node)
    {
    }
    ~NodeInterface() override = default;

    [[nodiscard]] NodeObject *node() const
    {
        return dynamic_cast<NodeObject *>(object());
    }

    QAccessibleInterface *parent() const override
    {
        const auto *value = node();
        return value && value->semanticParent
            ? QAccessible::queryAccessibleInterface(value->semanticParent) : nullptr;
    }

    QAccessibleInterface *child(int index) const override
    {
        const auto *value = node();
        return value && index >= 0 && index < value->semanticChildren.size()
            ? QAccessible::queryAccessibleInterface(value->semanticChildren[index])
            : nullptr;
    }

    int childCount() const override
    {
        const auto *value = node();
        return value ? int(value->semanticChildren.size()) : 0;
    }

    int indexOfChild(const QAccessibleInterface *childInterface) const override
    {
        const auto *value = node();
        if (!value || !childInterface) {
            return -1;
        }
        for (qsizetype index = 0; index < value->semanticChildren.size(); ++index) {
            if (value->semanticChildren[index] == childInterface->object()) {
                return int(index);
            }
        }
        return -1;
    }

    QAccessibleInterface *focusChild() const override
    {
        const auto *value = node();
        if (!value) {
            return nullptr;
        }
        for (auto *childNode : value->semanticChildren) {
            if (childNode->data.focused) {
                return QAccessible::queryAccessibleInterface(childNode);
            }
            if (auto *childInterface = QAccessible::queryAccessibleInterface(childNode)) {
                if (auto *focused = childInterface->focusChild()) {
                    return focused;
                }
            }
        }
        return nullptr;
    }

    QString text(QAccessible::Text type) const override
    {
        const auto *value = node();
        if (!value) {
            return {};
        }
        switch (type) {
        case QAccessible::Name:
            return value->data.name;
        case QAccessible::Description:
        case QAccessible::Help:
            return value->data.description;
        case QAccessible::Value:
            return value->data.current ? QStringLiteral("current") : QString{};
        case QAccessible::Identifier:
        case QAccessible::DebugDescription:
            return value->data.id;
        case QAccessible::Accelerator:
        case QAccessible::UserText:
            return {};
        }
        return {};
    }

    void setText(QAccessible::Text, const QString &) override {}

    QRect rect() const override
    {
        const auto *value = node();
        return value && value->data.visible ? value->data.rect : QRect{};
    }

    QAccessible::Role role() const override
    {
        const auto *value = node();
        return value ? accessibleRole(value->data.role) : QAccessible::NoRole;
    }

    QAccessible::State state() const override
    {
        QAccessible::State state;
        const auto *value = node();
        if (!value) {
            state.invalid = true;
            state.invisible = true;
            return state;
        }
        const bool actionable = !value->data.actions.isEmpty();
        state.disabled = !value->data.enabled || !value->data.visible;
        state.invisible = !value->data.visible;
        state.focusable = actionable && value->data.enabled
            && value->data.visible;
        state.focused = value->data.focused;
        state.selectable = value->data.role == NodeRole::Tab;
        state.selected = value->data.selected;
        // Qt has no distinct current-tab bit. Active plus Value="current"
        // preserves the semantic without pretending that tabs are check boxes.
        state.active = value->data.current;
        return state;
    }

    void *interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ActionInterface) {
            return static_cast<QAccessibleActionInterface *>(this);
        }
        return QAccessibleObject::interface_cast(type);
    }

    QStringList actionNames() const override
    {
        const auto *value = node();
        if (!value || value->data.actions.isEmpty() || !value->data.enabled
            || !value->data.visible) {
            return {};
        }
        QStringList result;
        for (const auto &action : value->data.actions) {
            result.append(action.name);
        }
        result.append(QAccessibleActionInterface::setFocusAction());
        return result;
    }

    void doAction(const QString &actionName) override
    {
        auto *value = node();
        if (!value) {
            return;
        }
        QString error;
        bool accepted = false;
        if (actionName == QAccessibleActionInterface::setFocusAction()) {
            accepted = value->backend.focusNode(value->data.id, &error);
        } else {
            accepted = value->backend.invokeNode(value->data.id, actionName, &error);
        }
        if (!accepted) {
            qWarning("QindaQt accessible chrome action failed: %s", qPrintable(error));
        }
    }

    QStringList keyBindingsForAction(const QString &) const override
    {
        // KGlobalAccel stores user-remapped bindings outside presentation.
        // Returning a hard-coded default here would become stale immediately.
        return {};
    }

    QString localizedActionName(const QString &actionName) const override
    {
        if (actionName == HybridChromeAccessibilityAdapter::dockPageActionName()) {
            return QStringLiteral("Reorganize page");
        }
        if (actionName
            == HybridChromeAccessibilityAdapter::reorderPagePreviousActionName()) {
            return QStringLiteral("Move page backward");
        }
        if (actionName
            == HybridChromeAccessibilityAdapter::reorderPageNextActionName()) {
            return QStringLiteral("Move page forward");
        }
        return QAccessibleActionInterface::localizedActionName(actionName);
    }

    QString localizedActionDescription(const QString &actionName) const override
    {
        if (actionName == HybridChromeAccessibilityAdapter::dockPageActionName()) {
            return QStringLiteral(
                "Start keyboard docking to move, detach, or regroup this page");
        }
        if (actionName
            == HybridChromeAccessibilityAdapter::reorderPagePreviousActionName()) {
            return QStringLiteral("Move this page one position backward");
        }
        if (actionName
            == HybridChromeAccessibilityAdapter::reorderPageNextActionName()) {
            return QStringLiteral("Move this page one position forward");
        }
        return QAccessibleActionInterface::localizedActionDescription(actionName);
    }
};

QAccessibleInterface *accessibleFactory(const QString &, QObject *object)
{
    if (auto *node = dynamic_cast<NodeObject *>(object)) {
        return new NodeInterface(node);
    }
    return nullptr;
}

int &factoryUsers()
{
    static int users = 0;
    return users;
}

} // namespace

NodeObject::NodeObject(NodeData nodeData, Backend &nodeBackend, QObject *parent)
    : QObject(parent)
    , data(std::move(nodeData))
    , backend(nodeBackend)
{
}

void retainAccessibleFactory()
{
    // All adapters are created on KWin's GUI thread. A process-global factory
    // is ref-counted so unloading the plugin leaves no callback into its DSO.
    if (factoryUsers()++ == 0) {
        QAccessible::installFactory(accessibleFactory);
    }
}

void releaseAccessibleFactory()
{
    Q_ASSERT(factoryUsers() > 0);
    if (--factoryUsers() == 0) {
        QAccessible::removeFactory(accessibleFactory);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal
