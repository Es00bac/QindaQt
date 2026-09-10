// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinmemberpolicyplatform.h"

#include "kwinchromemanager.h"
#include "managedwindowregistry.h"

#include <KDecoration3/Decoration>

#include <window.h>
#include <workspace.h>

#include <QPointer>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto MemberFocusProperty = "qindaqtMemberFocusMode";

void setFocusProperty(KWin::Window *window, MemberFocusMode mode, bool enabled)
{
    auto *decoration = window ? window->decoration() : nullptr;
    if (!decoration) {
        return;
    }
    const auto value = !enabled ? QString{}
        : mode == MemberFocusMode::Maximized ? QStringLiteral("maximized")
                                             : QStringLiteral("fullscreen");
    // AGENT-CONTRACT: QindaDecoration reads this process-local property only
    // for its maximize/restore glyph. KWin's real maximize bit stays clear so
    // the temporary group presentation cannot acquire independent geometry.
    decoration->setProperty(MemberFocusProperty, value);
    decoration->update();
}

bool preflight(const ManagedWindowRegistry &registry,
               const MemberGroupBaseline &baseline,
               const QSet<QString> &missing,
               QString *error)
{
    for (const auto &member : baseline.members) {
        if (missing.contains(member.windowId)) {
            continue;
        }
        if (!registry.window(member.windowId)) {
            if (error) {
                *error = QStringLiteral("group member '%1' is no longer managed")
                             .arg(member.windowId);
            }
            return false;
        }
    }
    return true;
}

} // namespace

KWinMemberPolicyPlatform::KWinMemberPolicyPlatform(ManagedWindowRegistry &registry,
                                                   KWinChromeManager &chrome,
                                                   NativeMemberDetach detach)
    : m_registry(registry)
    , m_chrome(chrome)
    , m_detach(std::move(detach))
{
}

bool KWinMemberPolicyPlatform::detachMember(const QString &containerId,
                                            const QString &windowId,
                                            const MemberGroupBaseline *focusBaseline,
                                            QString *error)
{
    if (!m_detach) {
        if (error) {
            *error = QStringLiteral("native member detach callback is unavailable");
        }
        return false;
    }
    // The topology scene transaction restores the detached client and
    // reflows survivors atomically. Only clear presentation metadata after
    // it commits, so a rejected mutation leaves focus mode untouched.
    if (!m_detach(containerId, windowId, error)) {
        return false;
    }
    if (focusBaseline) {
        for (const auto &member : focusBaseline->members) {
            if (auto *window = m_registry.window(member.windowId)) {
                setFocusProperty(window, MemberFocusMode::Maximized, false);
                window->setHidden(member.hidden);
            }
        }
        setChromeVisible(containerId, true);
    }
    return true;
}

bool KWinMemberPolicyPlatform::enterFocus(const MemberGroupBaseline &baseline,
                                          const QString &windowId,
                                          MemberFocusMode mode,
                                          QString *error)
{
    if (!preflight(m_registry, baseline, {}, error)) {
        return false;
    }
    auto *focused = m_registry.window(windowId);
    if (!focused || !baseline.member(windowId)) {
        if (error) {
            *error = QStringLiteral("focus member is not in the committed group");
        }
        return false;
    }

    setChromeVisible(baseline.containerId, false);
    for (const auto &member : baseline.members) {
        auto *window = m_registry.window(member.windowId);
        setFocusProperty(window, mode, member.windowId == windowId);
        if (member.windowId != windowId) {
            window->setHidden(true);
            continue;
        }
        window->setHidden(false);
        window->setMinimized(false);
        if (mode == MemberFocusMode::Fullscreen) {
            if (!window->isFullScreen()) {
                window->setFullScreen(true);
            }
        } else {
            if (window->isFullScreen()) {
                window->setFullScreen(false);
            }
            if (window->maximizeMode() != KWin::MaximizeRestore) {
                window->maximize(KWin::MaximizeRestore);
            }
            window->moveResize(baseline.outerFrame);
        }
    }
    KWin::workspace()->activateWindow(focused);
    return true;
}

bool KWinMemberPolicyPlatform::restoreRejectedPresentation(
    const MemberGroupBaseline &baseline,
    const QString &windowId,
    const QString &focusOwnerWindowId,
    MemberFocusMode mode,
    QString *error)
{
    const auto *member = baseline.member(windowId);
    auto *window = m_registry.window(windowId);
    auto *focusOwner = m_registry.window(focusOwnerWindowId);
    if (!member || !window || !baseline.member(focusOwnerWindowId) || !focusOwner) {
        if (error) {
            *error = QStringLiteral(
                "rejected focus member or accepted owner is absent from the committed group");
        }
        return false;
    }

    // AGENT-GUARD: A native peer fullscreen request can activate the peer
    // before KWin emits fullScreenChanged. Undo only that request and
    // immediately restore the accepted owner; do not install an
    // active-window observer, because Alt-Tab and outside focus must stay
    // free once this correction completes. HybridMemberPolicy keeps its
    // applying guard active while these KWin setters emit state signals.
    if (mode == MemberFocusMode::Fullscreen && window->isFullScreen()) {
        window->setFullScreen(false);
    }
    if (window->maximizeMode() != KWin::MaximizeRestore) {
        window->maximize(KWin::MaximizeRestore);
    }
    if (window->requestedQuickTileMode()
            != KWin::QuickTileMode(KWin::QuickTileFlag::None)
        || window->quickTileMode()
            != KWin::QuickTileMode(KWin::QuickTileFlag::None)) {
        window->setQuickTileMode(KWin::QuickTileFlag::None,
                                 member->frame.center());
    }
    window->moveResize(member->frame);
    if (focusOwner != window) {
        KWin::workspace()->activateWindow(focusOwner);
    }
    return true;
}

bool KWinMemberPolicyPlatform::restoreGroup(const MemberGroupBaseline &baseline,
                                            const QString &minimizeWindowId,
                                            const QSet<QString> &missingWindowIds,
                                            MemberRestoreActivation activationMode,
                                            QString *error)
{
    if (!preflight(m_registry, baseline, missingWindowIds, error)) {
        return false;
    }
    QPointer<KWin::Window> preservedActivation;
    if (activationMode == MemberRestoreActivation::PreserveCurrent) {
        preservedActivation = KWin::workspace()->activeWindow();
    }
    KWin::Window *baselineActivation = nullptr;
    for (const auto &member : baseline.members) {
        if (missingWindowIds.contains(member.windowId)) {
            continue;
        }
        auto *window = m_registry.window(member.windowId);
        setFocusProperty(window, MemberFocusMode::Maximized, false);
        if (window->isFullScreen()) {
            window->setFullScreen(false);
        }
        if (window->maximizeMode() != KWin::MaximizeRestore) {
            window->maximize(KWin::MaximizeRestore);
        }
        window->setHidden(member.hidden);
        window->moveResize(member.frame);
        window->setMinimized(member.minimized
                             || member.windowId == minimizeWindowId);
        if (member.active && member.windowId != minimizeWindowId
            && !member.minimized && !member.hidden) {
            baselineActivation = window;
        }
    }
    setChromeVisible(baseline.containerId, true);
    if (activationMode == MemberRestoreActivation::PreserveCurrent) {
        if (preservedActivation && !preservedActivation->isDeleted()
            && !preservedActivation->isMinimized()
            && !preservedActivation->isHidden()) {
            KWin::workspace()->activateWindow(preservedActivation);
        }
    } else if (baselineActivation) {
        KWin::workspace()->activateWindow(baselineActivation);
    }
    return true;
}

void KWinMemberPolicyPlatform::setChromeVisible(const QString &containerId, bool visible)
{
    m_chrome.setOverlayVisible(containerId, visible);
}

} // namespace QindaQt::Compositor::KWinIntegration
