// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridgroupcontext.h"

#include "qindaqt/hybrid/topologyscene.h"
#include "qindaqt/hybrid_constraints/constraint_solution.h"
#include "qindaqt/hybrid_constraints/member_size_constraints.h"
#include "qindaqt/hybrid_constraints/window_restore_state.h"

#include <QHash>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

class KWinHybridSceneTransaction;
class ManagedWindowRegistry;

// The narrow platform seam keeps transaction planning testable without a KWin
// workspace. Production uses the registry-backed implementation created by the
// factory's ManagedWindowRegistry constructor. Injected implementations are
// borrowed and must outlive the factory and all transactions it creates.
class KWinHybridScenePlatform
{
public:
    virtual ~KWinHybridScenePlatform() = default;

    [[nodiscard]] virtual QStringList windowIds() const = 0;
    [[nodiscard]] virtual bool windowExists(const QString &windowId) const = 0;
    [[nodiscard]] virtual QString owner(const QString &windowId) const = 0;
    [[nodiscard]] virtual std::optional<QRectF> currentFrame(
        const QString &windowId,
        QString *error = nullptr) const = 0;
    // Resolves a live work area by stable output identity. Implementations
    // must not retain output objects across calls because hotplug destroys them.
    [[nodiscard]] virtual std::optional<QRectF> placementArea(
        const QString &windowId,
        const QString &outputId,
        QString *error = nullptr) const = 0;
    [[nodiscard]] virtual std::optional<HybridConstraints::WindowRestoreState>
    captureState(const QString &windowId, QString *error = nullptr) const = 0;
    [[nodiscard]] virtual std::optional<HybridConstraints::MemberSizeConstraints>
    sizeConstraints(const QString &windowId, QString *error = nullptr) const = 0;
    [[nodiscard]] virtual bool validateState(
        const QString &windowId,
        const HybridConstraints::WindowRestoreState &state,
        QString *error = nullptr) const = 0;
    // AGENT-CONTRACT: Focus is applied once after every other member state.
    // Validate every fallible precondition before mutating, and leave focus
    // untouched here even when state.focused is true; rollback counts only
    // calls that returned success.
    [[nodiscard]] virtual bool applyState(
        const QString &windowId,
        const HybridConstraints::WindowRestoreState &state,
        QString *error = nullptr) = 0;
    // AGENT-CONTRACT: This is also the opaque rollback focus token. Production
    // returns it for every active KWin window, including dialogs and other
    // clients deliberately absent from ManagedWindowRegistry. A managed
    // window's token remains its topology ID for candidate-member comparison.
    [[nodiscard]] virtual QString activeWindowId() const = 0;
    [[nodiscard]] virtual bool activateWindow(const QString &windowId,
                                              QString *error = nullptr) = 0;
    // AGENT-CONTRACT: Rollback passes the exact pre-transaction focus token.
    // Resolve non-empty tokens against all live compositor windows, not the
    // managed-member registry. An empty token (or a target that closed before
    // rollback) clears focus; it never means "leave current focus".
    [[nodiscard]] virtual bool restoreFocus(const QString &focusToken,
                                            QString *error = nullptr) = 0;
    // AGENT-CONTRACT: Validate the complete owner/frame plan before its first
    // edit. A false result must leave every owner and target frame unchanged.
    [[nodiscard]] virtual bool finalizeOwners(
        const QHash<QString, QString> &expectedOwners,
        const QHash<QString, QString> &candidateOwners,
        const QHash<QString, QRectF> &targetFrames,
        const QSet<QString> &allowedMissingWindowIds,
        QString *error = nullptr) = 0;
};

// AGENT-NOTE: Private construction seam implemented beside the concrete KWin
// adapter. This local header is not installed; consumers construct the scene
// factory instead of depending on the platform implementation.
[[nodiscard]] std::unique_ptr<KWinHybridScenePlatform>
makeRegistryHybridScenePlatform(ManagedWindowRegistry &registry);

struct CommittedContainerLayout final
{
    QRect outerFrame;
    HybridConstraints::ConstraintSolution activePage;

    friend bool operator==(const CommittedContainerLayout &,
                           const CommittedContainerLayout &) = default;
};

class KWinHybridSceneFactory final : public Hybrid::SceneTransactionFactory
{
public:
    explicit KWinHybridSceneFactory(
        ManagedWindowRegistry &registry,
        HybridConstraints::LayoutMetrics metrics = {});
    explicit KWinHybridSceneFactory(
        KWinHybridScenePlatform &platform,
        HybridConstraints::LayoutMetrics metrics = {});
    ~KWinHybridSceneFactory() override;

    [[nodiscard]] std::unique_ptr<Hybrid::SceneTransaction> create() override;
    // Synchronous compositor-thread operation for interactive outer-title
    // move/maximize; callers serialize it with coordinator execution. The
    // container is borrowed only for this call and must be the current
    // repository snapshot. Success re-solves every page, updates
    // member state and unchanged registry owners, then publishes the copied
    // active-page layout. Failure rolls back applied state and focus; KWin
    // configure acknowledgement remains asynchronous at the protocol boundary.
    [[nodiscard]] Hybrid::SceneStepResult reflowContainer(
        const Core::WindowContainer &container,
        const QRect &outerFrame);
    // Adopts one member's post-KWin context as container state. The queued
    // signal adapter re-resolves source ownership before calling; this method
    // atomically maps the complete outer frame when the output changed, solves
    // all pages, propagates context, verifies effective state, and rolls back
    // without altering independent restore snapshots on failure.
    [[nodiscard]] Hybrid::SceneStepResult recontextualizeContainer(
        const Core::WindowContainer &container,
        const QString &sourceWindowId);
    // Last-resort plugin-unload path. It restores every still-live grouped
    // window from the factory-owned independent snapshot and clears registry
    // ownership without publishing another soon-to-be-destroyed topology.
    // Call only after normal release retries have failed.
    [[nodiscard]] Hybrid::SceneStepResult emergencyReleaseAll(
        const Hybrid::WindowTopology &topology);
    // AGENT-CONTRACT: Chrome consumes a copied committed value. The scene
    // boundary never exposes a topology object, transaction, or KWin window.
    [[nodiscard]] std::optional<CommittedContainerLayout> committedLayout(
        const QString &containerId) const;
    [[nodiscard]] std::optional<HybridConstraints::OverflowReport> overflowReport(
        const QString &containerId) const;
    // Direct KWin signals fire inside applyState(). Lifecycle adapters must
    // ignore those intermediate values until the coordinator publishes the
    // matching topology snapshot.
    [[nodiscard]] bool applyingWindowStates() const noexcept
    {
        return m_windowStateMutationDepth > 0;
    }

private:
    [[nodiscard]] Hybrid::SceneStepResult reflowContainerWithContext(
        const Core::WindowContainer &container,
        const QRect &outerFrame,
        const std::optional<HybridGroupContext> &context,
        const std::optional<HybridGroupContext> &rollbackContext);

    std::unique_ptr<KWinHybridScenePlatform> m_ownedPlatform;
    KWinHybridScenePlatform *m_platform = nullptr;
    HybridConstraints::LayoutMetrics m_metrics;
    QHash<QString, HybridConstraints::WindowRestoreState> m_restoreStates;
    QHash<QString, CommittedContainerLayout> m_committedLayouts;
    qsizetype m_windowStateMutationDepth = 0;

    friend class KWinHybridSceneTransaction;
};

} // namespace QindaQt::Compositor::KWinIntegration
