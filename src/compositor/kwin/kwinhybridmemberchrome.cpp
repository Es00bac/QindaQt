// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwingroupcontextmenu.h"
#include "kwinhybridsession.h"
#include "kwinworkspacecontroller.h"

#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "managedwindowregistry.h"
#include "memberchromevisibilitycontroller.h"

#include <KDecoration3/Decoration>
#include <window.h>

#include <QVariantMap>

namespace QindaQt::Compositor::KWinIntegration {

void KWinHybridSession::initializeMemberChromeSupport()
{
    m_chromeManager = std::make_unique<KWinChromeManager>(m_registry);
    m_memberChromeVisibility =
        std::make_unique<MemberChromeVisibilityController>(
            [this](const QString &windowId)
                -> std::optional<MemberChromeInspection> {
                const auto *window = m_registry.window(windowId);
                if (!window) {
                    return std::nullopt;
                }
                return MemberChromeInspection{
                    .noBorder = window->noBorder(),
                    .serverDecorated = window->decoration() != nullptr,
                    .userCanSetNoBorder = window->userCanSetNoBorder(),
                };
            },
            [this](const QString &windowId, bool noBorder, QString *error) {
                auto *window = m_registry.window(windowId);
                if (!window) {
                    // Closing a client completes restoration by destroying the
                    // presentation state. Treat that lifecycle race as done.
                    return true;
                }
                window->setNoBorder(noBorder);
                if (window->noBorder() == noBorder) {
                    return true;
                }
                if (error) {
                    *error = QStringLiteral(
                        "the compositor did not accept the native title change");
                }
                return false;
            });
    connect(m_chromeManager.get(), &KWinChromeManager::containerControlRequested,
            this, &KWinHybridSession::handleContainerControl);
    connect(m_chromeManager.get(), &KWinChromeManager::chromePlansPublished,
            this, &KWinHybridSession::publishMemberChromeIdentity);
}

void KWinHybridSession::publishMemberChromeIdentity()
{
    // ADR-0139: per-window decoration emphasis. The published chrome plans
    // are the single authority for both the container identity color and the
    // focused member, so publication rides the manager's post-snapshot hook
    // and stays in lockstep with every chrome synchronization.
    if (!m_chromeManager || !m_runtime || m_shutdown) {
        return;
    }
    static constexpr auto MemberIdentityProperty = "qindaqtMemberIdentity";
    for (const auto &windowId : m_registry.windowIds()) {
        QVariantMap identity;
        const auto owner = m_runtime->topology().ownerOf(windowId);
        if (owner) {
            const auto plan = m_chromeManager->plan(*owner);
            if (plan) {
                if (plan->identityColor.isValid()) {
                    identity.insert(QStringLiteral("identityColor"),
                                    plan->identityColor);
                }
                bool focused = false;
                for (const auto &member : plan->members) {
                    focused = focused
                        || (member.memberId == windowId && member.focused);
                }
                identity.insert(QStringLiteral("focused"), focused);
            }
        }
        auto *window = m_registry.window(windowId);
        auto *decoration = window ? window->decoration() : nullptr;
        if (!decoration) {
            continue;
        }
        const auto current = decoration->property(MemberIdentityProperty);
        if (identity.isEmpty()) {
            if (current.isValid()) {
                decoration->setProperty(MemberIdentityProperty, QVariant());
                decoration->update();
            }
            continue;
        }
        if (current == identity) {
            continue;
        }
        decoration->setProperty(MemberIdentityProperty, identity);
        decoration->update();
    }
}

void KWinHybridSession::synchronizeMemberChromeVisibility()
{
    if (!m_memberChromeVisibility) {
        return;
    }
    QString error;
    if (!m_memberChromeVisibility->synchronize(m_runtime->topology(), &error)) {
        qWarning("QindaQt native member title synchronization failed: %s",
                 qPrintable(error));
    }
}

bool KWinHybridSession::memberTitlesVisible(const QString &containerId) const noexcept
{
    return !m_memberChromeVisibility
        || m_memberChromeVisibility->visible(containerId);
}

void KWinHybridSession::restoreMemberChromeVisibilityForShutdown() noexcept
{
    if (!m_memberChromeVisibility) {
        return;
    }
    QString error;
    if (!m_memberChromeVisibility->restoreForShutdown(&error)) {
        qWarning("QindaQt could not restore native member titles: %s",
                 qPrintable(error));
    }
    m_memberChromeVisibility.reset();
}


void KWinHybridSession::setChromePalette(const HybridChrome::ChromePalette &palette)
{
    m_chromeStyle.palette = palette;
    synchronizeChrome();
}

void KWinHybridSession::setChromeStyle(const HybridChrome::ChromeStyle &style)
{
    m_chromeStyle = style;
    synchronizeChrome();
}

void KWinHybridSession::setNativePalette(const QPalette &palette)
{
    m_nativePalette = palette;
    if (m_workspaceController) {
        m_workspaceController->setPalette(palette);
    }
    if (m_groupContextMenu) {
        m_groupContextMenu->setPalette(m_nativePalette);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
