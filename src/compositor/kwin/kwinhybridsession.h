// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

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
class KWinTaskIdentityManager;
class KWinTransientManager;
class ManagedWindowRegistry;

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
    [[nodiscard]] std::optional<QJsonObject>
    publicSnapshot(const QString &containerId) const;

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
    [[nodiscard]] bool dispatchGroupWindowAction(
        const QString &containerId,
        HybridChrome::WindowAction action,
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
    [[nodiscard]] bool requestCloseContainer(const QString &containerId,
                                             QString *error = nullptr);
    void closeAllMembers(const QString &containerId);
    [[nodiscard]] bool detachNativeMember(const QString &containerId,
                                          const QString &windowId,
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
    QSet<QString> m_minimizedContainers;
    QString m_lastGroupStackingFailure;
    bool m_synchronizingChrome = false;
    bool m_applyingWindowAction = false;
    bool m_shutdown = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
