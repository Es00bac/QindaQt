// SPDX-License-Identifier: GPL-3.0-or-later
#include "memberchromevisibilitycontroller.h"

#include <QStringList>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

void appendError(QStringList *errors, const QString &error)
{
    if (!error.isEmpty()) {
        errors->append(error);
    }
}

} // namespace

MemberChromeVisibilityController::MemberChromeVisibilityController(
    InspectMemberChrome inspect,
    SetMemberNoBorder setNoBorder)
    : m_inspect(std::move(inspect))
    , m_setNoBorder(std::move(setNoBorder))
{
}

MemberChromeVisibilityController::~MemberChromeVisibilityController()
{
    (void)restoreForShutdown();
}

bool MemberChromeVisibilityController::restoreMember(
    const QString &windowId,
    const MemberBaseline &baseline,
    QString *error)
{
    if (!baseline.eligible) {
        return true;
    }
    const auto current = m_inspect ? m_inspect(windowId) : std::nullopt;
    // A closed window no longer has presentation state to restore.
    if (!current) {
        return true;
    }
    if (current->noBorder == baseline.noBorder) {
        return true;
    }
    if (!m_setNoBorder) {
        return fail(error, QStringLiteral("native member chrome mutation is unavailable"));
    }
    return m_setNoBorder(windowId, baseline.noBorder, error);
}

bool MemberChromeVisibilityController::synchronize(
    const Hybrid::WindowTopology &topology,
    QString *error)
{
    // Restore before forgetting membership. A member may move directly between
    // groups, and the destination must capture the original native state rather
    // than the source group's hidden state.
    for (auto containerIt = m_containers.begin(); containerIt != m_containers.end();) {
        auto &state = containerIt.value();
        for (auto memberIt = state.members.begin(); memberIt != state.members.end();) {
            if (topology.ownerOf(memberIt.key()) == containerIt.key()) {
                ++memberIt;
                continue;
            }
            QString restoreError;
            if (!restoreMember(memberIt.key(), memberIt.value(), &restoreError)) {
                return fail(error,
                            QStringLiteral("could not restore member '%1': %2")
                                .arg(memberIt.key(), restoreError));
            }
            memberIt = state.members.erase(memberIt);
        }
        if (state.members.isEmpty() && !topology.container(containerIt.key())) {
            containerIt = m_containers.erase(containerIt);
        } else {
            ++containerIt;
        }
    }

    for (const auto &containerId : topology.containerIds()) {
        auto &state = m_containers[containerId];
        for (const auto &windowId : topology.windowIds(containerId)) {
            if (state.members.contains(windowId)) {
                continue;
            }
            const auto inspection = m_inspect ? m_inspect(windowId) : std::nullopt;
            if (!inspection) {
                return fail(error,
                            QStringLiteral("group member '%1' is unavailable")
                                .arg(windowId));
            }
            MemberBaseline baseline{
                .noBorder = inspection->noBorder,
                .eligible = !inspection->noBorder && inspection->serverDecorated
                    && inspection->userCanSetNoBorder,
                .clientDecorated = !inspection->serverDecorated,
            };
            state.members.insert(windowId, baseline);
            if (!state.visible && baseline.eligible) {
                QString mutationError;
                if (!m_setNoBorder
                    || !m_setNoBorder(windowId, true, &mutationError)) {
                    state.members.remove(windowId);
                    return fail(error,
                                QStringLiteral("could not hide member '%1': %2")
                                    .arg(windowId, mutationError));
                }
            }
        }
    }
    return true;
}

bool MemberChromeVisibilityController::setVisible(
    const QString &containerId,
    bool requestedVisible,
    MemberChromeVisibilitySummary *summary,
    QString *error)
{
    auto containerIt = m_containers.find(containerId);
    if (containerIt == m_containers.end()) {
        return fail(error,
                    QStringLiteral("unknown window group '%1'").arg(containerId));
    }
    auto &state = containerIt.value();
    MemberChromeVisibilitySummary nextSummary;
    for (const auto &baseline : std::as_const(state.members)) {
        if (baseline.clientDecorated) {
            ++nextSummary.unchangedClientDecoratedMembers;
        }
    }
    if (state.visible == requestedVisible) {
        if (summary) {
            *summary = nextSummary;
        }
        return true;
    }

    struct AppliedMutation final
    {
        QString windowId;
        bool previousNoBorder = false;
    };
    QVector<AppliedMutation> applied;
    const bool requestedNoBorder = !requestedVisible;
    state.visible = requestedVisible;
    auto memberIds = state.members.keys();
    memberIds.sort();
    for (const auto &windowId : memberIds) {
        const auto baseline = state.members.value(windowId);
        if (!baseline.eligible) {
            continue;
        }
        const auto current = m_inspect ? m_inspect(windowId) : std::nullopt;
        if (!current) {
            state.visible = !requestedVisible;
            for (auto rollback = applied.crbegin(); rollback != applied.crend(); ++rollback) {
                QString ignored;
                (void)m_setNoBorder(rollback->windowId, rollback->previousNoBorder,
                                    &ignored);
            }
            return fail(error,
                        QStringLiteral("group member '%1' is unavailable")
                            .arg(windowId));
        }
        if (current->noBorder == requestedNoBorder) {
            continue;
        }
        QString mutationError;
        if (!m_setNoBorder
            || !m_setNoBorder(windowId, requestedNoBorder, &mutationError)) {
            state.visible = !requestedVisible;
            for (auto rollback = applied.crbegin(); rollback != applied.crend(); ++rollback) {
                QString ignored;
                (void)m_setNoBorder(rollback->windowId, rollback->previousNoBorder,
                                    &ignored);
            }
            return fail(error,
                        QStringLiteral("could not update member '%1': %2")
                            .arg(windowId, mutationError));
        }
        applied.append({.windowId = windowId,
                        .previousNoBorder = current->noBorder});
        ++nextSummary.changedMembers;
    }
    if (summary) {
        *summary = nextSummary;
    }
    return true;
}

bool MemberChromeVisibilityController::toggle(
    const QString &containerId,
    MemberChromeVisibilitySummary *summary,
    QString *error)
{
    return setVisible(containerId, !visible(containerId), summary, error);
}

bool MemberChromeVisibilityController::visible(
    const QString &containerId) const noexcept
{
    const auto it = m_containers.constFind(containerId);
    return it == m_containers.cend() || it->visible;
}

bool MemberChromeVisibilityController::nativeTitleVisible(
    const QString &containerId,
    const QString &windowId) const noexcept
{
    const auto containerIt = m_containers.constFind(containerId);
    if (containerIt == m_containers.cend()) {
        return true;
    }
    const auto memberIt = containerIt->members.constFind(windowId);
    if (memberIt == containerIt->members.cend()) {
        return true;
    }
    if (memberIt->eligible) {
        return containerIt->visible;
    }
    return !memberIt->clientDecorated && !memberIt->noBorder;
}

bool MemberChromeVisibilityController::restoreForShutdown(QString *error) noexcept
{
    QStringList errors;
    for (auto containerIt = m_containers.begin(); containerIt != m_containers.end();) {
        for (auto memberIt = containerIt->members.begin();
             memberIt != containerIt->members.end();) {
            QString restoreError;
            if (!restoreMember(memberIt.key(), memberIt.value(), &restoreError)) {
                appendError(&errors,
                            QStringLiteral("%1: %2").arg(memberIt.key(), restoreError));
                ++memberIt;
            } else {
                memberIt = containerIt->members.erase(memberIt);
            }
        }
        if (containerIt->members.isEmpty()) {
            containerIt = m_containers.erase(containerIt);
        } else {
            ++containerIt;
        }
    }
    if (!errors.isEmpty()) {
        return fail(error, errors.join(QStringLiteral("; ")));
    }
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
