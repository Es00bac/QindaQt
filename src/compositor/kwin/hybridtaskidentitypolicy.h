// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/windowtopology.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct TaskMemberIdentity final
{
    QString containerId;
    QString pageId;
    QString windowId;
    bool activePage = false;
    bool primary = false;
    bool skipTaskbar = true;
    bool skipSwitcher = true;

    friend bool operator==(const TaskMemberIdentity &,
                           const TaskMemberIdentity &) = default;
};

struct TaskContainerIdentity final
{
    QString containerId;
    QString activePageId;
    QString primaryWindowId;
    QVector<TaskMemberIdentity> members;

    [[nodiscard]] const TaskMemberIdentity *member(
        const QString &windowId) const noexcept;
    [[nodiscard]] bool isValid(QString *error = nullptr) const;

    friend bool operator==(const TaskContainerIdentity &,
                           const TaskContainerIdentity &) = default;
};

enum class TaskIdentityEvent {
    Activated,
    Minimized,
    Unminimized,
};

enum class TaskIdentityAction {
    None,
    ActivatePage,
    MinimizeContainer,
};

struct TaskIdentityDecision final
{
    TaskIdentityAction action = TaskIdentityAction::None;
    QString containerId;
    QString pageId;
    QString windowId;
    // The KWin adapter re-minimizes an externally exposed inactive member
    // before synchronously activating its page. Scene-owned changes are muted.
    bool hideBeforeAction = false;

    [[nodiscard]] bool hasAction() const noexcept
    {
        return action != TaskIdentityAction::None;
    }

    friend bool operator==(const TaskIdentityDecision &,
                           const TaskIdentityDecision &) = default;
};

// Pure policy for collapsing every group to one native KWin task identity.
// The chosen member remains a real client, so its native caption and icon are
// the container identity; no synthetic window or transient enters topology.
class HybridTaskIdentityPolicy final
{
public:
    [[nodiscard]] static std::optional<TaskContainerIdentity> planContainer(
        const Core::WindowContainer &container,
        const QString &preferredActiveWindowId = {},
        QString *error = nullptr);
    [[nodiscard]] static std::optional<QVector<TaskContainerIdentity>> planTopology(
        const Hybrid::WindowTopology &topology,
        const QString &preferredActiveWindowId = {},
        QString *error = nullptr);
    [[nodiscard]] static const TaskMemberIdentity *findMember(
        const QVector<TaskContainerIdentity> &plans,
        const QString &windowId) noexcept;
    [[nodiscard]] static TaskIdentityDecision decide(
        const TaskMemberIdentity *member,
        TaskIdentityEvent event);
};

} // namespace QindaQt::Compositor::KWinIntegration
