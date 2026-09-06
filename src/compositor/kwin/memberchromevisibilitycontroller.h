// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/windowtopology.h"

#include <QHash>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct MemberChromeInspection final
{
    bool noBorder = false;
    bool serverDecorated = false;
    bool userCanSetNoBorder = false;
};

struct MemberChromeVisibilitySummary final
{
    qsizetype changedMembers = 0;
    qsizetype unchangedClientDecoratedMembers = 0;
};

using InspectMemberChrome =
    std::function<std::optional<MemberChromeInspection>(const QString &windowId)>;
using SetMemberNoBorder =
    std::function<bool(const QString &windowId, bool noBorder, QString *error)>;

// Owns the session-local native-title visibility choice for grouped windows.
// Platform access is injected so baseline capture and restoration can be tested
// without KWin. The callbacks and controller must share one compositor thread.
class MemberChromeVisibilityController final
{
public:
    MemberChromeVisibilityController(InspectMemberChrome inspect,
                                     SetMemberNoBorder setNoBorder);
    ~MemberChromeVisibilityController();

    MemberChromeVisibilityController(const MemberChromeVisibilityController &) = delete;
    MemberChromeVisibilityController &operator=(
        const MemberChromeVisibilityController &) = delete;

    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   QString *error = nullptr);
    [[nodiscard]] bool setVisible(
        const QString &containerId,
        bool visible,
        MemberChromeVisibilitySummary *summary = nullptr,
        QString *error = nullptr);
    [[nodiscard]] bool toggle(
        const QString &containerId,
        MemberChromeVisibilitySummary *summary = nullptr,
        QString *error = nullptr);

    [[nodiscard]] bool visible(const QString &containerId) const noexcept;
    [[nodiscard]] bool nativeTitleVisible(const QString &containerId,
                                          const QString &windowId) const noexcept;

    // Restores the exact pre-grouping noBorder value. This is idempotent and
    // must run while the injected platform callbacks are still alive.
    [[nodiscard]] bool restoreForShutdown(QString *error = nullptr) noexcept;

private:
    struct MemberBaseline final
    {
        bool noBorder = false;
        bool eligible = false;
        bool clientDecorated = false;
    };

    struct ContainerState final
    {
        bool visible = true;
        QHash<QString, MemberBaseline> members;
    };

    [[nodiscard]] bool restoreMember(const QString &windowId,
                                     const MemberBaseline &baseline,
                                     QString *error);

    InspectMemberChrome m_inspect;
    SetMemberNoBorder m_setNoBorder;
    QHash<QString, ContainerState> m_containers;
};

} // namespace QindaQt::Compositor::KWinIntegration
