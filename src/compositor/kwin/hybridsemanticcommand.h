// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

enum class HybridSemanticCommand {
    BeginPageDock,
    ActivateNextPage,
    ActivatePreviousPage,
    ReorderPageNext,
    ReorderPagePrevious,
    CloseGroup,
    MinimizeGroup,
    MaximizeGroup,
    RestoreGroup,
};

enum class HybridSemanticRequestKind {
    BeginPageDock,
    ActivatePage,
    ReorderPage,
    GroupWindowAction,
};

// A resolved request contains stable topology IDs and no pointer coordinates or
// KWin objects. Shortcuts, accessibility, and future shell commands share this
// value so they cannot drift into subtly different window-management policy.
struct HybridSemanticRequest final
{
    HybridSemanticRequestKind kind = HybridSemanticRequestKind::ActivatePage;
    QString containerId;
    QString pageId;
    qsizetype destinationPageIndex = -1;
    HybridInput::HitTarget dockSource;
    std::optional<HybridChrome::WindowAction> windowAction;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;
    friend bool operator==(const HybridSemanticRequest &,
                           const HybridSemanticRequest &) = default;
};

class HybridSemanticCommandResolver final
{
public:
    // Resolves against a borrowed immutable topology snapshot. The caller must
    // dispatch before publishing another revision; runtime ownership checks are
    // still the final stale-request guard.
    [[nodiscard]] static std::optional<HybridSemanticRequest> resolveActive(
        const Hybrid::WindowTopology &topology,
        const QString &activeWindowId,
        HybridSemanticCommand command,
        QString *error = nullptr);

    [[nodiscard]] static std::optional<HybridSemanticRequest> activatePage(
        const Hybrid::WindowTopology &topology,
        const QString &containerId,
        const QString &pageId,
        QString *error = nullptr);
    [[nodiscard]] static std::optional<HybridSemanticRequest> reorderPage(
        const Hybrid::WindowTopology &topology,
        const QString &containerId,
        const QString &pageId,
        qsizetype destinationPageIndex,
        QString *error = nullptr);
    [[nodiscard]] static std::optional<HybridSemanticRequest> groupWindowAction(
        const Hybrid::WindowTopology &topology,
        const QString &containerId,
        HybridChrome::WindowAction action,
        QString *error = nullptr);
};

struct HybridSemanticCommandHandlers final
{
    std::function<bool(const HybridInput::HitTarget &, QString *)> beginPageDock;
    std::function<bool(const QString &, const QString &, QString *)> activatePage;
    std::function<bool(const QString &, const QString &, qsizetype, QString *)>
        reorderPage;
    std::function<bool(const QString &, HybridChrome::WindowAction, QString *)>
        groupWindowAction;
};

// Handlers are borrowed callable values and run synchronously on the caller's
// thread. A false result must not partially publish topology; the existing
// runtime/scene transaction remains responsible for that atomicity.
class HybridSemanticCommandDispatcher final
{
public:
    explicit HybridSemanticCommandDispatcher(HybridSemanticCommandHandlers handlers);

    [[nodiscard]] bool dispatch(const HybridSemanticRequest &request,
                                QString *error = nullptr) const;

private:
    HybridSemanticCommandHandlers m_handlers;
};

} // namespace QindaQt::Compositor::KWinIntegration
