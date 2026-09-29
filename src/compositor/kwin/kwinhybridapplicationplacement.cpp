// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridiconifycontroller.h"
#include "hybridinteractionruntime.h"
#include "kwinapplicationplacementserver.h"
#include "kwinhybridsession.h"
#include "kwinmemberpolicy.h"
#include "managedwindowregistry.h"
#include <QUuid>
#include <qindaqt/application_window_management/placement.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
namespace QindaQt::Compositor::KWinIntegration {
void KWinHybridSession::initializeApplicationPlacement() {
  if (!KWin::waylandServer() || !ready())
    return;
  m_applicationPlacement = std::make_unique<KWinApplicationPlacementServer>(
      m_registry,
      [this](const QString &sourceId, const QString &createdId,
             ApplicationWindowManagement::Placement placement) {
        using ApplicationWindowManagement::Status;
        const auto refuse = [](QString message) {
          return ApplicationPlacementResult{
              Status::Denied, {}, std::move(message)};
        };
        auto *source = m_registry.window(sourceId),
             *created = m_registry.window(createdId);
        if (m_shutdown || !ready() || !source || !created ||
            source->isDeleted() || created->isDeleted() ||
            !source->isNormalWindow() || !created->isNormalWindow() ||
            source->isMinimized() || created->isMinimized() ||
            source->isFullScreen() || created->isFullScreen() ||
            KWin::waylandServer()->isScreenLocked())
          return refuse(QStringLiteral("normal visible windows are required"));
        auto *active = KWin::workspace()->activeWindow();
        if (active != source && active != created)
          return refuse(QStringLiteral("application is no longer foreground"));
        const QString owner = m_registry.owner(sourceId);
        if (isWindowIconified(sourceId) ||
            (!owner.isEmpty() && (isContainerShaded(owner) ||
                                  m_minimizedContainers.contains(owner))))
          return refuse(
              QStringLiteral("restore the source window before placement"));
        const auto plan = ApplicationWindowManagement::planPlacement(
            m_runtime->topology(), sourceId, createdId, placement,
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!plan.command)
          return ApplicationPlacementResult{Status::Invalid, {}, plan.message};
        // AGENT-GUARD: Scene transactions re-plan every container. Reject a
        // native request while temporary member-focus presentation is active;
        // restoring it before a fallible request would change unrelated state
        // even if placement failed. Ordinary restored windows need no pre-edit.
        if (m_memberPolicy && !m_memberPolicy->focusStates().isEmpty())
          return refuse(QStringLiteral("restore member focus before placement"));
        const auto result = m_runtime->execute(*plan.command);
        if (!result.topologyChanged())
          return ApplicationPlacementResult{
              Status::Unavailable, {}, result.message};
        // The scene commit already propagates the stationary source context,
        // verifies all owner/frame changes, and activates the created subject.
        // A second adoption or activation after publication could fail outside
        // rollback, falsely reporting a rejected request that changed topology.
        synchronizeChrome();
        Q_EMIT shellVisibilityStateChanged();
        return ApplicationPlacementResult{
            Status::Accepted, plan.containerId, {}};
      },
      this);
}
void KWinHybridSession::shutdownApplicationPlacement() noexcept {
  m_applicationPlacement.reset();
}
} // namespace QindaQt::Compositor::KWinIntegration
