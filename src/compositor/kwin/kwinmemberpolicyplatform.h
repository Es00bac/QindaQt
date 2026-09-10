// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kwinmemberpolicy.h"

namespace QindaQt::Compositor::KWinIntegration {

class KWinChromeManager;
class ManagedWindowRegistry;

// Atomic platform seam of KWinMemberPolicyManager against live KWin windows.
// Split out of kwinmemberpolicy.cpp to keep both translation units under the
// decomposition limit; this class is private to the KWin plugin target.
class KWinMemberPolicyPlatform final : public HybridMemberPolicyPlatform
{
public:
    KWinMemberPolicyPlatform(ManagedWindowRegistry &registry,
                             KWinChromeManager &chrome,
                             NativeMemberDetach detach);

    [[nodiscard]] bool detachMember(const QString &containerId,
                                    const QString &windowId,
                                    const MemberGroupBaseline *focusBaseline,
                                    QString *error = nullptr) override;
    [[nodiscard]] bool enterFocus(const MemberGroupBaseline &baseline,
                                  const QString &windowId,
                                  MemberFocusMode mode,
                                  QString *error = nullptr) override;
    [[nodiscard]] bool restoreRejectedPresentation(
        const MemberGroupBaseline &baseline,
        const QString &windowId,
        const QString &focusOwnerWindowId,
        MemberFocusMode mode,
        QString *error = nullptr) override;
    [[nodiscard]] bool restoreGroup(const MemberGroupBaseline &baseline,
                                    const QString &minimizeWindowId,
                                    const QSet<QString> &missingWindowIds,
                                    MemberRestoreActivation activation,
                                    QString *error = nullptr) override;

    void setChromeVisible(const QString &containerId, bool visible);

private:
    ManagedWindowRegistry &m_registry;
    KWinChromeManager &m_chrome;
    NativeMemberDetach m_detach;
};

} // namespace QindaQt::Compositor::KWinIntegration
