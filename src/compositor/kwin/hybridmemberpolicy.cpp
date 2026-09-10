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

    // AGENT-GUARD: A focus transition owns the pre-action baseline. Geometry
    // and visibility signals emitted by its KWin writes must not replace that
    // copy with temporary presentation, and a refresh nested inside a
    // transition must not restore a container that transition is already
    // unwinding (native detach synchronizes topology inside its platform
    // callback and clears its own container's focus after the callback
    // returns). Only a top-level refresh restores presentation whose owner
    // left the active page or whose container disappeared.
    if (!m_applying) {
        QStringList invalidated;
        for (auto entry = m_focus.cbegin(); entry != m_focus.cend(); ++entry) {
            bool stillValid = false;
            for (const auto &group : groups) {
                const auto *member = group.containerId == entry.key()
                    ? group.member(entry->state.windowId) : nullptr;
                if (member && member->activePage) {
                    stillValid = true;
                    break;
                }
            }
            if (!stillValid) {
                invalidated.append(entry.key());
            }
        }
        for (const auto &containerId : std::as_const(invalidated)) {
            QSet<QString> missing;
            for (const auto &member : m_focus.value(containerId).baseline.members) {
                if (!windowIds.contains(member.windowId)) {
                    missing.insert(member.windowId);
                }
            }
            if (!restore(containerId, {}, missing,
                         MemberRestoreActivation::RestoreBaseline, error)) {
                return false;
            }
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

QString HybridMemberPolicy::focusedContainerOf(const QString &windowId) const
{
    for (auto entry = m_focus.cbegin(); entry != m_focus.cend(); ++entry) {
        if (entry->state.windowId == windowId) {
            return entry.key();
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
    // AGENT-GUARD: The detach below is a scene transaction and re-plans every
    // group, so every *other* container's focus presentation must leave first
    // (PreserveCurrent keeps KWin's activation on the dragged window). The
    // dragged member's own container is instead handed to the platform, which
    // unwinds it together with the detach.
    const auto focusedContainers = m_focus.keys();
    for (const auto &focusedId : focusedContainers) {
        if (focusedId != containerId
            && !restore(focusedId, {}, {},
                        MemberRestoreActivation::PreserveCurrent, error)) {
            return false;
        }
    }
    const std::optional<MemberGroupBaseline> focusBaseline = m_focus.contains(containerId)
        ? std::optional<MemberGroupBaseline>(m_focus.value(containerId).baseline)
        : std::nullopt;
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
        m_focus.remove(containerId);
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
    m_focus.insert(group.containerId,
                   FocusEntry{MemberFocusState{group.containerId, member.windowId, mode},
                              group});
    return true;
}

bool HybridMemberPolicy::restoreRejectedPresentation(const QString &containerId,
                                                     const QString &windowId,
                                                     MemberFocusMode mode,
                                                     QString *error)
{
    const auto found = m_focus.constFind(containerId);
    if (found == m_focus.cend()) {
        return fail(error, QStringLiteral(
                               "rejected member presentation has no focus baseline"));
    }
    if (!found->baseline.member(windowId)) {
        return fail(error, QStringLiteral(
                               "rejected member is not in the focus baseline"));
    }
    // Copy: the platform may re-enter synchronize() while this runs.
    const FocusEntry entry = *found;
    m_applying = true;
    const auto guard = qScopeGuard([this] { m_applying = false; });
    return m_platform.restoreRejectedPresentation(
        entry.baseline, windowId, entry.state.windowId, mode, error);
}

bool HybridMemberPolicy::restore(const QString &containerId,
                                 const QString &minimizeWindowId,
                                 const QSet<QString> &missingWindowIds,
                                 MemberRestoreActivation activation,
                                 QString *error)
{
    const auto found = m_focus.constFind(containerId);
    if (found == m_focus.cend()) {
        return false;
    }
    // Copy both the baseline and the key: the platform may re-enter
    // synchronize(), and the caller's containerId may reference this entry.
    const MemberGroupBaseline baseline = found->baseline;
    const QString key = containerId;
    m_applying = true;
    const auto guard = qScopeGuard([this] { m_applying = false; });
    if (!m_platform.restoreGroup(baseline, minimizeWindowId,
                                 missingWindowIds, activation, error)) {
        return false;
    }
    m_focus.remove(key);
    return true;
}

bool HybridMemberPolicy::restoreAll(const QSet<QString> &missingWindowIds,
                                    MemberRestoreActivation activation,
                                    QString *error)
{
    const auto focusedContainers = m_focus.keys();
    for (const auto &containerId : focusedContainers) {
        if (!restore(containerId, {}, missingWindowIds, activation, error)) {
            return false;
        }
    }
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
    const QString containerId = m_groups[location.groupIndex].containerId;
    const auto owner = m_focus.constFind(containerId);
    if (owner != m_focus.cend()) {
        if (owner->state.windowId == windowId
            && owner->state.mode == MemberFocusMode::Maximized) {
            // The adapter clears KWin's real maximize bit on entry. A second
            // native-button request therefore arrives as another maximize=true;
            // while policy state supplies the decoration's restore glyph.
            return restore(containerId, {}, {},
                           MemberRestoreActivation::RestoreBaseline, error);
        }
        if (owner->state.windowId != windowId) {
            static_cast<void>(restoreRejectedPresentation(
                containerId, windowId, MemberFocusMode::Maximized, error));
            return false;
        }
        return fail(error, QStringLiteral("member focus mode conflicts with existing owner"));
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
        const QString containerId = focusedContainerOf(windowId);
        if (containerId.isEmpty()
            || m_focus.value(containerId).state.mode != MemberFocusMode::Fullscreen) {
            return false;
        }
        return restore(containerId, {}, {},
                       MemberRestoreActivation::PreserveCurrent, error);
    }
    const auto location = locate(windowId);
    if (!location.isValid()) {
        return false;
    }
    const QString containerId = m_groups[location.groupIndex].containerId;
    const auto owner = m_focus.constFind(containerId);
    if (owner != m_focus.cend()) {
        if (owner->state.windowId != windowId) {
            static_cast<void>(restoreRejectedPresentation(
                containerId, windowId, MemberFocusMode::Fullscreen, error));
            return false;
        }
        return fail(error, QStringLiteral("member focus mode conflicts with existing owner"));
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
    if (m_applying || !minimized) {
        return false;
    }
    const QString containerId = focusedContainerOf(windowId);
    if (containerId.isEmpty()) {
        return false;
    }
    return restore(containerId, windowId, {},
                   MemberRestoreActivation::RestoreBaseline, error);
}

bool HybridMemberPolicy::memberClosed(const QString &windowId, QString *error)
{
    if (error) {
        error->clear();
    }
    m_detaching.remove(windowId);
    if (m_applying) {
        return false;
    }
    for (auto entry = m_focus.cbegin(); entry != m_focus.cend(); ++entry) {
        if (!entry->baseline.member(windowId)) {
            continue;
        }
        // KWin removes the client and selects its successor before the
        // registry emits managedWindowClosed. Preserve that compositor-owned
        // choice; a hidden peer closing must not reactivate the old group
        // baseline.
        const QString containerId = entry.key();
        return restore(containerId, {}, {windowId},
                       MemberRestoreActivation::PreserveCurrent, error);
    }
    return false;
}

bool HybridMemberPolicy::restoreForTopologyMutation(QString *error)
{
    if (error) {
        error->clear();
    }
    return restoreAll({}, MemberRestoreActivation::RestoreBaseline, error);
}

bool HybridMemberPolicy::restoreForContainerAction(const QString &containerId,
                                                   QString *error)
{
    if (error) {
        error->clear();
    }
    return !m_focus.contains(containerId)
        || restore(containerId, {}, {},
                   MemberRestoreActivation::RestoreBaseline, error);
}

bool HybridMemberPolicy::restoreForLifecycleMutation(QString *error)
{
    if (error) {
        error->clear();
    }
    return restoreAll({}, MemberRestoreActivation::PreserveCurrent, error);
}

bool HybridMemberPolicy::restoreForShutdown(QSet<QString> missingWindowIds,
                                            QString *error)
{
    if (error) {
        error->clear();
    }
    return restoreAll(missingWindowIds,
                      MemberRestoreActivation::PreserveCurrent, error);
}

std::optional<MemberFocusState> HybridMemberPolicy::focusState(
    const QString &containerId) const
{
    const auto found = m_focus.constFind(containerId);
    return found == m_focus.cend()
        ? std::nullopt
        : std::optional<MemberFocusState>(found->state);
}

QVector<MemberFocusState> HybridMemberPolicy::focusStates() const
{
    QVector<MemberFocusState> result;
    result.reserve(m_focus.size());
    for (auto entry = m_focus.cbegin(); entry != m_focus.cend(); ++entry) {
        result.append(entry->state);
    }
    return result;
}

std::optional<MemberGroupBaseline> HybridMemberPolicy::focusBaseline(
    const QString &containerId) const
{
    const auto found = m_focus.constFind(containerId);
    return found == m_focus.cend()
        ? std::nullopt
        : std::optional<MemberGroupBaseline>(found->baseline);
}

} // namespace QindaQt::Compositor::KWinIntegration
