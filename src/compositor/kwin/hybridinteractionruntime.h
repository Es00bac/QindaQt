// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/topologycoordinator.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

enum class HybridRuntimeStatus {
    PreviewOnly,
    Delegated,
    TopologyCommitted,
    NoChange,
    NeedsGeometry,
    Rejected,
    Unsupported,
};

struct HybridRuntimeResult final
{
    [[nodiscard]] bool topologyChanged() const noexcept
    {
        return status == HybridRuntimeStatus::TopologyCommitted;
    }

    HybridRuntimeStatus status = HybridRuntimeStatus::Rejected;
    std::optional<Hybrid::TopologyCommandResult> topologyResult;
    QString message;
};

struct DirectInteractionResult final
{
    [[nodiscard]] static DirectInteractionResult handled(QString message = {})
    {
        return {true, std::move(message)};
    }
    [[nodiscard]] static DirectInteractionResult rejected(QString message)
    {
        return {false, std::move(message)};
    }

    bool accepted = false;
    QString message;
};

using PreviewSink = std::function<void(const HybridInput::InteractionIntent &)>;
using DirectInteractionHandler =
    std::function<DirectInteractionResult(const HybridInput::InteractionIntent &)>;

struct DividerGeometryResult final
{
    [[nodiscard]] static DividerGeometryResult resolved(double ratio)
    {
        return {ratio, {}};
    }
    [[nodiscard]] static DividerGeometryResult unavailable(QString message = {})
    {
        return {std::nullopt, std::move(message)};
    }

    std::optional<double> ratio;
    QString message;
};

using DividerGeometryHandler =
    std::function<DividerGeometryResult(const HybridInput::InteractionIntent &)>;

struct HybridRuntimeCallbacks final
{
    PreviewSink preview;
    DirectInteractionHandler containerMove;
    DividerGeometryHandler dividerResize;
    DirectInteractionHandler containerResize;
};

struct ReleaseAllResult final
{
    bool complete = true;
    QVector<Hybrid::TopologyCommandResult> commands;
    QString message;
};

// This class owns session-wide Hybrid topology state but no KWin objects. The
// scene factory and callbacks must outlive synchronous calls; callers serialize
// all methods on the compositor thread and must not reenter from callbacks.
class HybridInteractionRuntime final
{
public:
    HybridInteractionRuntime(QStringList initialIndependentWindows,
                             Hybrid::SceneTransactionFactory &sceneFactory,
                             HybridRuntimeCallbacks callbacks = {});

    [[nodiscard]] bool ready() const noexcept { return m_initializationError.isEmpty(); }
    [[nodiscard]] const QString &initializationError() const noexcept
    {
        return m_initializationError;
    }
    [[nodiscard]] const Hybrid::WindowTopology &topology() const noexcept
    {
        return m_repository.topology();
    }
    [[nodiscard]] QString activePageFirstSplitId(
        const QString &containerId) const
    {
        const auto *container = topology().container(containerId);
        const auto *page = container ? container->page(container->activePageId()) : nullptr;
        // AGENT-CONTRACT: A page containing any split has a split root; its
        // root ID is therefore the deterministic first pre-order split.
        return page && page->root().isSplit() ? page->root().id() : QString{};
    }

    [[nodiscard]] HybridRuntimeResult addWindow(const QString &windowId);
    [[nodiscard]] HybridRuntimeResult forgetWindow(const QString &windowId);
    [[nodiscard]] HybridRuntimeResult activatePage(const QString &containerId,
                                                   const QString &pageId);
    [[nodiscard]] HybridRuntimeResult reorderPage(const QString &containerId,
                                                  const QString &pageId,
                                                  qsizetype destinationPageIndex);
    [[nodiscard]] HybridRuntimeResult resizeSplit(const QString &containerId,
                                                  const QString &splitNodeId,
                                                  double ratio);
    [[nodiscard]] HybridRuntimeResult handleIntent(
        const HybridInput::InteractionIntent &intent);
    [[nodiscard]] HybridRuntimeResult releaseContainer(const QString &containerId);
    [[nodiscard]] ReleaseAllResult releaseAll();

private:
    struct Placement final
    {
        bool known = false;
        QString containerId;
    };

    struct PageLocation final
    {
        QString pageId;
        qsizetype index = 0;
    };

    [[nodiscard]] HybridRuntimeResult commitMemberDock(
        const HybridInput::InteractionIntent &intent);
    [[nodiscard]] HybridRuntimeResult commitIndependentDock(
        const HybridInput::InteractionIntent &intent,
        const Placement &targetPlacement);
    [[nodiscard]] HybridRuntimeResult commitGroupedDock(
        const HybridInput::InteractionIntent &intent,
        const QString &sourceContainerId,
        const Placement &targetPlacement);
    [[nodiscard]] HybridRuntimeResult commitWithinContainer(
        const HybridInput::InteractionIntent &intent,
        const QString &containerId);
    [[nodiscard]] HybridRuntimeResult commitAcrossContainers(
        const HybridInput::InteractionIntent &intent,
        const QString &sourceContainerId,
        const QString &targetContainerId);
    [[nodiscard]] HybridRuntimeResult execute(Hybrid::TopologyCommand command);
    [[nodiscard]] HybridRuntimeResult delegate(
        const HybridInput::InteractionIntent &intent,
        const DirectInteractionHandler &handler,
        QString operationName);
    [[nodiscard]] Placement placement(const QString &windowId) const;
    [[nodiscard]] std::optional<PageLocation> pageLocation(
        const QString &containerId, const QString &windowId) const;
    [[nodiscard]] QString structuralId(QLatin1StringView role) const;

    QString m_initializationError;
    Hybrid::TopologyRepository m_repository;
    Hybrid::TopologyCoordinator m_coordinator;
    HybridRuntimeCallbacks m_callbacks;
};

} // namespace QindaQt::Compositor::KWinIntegration
