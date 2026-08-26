// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridmemberpolicy.h"

#include <QScopeGuard>

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

} // namespace

const MemberLayoutBaseline *MemberGroupBaseline::member(const QString &windowId) const
{
    for (const auto &candidate : members) {
        if (candidate.windowId == windowId) {
            return &candidate;
        }
    }
    return nullptr;
}

bool MemberGroupBaseline::isValid(QString *error) const
{
    if (containerId.trimmed().isEmpty() || !outerFrame.isValid()) {
        return fail(error, QStringLiteral("member group needs an ID and valid outer frame"));
    }
    if (members.size() < 2) {
        return fail(error, QStringLiteral("member group needs at least two members"));
    }
    QSet<QString> ids;
    qsizetype activePageMembers = 0;
    for (const auto &candidate : members) {
        if (candidate.windowId.trimmed().isEmpty() || !candidate.frame.isValid()) {
            return fail(error, QStringLiteral("member baseline needs an ID and valid frame"));
        }
        if (ids.contains(candidate.windowId)) {
            return fail(error, QStringLiteral("member group contains duplicate window '%1'")
                                   .arg(candidate.windowId));
        }
        ids.insert(candidate.windowId);
        activePageMembers += candidate.activePage ? 1 : 0;
    }
    if (activePageMembers == 0) {
        return fail(error, QStringLiteral("member group has no active-page member"));
    }
    return true;
}

HybridMemberPolicy::HybridMemberPolicy(HybridMemberPolicyPlatform &platform)
    : m_platform(platform)
{
}

bool HybridMemberPolicy::synchronize(QVector<MemberGroupBaseline> groups, QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> groupIds;
    QSet<QString> windowIds;
    for (const auto &group : groups) {
        if (!group.isValid(error)) {
            return false;
        }
        if (groupIds.contains(group.containerId)) {
            return fail(error, QStringLiteral("duplicate member group '%1'")
                                   .arg(group.containerId));
        }
        groupIds.insert(group.containerId);
        for (const auto &member : group.members) {
            if (windowIds.contains(member.windowId)) {
                return fail(error, QStringLiteral("window '%1' belongs to multiple groups")
                                       .arg(member.windowId));
            }
            windowIds.insert(member.windowId);
        }
    }

    bool focusStillValid = !m_focus;
    if (m_focus) {
        for (const auto &group : groups) {
            const auto *member = group.containerId == m_focus->containerId
                ? group.member(m_focus->windowId) : nullptr;
            if (member && member->activePage) {
                focusStillValid = true;
                break;
            }
        }
    }
    const bool focusedDetachOwnsInvalidation = m_applying && m_focus
        && m_detaching.contains(m_focus->windowId);
    // AGENT-GUARD: A focus transition owns the pre-action baseline. Geometry
    // and visibility signals emitted by its KWin writes must not replace that
    // copy with temporary presentation. Native detach synchronizes topology
    // inside its platform callback; that outer transition alone consumes the
    // copied baseline and clears focus after the callback returns.
    if (m_focus && !focusStillValid && !focusedDetachOwnsInvalidation) {
        QSet<QString> missing;
        if (m_focusBaseline) {
            for (const auto &member : m_focusBaseline->members) {
                if (!windowIds.contains(member.windowId)) {
                    missing.insert(member.windowId);
                }
            }
        }
        if (!restore({}, std::move(missing),
                     MemberRestoreActivation::RestoreBaseline, error)) {
            return false;
        }
    }
    m_groups = std::move(groups);
    m_detaching.intersect(windowIds);
    return true;
}

HybridMemberPolicy::MemberLocation HybridMemberPolicy::locate(
    const QString &windowId) const
{
    for (qsizetype groupIndex = 0; groupIndex < m_groups.size(); ++groupIndex) {
        const auto &members = m_groups[groupIndex].members;
        for (qsizetype memberIndex = 0; memberIndex < members.size(); ++memberIndex) {
            if (members[memberIndex].windowId == windowId) {
                return {groupIndex, memberIndex};
            }
        }
    }
    return {};
}

bool HybridMemberPolicy::interactiveMoveStarted(const QString &windowId,
                                                bool interactiveMove,
                                                QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_applying || !interactiveMove || m_detaching.contains(windowId)) {
        return false;
    }
    const auto location = locate(windowId);
    if (!location.isValid()) {
        return false;
    }
    // Both values cross a production callback that synchronously publishes a
    // new topology and replaces m_groups. They must not borrow policy storage.
    const QString containerId = m_groups[location.groupIndex].containerId;
    const std::optional<MemberGroupBaseline> focusBaseline = m_focus
            && m_focus->containerId == containerId && m_focusBaseline
        ? m_focusBaseline : std::nullopt;
    m_applying = true;
    const auto guard = qScopeGuard([this] { m_applying = false; });
    // AGENT-GUARD: Mark before the platform callback. Production detach
    // publishes and synchronizes the new topology synchronously; that refresh
    // must be able to discard this ID while it is absent. Re-inserting after
    // the callback would poison a later redock and make native detach one-shot.
    m_detaching.insert(windowId);
    if (!m_platform.detachMember(containerId, windowId,
                                 focusBaseline ? &*focusBaseline : nullptr,
                                 error)) {
        m_detaching.remove(windowId);
        return false;
    }
    if (focusBaseline) {
        m_focus.reset();
        m_focusBaseline.reset();
    }
    return true;
}

bool HybridMemberPolicy::enter(const MemberLocation &location,
                               MemberFocusMode mode,
                               QString *error)
{
    const auto &group = m_groups[location.groupIndex];
    const auto &member = group.members[location.memberIndex];
    if (!member.activePage) {
        return fail(error, QStringLiteral("only an active-page member can enter focus mode"));
    }

    m_applying = true;
    const auto guard = qScopeGuard([this] { m_applying = false; });
    if (!m_platform.enterFocus(group, member.windowId, mode, error)) {
        return false;
    }
    m_focusBaseline = group;
    m_focus = MemberFocusState{group.containerId, member.windowId, mode};
    return true;
}

bool HybridMemberPolicy::restore(const QString &minimizeWindowId,
                                 QSet<QString> missingWindowIds,
                                 MemberRestoreActivation activation,
                                 QString *error)
{
    if (!m_focus || !m_focusBaseline) {
        return false;
    }
    m_applying = true;
    const auto guard = qScopeGuard([this] { m_applying = false; });
    if (!m_platform.restoreGroup(*m_focusBaseline, minimizeWindowId,
                                 missingWindowIds, activation, error)) {
        return false;
    }
    m_focus.reset();
    m_focusBaseline.reset();
    return true;
}

bool HybridMemberPolicy::maximizedChanged(const QString &windowId,
                                          bool maximized,
                                          QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_applying || !maximized) {
        return false;
    }
    const auto location = locate(windowId);
    if (!location.isValid()) {
        return false;
    }
    if (m_focus && m_focus->windowId == windowId
        && m_focus->mode == MemberFocusMode::Maximized) {
        // The adapter clears KWin's real maximize bit on entry. A second
        // native-button request therefore arrives as another maximize=true;
        // while policy state supplies the decoration's restore glyph.
        return restore({}, {}, MemberRestoreActivation::RestoreBaseline, error);
    }
    if (m_focus) {
        return fail(error, QStringLiteral("another member already owns focus mode"));
    }
    return enter(location, MemberFocusMode::Maximized, error);
}

bool HybridMemberPolicy::fullscreenChanged(const QString &windowId,
                                           bool fullscreen,
                                           QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_applying) {
        return false;
    }
    if (!fullscreen) {
        return m_focus && m_focus->windowId == windowId
                && m_focus->mode == MemberFocusMode::Fullscreen
            ? restore({}, {}, MemberRestoreActivation::RestoreBaseline, error)
            : false;
    }
    const auto location = locate(windowId);
    if (!location.isValid()) {
        return false;
    }
    if (m_focus) {
        return fail(error, QStringLiteral("another member already owns focus mode"));
    }
    return enter(location, MemberFocusMode::Fullscreen, error);
}

bool HybridMemberPolicy::minimizedChanged(const QString &windowId,
                                          bool minimized,
                                          QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_applying || !minimized || !m_focus || m_focus->windowId != windowId) {
        return false;
    }
    return restore(windowId, {}, MemberRestoreActivation::RestoreBaseline, error);
}

bool HybridMemberPolicy::memberClosed(const QString &windowId, QString *error)
{
    if (error) {
        error->clear();
    }
    m_detaching.remove(windowId);
    if (m_applying || !m_focus || !m_focusBaseline
        || !m_focusBaseline->member(windowId)) {
        return false;
    }
    // KWin removes the client and selects its successor before the registry
    // emits managedWindowClosed. Preserve that compositor-owned choice; a
    // hidden peer closing must not reactivate the old group baseline.
    return restore({}, {windowId}, MemberRestoreActivation::PreserveCurrent, error);
}

bool HybridMemberPolicy::restoreForTopologyMutation(QString *error)
{
    if (error) {
        error->clear();
    }
    return !m_focus || restore({}, {}, MemberRestoreActivation::RestoreBaseline, error);
}

bool HybridMemberPolicy::restoreForLifecycleMutation(QString *error)
{
    if (error) {
        error->clear();
    }
    return !m_focus
        || restore({}, {}, MemberRestoreActivation::PreserveCurrent, error);
}

bool HybridMemberPolicy::restoreForShutdown(QSet<QString> missingWindowIds,
                                            QString *error)
{
    if (error) {
        error->clear();
    }
    return !m_focus
        || restore({}, std::move(missingWindowIds),
                   MemberRestoreActivation::PreserveCurrent, error);
}

} // namespace QindaQt::Compositor::KWinIntegration
