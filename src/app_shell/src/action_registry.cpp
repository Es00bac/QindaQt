// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/app_shell/action_registry.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <tuple>
#include <utility>

namespace QindaQt::AppShell {
namespace {

const QRegularExpression &identifierPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$"));
    return pattern;
}

bool boundedIdentifier(const QString &value)
{
    return !value.isEmpty() && value.size() <= MaximumIdentifierLength
        && identifierPattern().match(value).hasMatch();
}

QVariantMap actionMap(const ActionSpec &action)
{
    return {{QStringLiteral("id"), action.id},
            {QStringLiteral("label"), action.label},
            {QStringLiteral("accessibleDescription"), action.accessibleDescription},
            {QStringLiteral("shortcut"),
             action.shortcut.toString(QKeySequence::PortableText)},
            {QStringLiteral("enabled"), action.enabled},
            {QStringLiteral("checkable"), action.checkable},
            {QStringLiteral("checked"), action.checked},
            {QStringLiteral("destructive"), action.destructive}};
}

} // namespace

ActionRegistry::ActionRegistry(QObject *parent)
    : QObject(parent)
{
}

QVariantList ActionRegistry::menus() const
{
    return m_menus;
}

QList<ActionSpec> ActionRegistry::actions() const
{
    return m_actions;
}

Error ActionRegistry::replaceActions(const QList<ActionSpec> &actions)
{
    if (const Error threadError = verifyThread(); !threadError.ok()) {
        return threadError;
    }
    if (const Error error = validate(actions); !error.ok()) {
        return error;
    }
    m_actions = actions;
    rebuildSnapshot();
    emit menusChanged();
    return Error::success();
}

Error ActionRegistry::setEnabled(const QString &actionId, bool enabled)
{
    if (const Error threadError = verifyThread(); !threadError.ok()) {
        return threadError;
    }
    auto action = std::find_if(m_actions.begin(), m_actions.end(), [&](const ActionSpec &candidate) {
        return candidate.id == actionId;
    });
    if (action == m_actions.end()) {
        return makeError(ErrorCode::UnknownAction,
                         QStringLiteral("Unknown action: %1").arg(actionId));
    }
    if (action->enabled != enabled) {
        action->enabled = enabled;
        rebuildSnapshot();
        emit menusChanged();
    }
    return Error::success();
}

Error ActionRegistry::setChecked(const QString &actionId, bool checked)
{
    if (const Error threadError = verifyThread(); !threadError.ok()) {
        return threadError;
    }
    auto action = std::find_if(m_actions.begin(), m_actions.end(), [&](const ActionSpec &candidate) {
        return candidate.id == actionId;
    });
    if (action == m_actions.end()) {
        return makeError(ErrorCode::UnknownAction,
                         QStringLiteral("Unknown action: %1").arg(actionId));
    }
    if (!action->checkable) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Action is not checkable: %1").arg(actionId));
    }
    if (action->checked != checked) {
        action->checked = checked;
        rebuildSnapshot();
        emit menusChanged();
    }
    return Error::success();
}

Error ActionRegistry::requestActivation(const QString &actionId)
{
    if (const Error threadError = verifyThread(); !threadError.ok()) {
        return threadError;
    }
    const auto action = std::find_if(
        m_actions.cbegin(), m_actions.cend(), [&](const ActionSpec &candidate) {
            return candidate.id == actionId;
        });
    if (action == m_actions.cend()) {
        return makeError(ErrorCode::UnknownAction,
                         QStringLiteral("Unknown action: %1").arg(actionId));
    }
    if (!action->enabled) {
        return makeError(ErrorCode::Unavailable,
                         QStringLiteral("Action is unavailable: %1").arg(actionId),
                         true);
    }
    emit activationRequested(actionId);
    return Error::success();
}

Error ActionRegistry::validate(const QList<ActionSpec> &actions) const
{
    if (actions.size() > MaximumActionCount) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Action count exceeds %1").arg(MaximumActionCount));
    }

    QSet<QString> actionIds;
    QHash<QString, QString> menuLabels;
    QHash<QString, int> menuOrders;
    for (const ActionSpec &action : actions) {
        if (!boundedIdentifier(action.id) || !boundedIdentifier(action.menuId)) {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Action and menu identifiers must be bounded stable IDs"));
        }
        if (action.label.trimmed().isEmpty() || action.label != action.label.trimmed()
            || action.label.size() > MaximumLabelLength
            || action.menuLabel.trimmed().isEmpty()
            || action.menuLabel != action.menuLabel.trimmed()
            || action.menuLabel.size() > MaximumLabelLength
            || action.accessibleDescription.size() > MaximumDiagnosticLength
            || action.shortcut.isEmpty() || (action.checked && !action.checkable)) {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Action %1 has invalid presentation state").arg(action.id));
        }
        if (actionIds.contains(action.id)) {
            return makeError(ErrorCode::DuplicateAction,
                             QStringLiteral("Duplicate action ID: %1").arg(action.id));
        }
        actionIds.insert(action.id);

        const auto existingLabel = menuLabels.constFind(action.menuId);
        if (existingLabel != menuLabels.cend() && existingLabel.value() != action.menuLabel) {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Menu %1 has inconsistent labels").arg(action.menuId));
        }
        const auto existingOrder = menuOrders.constFind(action.menuId);
        if (existingOrder != menuOrders.cend() && existingOrder.value() != action.menuOrder) {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Menu %1 has inconsistent ordering").arg(action.menuId));
        }
        menuLabels.insert(action.menuId, action.menuLabel);
        menuOrders.insert(action.menuId, action.menuOrder);
    }
    if (menuLabels.size() > MaximumMenuCount) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Menu count exceeds %1").arg(MaximumMenuCount));
    }
    return Error::success();
}

Error ActionRegistry::verifyThread() const
{
    if (thread() != QThread::currentThread()) {
        return makeError(ErrorCode::WrongThread,
                         QStringLiteral("Action registry mutation must run on its owning thread"));
    }
    return Error::success();
}

void ActionRegistry::rebuildSnapshot()
{
    QList<ActionSpec> ordered = m_actions;
    std::sort(ordered.begin(), ordered.end(), [](const ActionSpec &left, const ActionSpec &right) {
        return std::tie(left.menuOrder, left.menuId, left.order, left.id)
            < std::tie(right.menuOrder, right.menuId, right.order, right.id);
    });

    QVariantList menus;
    QString currentMenu;
    QString currentLabel;
    QVariantList currentActions;
    const auto appendMenu = [&]() {
        if (!currentMenu.isEmpty()) {
            menus.append(QVariantMap{{QStringLiteral("id"), currentMenu},
                                     {QStringLiteral("label"), currentLabel},
                                     {QStringLiteral("actions"), currentActions}});
        }
    };

    for (const ActionSpec &action : std::as_const(ordered)) {
        if (action.menuId != currentMenu) {
            appendMenu();
            currentMenu = action.menuId;
            currentLabel = action.menuLabel;
            currentActions.clear();
        }
        currentActions.append(actionMap(action));
    }
    appendMenu();
    m_menus = menus;
}

} // namespace QindaQt::AppShell
