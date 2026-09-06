// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwingroupcontextmenu.h"
#include "kwinhybridsession.h"

#include "hybridinteractionruntime.h"
#include "kwinchromemanager.h"
#include "managedwindowregistry.h"
#include "memberchromevisibilitycontroller.h"

#include <window.h>

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
    m_chromePalette = palette;
    synchronizeChrome();
}

void KWinHybridSession::setNativePalette(const QPalette &palette)
{
    m_nativePalette = palette;
    if (m_groupContextMenu) {
        m_groupContextMenu->setPalette(m_nativePalette);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
