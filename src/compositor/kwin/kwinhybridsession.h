// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontypes.h"
#include "qindaqt/compositor/shellwindowactions.h"
#include "hybridtaskidentitypolicy.h"

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace QindaQt::HybridInput {
class InteractionController;
}

namespace QindaQt::Compositor::KWinIntegration {

class HybridChromeDragTranslator;
class HybridChromeAccessibilityRegistry;
class HybridChromePointerRouter;
class HybridChromeSyncScheduler;
struct ChromePointerDecision;
class ContainerClosePrompt;
enum class ContainerCloseDecision;
class HybridContainerPlacementController;
class HybridInteractionRuntime;
class HybridShortcutManager;
enum class HybridSemanticCommand;
struct HybridSemanticRequest;
class KWinChromeManager;
class KWinChromeSceneLifecycle;
class KWinDockPreview;
class KWinGroupContextMenu;
class KWinGroupContextManager;
class KWinHybridSceneFactory;
class KWinHybridGroupStacking;
class KWinInteractionFilter;
class KWinInteractionTargetResolver;
class KWinMemberPolicyManager;
enum class NativeQuickTileEdge;
class KWinTaskIdentityManager;
class KWinTransientManager;
class ManagedWindowRegistry;
class MemberChromeVisibilityController;

// Owns the production Hybrid collaborator graph for one KWin plugin lifetime.
// The registry is borrowed and must outlive this object. All calls and Qt
// signals are serialized on KWin's compositor/GUI thread.
class KWinHybridSession final : public QObject
{
    Q_OBJECT

public:
    explicit KWinHybridSession(ManagedWindowRegistry &registry,
                               QObject *parent = nullptr);
    ~KWinHybridSession() override;

    KWinHybridSession(const KWinHybridSession &) = delete;
    KWinHybridSession &operator=(const KWinHybridSession &) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool inputFilterInstalled() const noexcept;
    [[nodiscard]] quint64 topologyRevision() const noexcept;
    [[nodiscard]] qsizetype containerCount() const noexcept;
    [[nodiscard]] bool isContainerMaximized(const QString &containerId) const noexcept;
    [[nodiscard]] QJsonObject diagnostics() const;
    [[nodiscard]] QJsonArray publicContainers() const;
    [[nodiscard]] QVector<TaskContainerIdentity> taskIdentityPlans() const;
    [[nodiscard]] std::optional<QJsonObject>
    publicSnapshot(const QString &containerId) const;
    [[nodiscard]] bool executeShellWindowAction(
        const QString &windowId,
        ShellWindowAction action,
        QString *error = nullptr);
    void setChromePalette(const HybridChrome::ChromePalette &palette);

    // Idempotent. Restores every Hybrid-owned client before destroying scene,
    // chrome, input, and shortcut collaborators.
    void shutdown() noexcept;

Q_SIGNALS:
    // Group maximize is compositor-owned placement state and does not change a
    // member Window::maximizeMode. Consumers of public window state must
    // invalidate when this signal fires.
    void shellVisibilityStateChanged();

private:
    struct ActiveKeyboardContext final
    {
        QString windowId;
        QString containerId;
    };

    void dispatchIntent(const HybridInput::InteractionIntent &intent);
    void dispatchChromePointerDecision(const ChromePointerDecision &decision);
    void handleChromeDrag(const QString &containerId,
                          const HybridChrome::ChromeDragEvent &event);
    void handleWindowAction(const QString &containerId,
                            HybridChrome::WindowAction action);
    void handleContainerControl(const QString &containerId,
                                HybridChrome::ContainerControl control);
    void initializeMemberChromeSupport();
    void synchronizeMemberChromeVisibility();
    [[nodiscard]] bool memberTitlesVisible(
        const QString &containerId) const noexcept;
    void restoreMemberChromeVisibilityForShutdown() noexcept;
    [[nodiscard]] bool dispatchGroupWindowAction(
        const QString &containerId,
        HybridChrome::WindowAction action,
        QString *error = nullptr);
    [[nodiscard]] bool dispatchContainerControl(
        const QString &containerId,
        HybridChrome::ContainerControl control,
        QString *error = nullptr);
    [[nodiscard]] bool restoreMemberFocusForInteraction(
        QString *error = nullptr);
    [[nodiscard]] bool restoreMemberFocusForLifecycleChange(
        QString *error = nullptr);
    void handleTabActivation(const QString &containerId, const QString &pageId);
    void startKeyboardDock();
    void startKeyboardMove();
    void startKeyboardDividerResize();
    void startKeyboardContainerResize();
    void toggleActiveMemberChrome();
    [[nodiscard]] bool beginArrangeWindows(const QString &containerId,
                                           const QString &windowId,
                                           QString *error = nullptr);
    void initializeTaskIdentityAndShortcuts();
    void synchronizeTaskIdentity();
    void shutdownTaskIdentity() noexcept;
    void dispatchSemanticShortcut(HybridSemanticCommand command);
    [[nodiscard]] bool dispatchSemanticRequest(
        const HybridSemanticRequest &request,
        QString *error = nullptr);
    void synchronizeAccessibility();
    void shutdownAccessibility() noexcept;
    void addManagedWindow(const QString &windowId);
    void forgetManagedWindow(const QString &windowId);
    void handleWindowsChanged();
    void initializeGroupContextMenu();
    void showGroupContextMenu(const QString &containerId,
                              const QPointF &globalPosition);
    void adoptMemberContext(const QString &containerId,
                            const QString &sourceWindowId);
    void invalidateChromePublication();
    void synchronizeChrome();
    void reconcileMinimizedContainers();
    void minimizeContainer(const QString &containerId);
    [[nodiscard]] bool unminimizeContainer(const QString &containerId,
                                           QString *error = nullptr);
    [[nodiscard]] bool requestCloseContainer(const QString &containerId,
                                             QString *error = nullptr);
    [[nodiscard]] bool ungroupContainer(const QString &containerId,
                                        QString *error = nullptr);
    void closeAllMembers(const QString &containerId);
    [[nodiscard]] bool detachNativeMember(const QString &containerId,
                                          const QString &windowId,
                                          QString *error = nullptr);
    // Reactive enforcement seam: a grouped member's native quick-tile request
    // (bare Meta+Arrow) was already reverted by the caller before this runs.
    // Redirects the requested direction through the existing within-container
    // dock commands instead, or is a deterministic no-op when no same-
    // container sibling exists in that direction.
    [[nodiscard]] bool handleNativeMemberQuickTile(const QString &containerId,
                                                   const QString &windowId,
                                                   NativeQuickTileEdge edge,
                                                   QString *error = nullptr);
    void handleCloseDecision(const QString &containerId,
                             ContainerCloseDecision decision);

    [[nodiscard]] qreal containerScale(const QString &containerId) const;
    [[nodiscard]] QRect workArea(const QString &containerId) const;
    [[nodiscard]] QStringList containerStackingOrder() const;
    [[nodiscard]] std::optional<QRectF> dockTargetFrame(
        const HybridInput::DockTarget &target) const;
    [[nodiscard]] std::optional<ActiveKeyboardContext>
    activeKeyboardContext(QLatin1StringView operation) const;

    ManagedWindowRegistry &m_registry;
    std::unique_ptr<KWinHybridSceneFactory> m_sceneFactory;
    std::unique_ptr<HybridInteractionRuntime> m_runtime;
    std::unique_ptr<KWinChromeManager> m_chromeManager;
    std::unique_ptr<MemberChromeVisibilityController> m_memberChromeVisibility;
    std::unique_ptr<KWinChromeSceneLifecycle> m_chromeSceneLifecycle;
    std::unique_ptr<KWinHybridGroupStacking> m_groupStacking;
    std::unique_ptr<KWinGroupContextManager> m_groupContext;
    std::unique_ptr<KWinGroupContextMenu> m_groupContextMenu;
    std::unique_ptr<HybridChromeSyncScheduler> m_chromeSyncScheduler;
    std::unique_ptr<KWinMemberPolicyManager> m_memberPolicy;
    std::unique_ptr<KWinTaskIdentityManager> m_taskIdentity;
    std::unique_ptr<HybridChromeAccessibilityRegistry> m_accessibility;
    std::unique_ptr<KWinTransientManager> m_transientManager;
    std::unique_ptr<KWinInteractionTargetResolver> m_targetResolver;
    std::unique_ptr<HybridChromeDragTranslator> m_dragTranslator;
    std::unique_ptr<HybridContainerPlacementController> m_placement;
    std::unique_ptr<HybridInput::InteractionController> m_interactionController;
    std::unique_ptr<HybridChromePointerRouter> m_chromePointerRouter;
    std::unique_ptr<KWinDockPreview> m_dockPreview;
    std::unique_ptr<KWinInteractionFilter> m_inputFilter;
    std::unique_ptr<HybridShortcutManager> m_shortcuts;
    std::unique_ptr<ContainerClosePrompt> m_closePrompt;
    HybridChrome::ChromePalette m_chromePalette;
    QSet<QString> m_minimizedContainers;
    QString m_lastGroupStackingFailure;
    bool m_synchronizingChrome = false;
    bool m_applyingWindowAction = false;
    bool m_shutdown = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
