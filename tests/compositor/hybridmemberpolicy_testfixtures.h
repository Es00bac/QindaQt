// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridmemberpolicy.h"

#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration::Test {

enum class CallKind {
    Detach,
    Enter,
    Reject,
    Restore,
};

struct PlatformCall final
{
    CallKind kind = CallKind::Detach;
    MemberGroupBaseline baseline;
    QString windowId;
    QString focusOwnerWindowId;
    std::optional<MemberFocusMode> mode;
    QSet<QString> missing;
    MemberRestoreActivation activation = MemberRestoreActivation::RestoreBaseline;
};

class FakePlatform final : public HybridMemberPolicyPlatform
{
public:
    bool detachMember(const QString &containerId,
                      const QString &windowId,
                      const MemberGroupBaseline *focusBaseline,
                      QString *error) override
    {
        MemberGroupBaseline identity;
        identity.containerId = containerId;
        calls.append({CallKind::Detach,
                      focusBaseline ? *focusBaseline : identity,
                      windowId, {}, {}, {}});
        if (!accept(error)) {
            return false;
        }
        if (onDetach) {
            onDetach();
        }
        // Production consumes these borrowed values after its synchronous
        // topology callback. Reading them here makes lifetime regressions
        // deterministic under ASan instead of relying on allocator reuse.
        postDetachContainerId = containerId;
        if (focusBaseline) {
            postDetachFocusBaseline = *focusBaseline;
        }
        return true;
    }

    bool enterFocus(const MemberGroupBaseline &baseline,
                    const QString &windowId,
                    MemberFocusMode mode,
                    QString *error) override
    {
        calls.append({CallKind::Enter, baseline, windowId, {}, mode, {}});
        return accept(error);
    }

    bool restoreRejectedPresentation(const MemberGroupBaseline &baseline,
                                     const QString &windowId,
                                     const QString &focusOwnerWindowId,
                                     MemberFocusMode mode,
                                     QString *error) override
    {
        calls.append({CallKind::Reject, baseline, windowId, focusOwnerWindowId, mode, {}});
        if (onReject) {
            onReject();
        }
        return accept(error);
    }

    bool restoreGroup(const MemberGroupBaseline &baseline,
                      const QString &minimizeWindowId,
                      const QSet<QString> &missingWindowIds,
                      MemberRestoreActivation activation,
                      QString *error) override
    {
        calls.append({CallKind::Restore, baseline, minimizeWindowId, {}, {},
                      missingWindowIds, activation});
        return accept(error);
    }

    bool accept(QString *error)
    {
        if (!failNext) {
            return true;
        }
        failNext = false;
        if (error) {
            *error = QStringLiteral("injected platform failure");
        }
        return false;
    }

    QVector<PlatformCall> calls;
    std::function<void()> onDetach;
    std::function<void()> onReject;
    QString postDetachContainerId;
    std::optional<MemberGroupBaseline> postDetachFocusBaseline;
    bool failNext = false;
};

inline MemberGroupBaseline secondGroup()
{
    return {
        .containerId = QStringLiteral("second"),
        .outerFrame = QRectF(1100.0, 40.0, 800.0, 600.0),
        .members = {
            {.windowId = QStringLiteral("second-left"),
             .frame = QRectF(1101.0, 109.0, 399.0, 530.0),
             .active = false,
             .activePage = true},
            {.windowId = QStringLiteral("second-right"),
             .frame = QRectF(1502.0, 109.0, 397.0, 530.0),
             .activePage = true},
            {.windowId = QStringLiteral("second-other-page"),
             .frame = QRectF(1200.0, 200.0, 500.0, 300.0),
             .minimized = true},
        },
    };
}

inline MemberGroupBaseline group()
{
    return {
        .containerId = QStringLiteral("group"),
        .outerFrame = QRectF(10.0, 20.0, 1000.0, 700.0),
        .members = {
            {.windowId = QStringLiteral("left"),
             .frame = QRectF(11.0, 89.0, 499.0, 630.0),
             .active = true,
             .activePage = true},
            {.windowId = QStringLiteral("right"),
             .frame = QRectF(512.0, 89.0, 497.0, 630.0),
             .activePage = true},
            {.windowId = QStringLiteral("other-page"),
             .frame = QRectF(90.0, 110.0, 600.0, 400.0),
             .minimized = true},
        },
    };
}

} // namespace QindaQt::Compositor::KWinIntegration::Test
