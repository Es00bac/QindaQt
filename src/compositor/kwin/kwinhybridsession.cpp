// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridchromedragtranslator.h"
#include "hybridchromeaccessibilityregistry.h"
#include "hybridchromepointerrouter.h"
#include "hybridchromesyncscheduler.h"
#include "hybridchromeplanbuilder.h"
#include "hybridcontainerplacement.h"
#include "hybridgroupedgeometryreconciler.h"
#include "hybridinteractionruntime.h"
#include "hybridshortcutmanager.h"
#include "kwinworkspacecontroller.h"
#include "kwinworkspaceuiport.h"
#include "qindaqt/workspaces_apps/desktop_applications.h"
#include "containercloseprompt.h"
#include "kwinchromemanager.h"
#include "kwinchromescenelifecycle.h"
#include "kwindockpreview.h"
#include "kwingroupcontextmenu.h"
#include "kwingroupcontextmanager.h"
#include "kwinhybridscene.h"
#include "kwinhybridshutdown.h"
#include "kwinhybridgroupstacking.h"
#include "kwininteractionfilter.h"
#include "kwininteractiontargetresolver.h"
#include "kwinmemberpolicy.h"
#include "kwintaskidentitymanager.h"
#include "kwintransientmanager.h"
#include "managedwindowregistry.h"
#include "memberchromevisibilitycontroller.h"
#include "hybridstackingorder.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_input/interactioncontroller.h"

#include <compositor.h>
#include <input.h>
#include <window.h>
#include <workspace.h>

#include <QApplication>
#include <QMargins>
#include <QScopedValueRollback>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

HybridChrome::ChromeMetrics chromeMetrics()
{
    return {};
}

void collectPageWindowIds(const Core::LayoutNode &node, QStringList *windowIds)
{
    if (node.isLeaf()) {
        windowIds->append(node.windowId());
        return;
    }
    collectPageWindowIds(*node.firstChild(), windowIds);
    collectPageWindowIds(*node.secondChild(), windowIds);
}

HybridConstraints::LayoutMetrics sceneMetrics()
{
    const auto chrome = chromeMetrics();
    return {
        .contentInsets = QMargins(qRound(chrome.outerBorder),
                                  qRound(chrome.outerBorder + chrome.titleBarHeight),
                                  qRound(chrome.outerBorder),
                                  qRound(chrome.outerBorder)),
        .dividerThickness = qRound(chrome.dividerVisualThickness),
    };
}

HybridChromePlanOptions chromePlanOptions(const HybridChrome::ChromeStyle &style)
{
    HybridChromePlanOptions options;
    options.metrics = chromeMetrics();
    // The resolved theme and chrome preferences own the arrangement
    // (ADR-0129); an unauthored theme resolves to the Qinda macOS style.
    options.style = style;
    return options;
}

} // namespace

KWinHybridSession::KWinHybridSession(ManagedWindowRegistry &registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    m_sceneFactory = std::make_unique<KWinHybridSceneFactory>(registry, sceneMetrics());
    m_sceneFactory->setMinimizedContainerProbe(
        [this](const QString &containerId) {
            return m_minimizedContainers.contains(containerId);
        });
    m_runtime = std::make_unique<HybridInteractionRuntime>(
        registry.windowIds(), *m_sceneFactory,
        HybridRuntimeCallbacks{
            .preview = [this](const HybridInput::InteractionIntent &intent) {
                if (m_dockPreview) {
                    m_dockPreview->handleIntent(intent);
                }
            },
            .containerMove = [this](const HybridInput::InteractionIntent &intent) {
                return m_placement
                    ? m_placement->handleMove(intent)
                    : DirectInteractionResult::rejected(
                          QStringLiteral("container placement is not initialized"));
            },
            .dividerResize = [this](const HybridInput::InteractionIntent &intent) {
                return m_placement
                    ? m_placement->dividerRatio(intent)
                    : DividerGeometryResult::unavailable(
                          QStringLiteral("container placement is not initialized"));
            },
            .containerResize = [this](const HybridInput::InteractionIntent &intent) {
                return m_placement
                    ? m_placement->handleResize(intent)
                    : DirectInteractionResult::rejected(
                          QStringLiteral("container placement is not initialized"));
            },
        });
    initializeMemberChromeSupport();
    m_groupStacking = std::make_unique<KWinHybridGroupStacking>(
        registry, *m_chromeManager);
    m_groupContext = std::make_unique<KWinGroupContextManager>(
        registry,
        [this](const QString &containerId, const QString &sourceWindowId) {
            adoptMemberContext(containerId, sourceWindowId);
        },
        [this] {
            return m_shutdown
                || (m_sceneFactory && m_sceneFactory->applyingWindowStates());
        });
    m_memberPolicy = std::make_unique<KWinMemberPolicyManager>(
        registry, *m_chromeManager,
        [this](const QString &containerId, const QString &windowId, QString *error) {
            return detachNativeMember(containerId, windowId, error);
        },
        [this] {
            return m_shutdown
                || (m_sceneFactory && m_sceneFactory->applyingWindowStates());
        },
        [this](const QString &containerId, const QString &windowId,
               NativeQuickTileEdge edge, QString *error) {
            return handleNativeMemberQuickTile(containerId, windowId, edge, error);
        });
    m_transientManager = std::make_unique<KWinTransientManager>(registry);
    m_chromeSyncScheduler = std::make_unique<HybridChromeSyncScheduler>(
        [this](HybridChromeSyncReasons reasons) {
            if (reasons.testFlag(HybridChromeSyncReason::Windows)) {
                reconcileMinimizedContainers();
            } else {
                synchronizeChrome();
            }
        });
    auto *const workspace = KWin::workspace();
    connect(workspace, &KWin::Workspace::stackingOrderChanged,
            m_chromeSyncScheduler.get(),
            &HybridChromeSyncScheduler::stackingOrderChanged);
    connect(workspace, &KWin::Workspace::windowActivated,
            m_chromeSyncScheduler.get(),
            &HybridChromeSyncScheduler::activeWindowChanged);
    connect(&registry, &ManagedWindowRegistry::outputsChanged,
            m_chromeSyncScheduler.get(),
            &HybridChromeSyncScheduler::outputsChanged);
    m_targetResolver = std::make_unique<KWinInteractionTargetResolver>(
        registry, m_chromeManager.get(),
        [this](const QString &containerId,
               const QPointF &position,
               const QSet<QString> &excludedWindowIds) {
            return m_groupStacking
                && m_groupStacking->chromeExposedAt(
                    containerId, position, excludedWindowIds);
        },
        [this](const QString &containerId) -> std::optional<QRectF> {
            const auto layout = m_sceneFactory->committedLayout(containerId);
            if (!layout) {
                return std::nullopt;
            }
            return QRectF(layout->activePage.contentFrame);
        },
        [this](const QString &containerId, const QString &pageId) -> QStringList {
            const auto *container = m_runtime->topology().container(containerId);
            const auto *page = container ? container->page(pageId) : nullptr;
            if (!page) {
                return {};
            }
            QStringList members;
            collectPageWindowIds(page->root(), &members);
            return members;
        });
    m_dragTranslator = std::make_unique<HybridChromeDragTranslator>(*m_targetResolver);
    m_placement = std::make_unique<HybridContainerPlacementController>(
        [this]() -> const Hybrid::WindowTopology & { return m_runtime->topology(); },
        [this](const QString &id) { return m_sceneFactory->committedLayout(id); },
        [this](const Core::WindowContainer &container, const QRect &frame) {
            return m_sceneFactory->reflowContainer(container, frame);
        },
        [this](const QString &id) { return workArea(id); },
        [this] {
            synchronizeChrome();
            Q_EMIT shellVisibilityStateChanged();
        });
    initializeGroupedGeometryReconciliation();
    // KWin exposes the start of work-area rearrangement. Queue the Hybrid
    // reconciliation so MaximizeArea and ordinary-client constraints include
    // the newly committed layer-shell struts.
    connect(workspace, &KWin::Workspace::aboutToRearrange, this,
            &KWinHybridSession::reconcileWorkAreaGeometry,
            Qt::QueuedConnection);
    m_interactionController = std::make_unique<HybridInput::InteractionController>(
        *m_targetResolver);
    m_chromePointerRouter = std::make_unique<HybridChromePointerRouter>(
        [this](const QPointF &position) {
            const auto hit = m_chromeManager->pointerTargetAt(position);
            if (!hit || !m_groupStacking
                || !m_groupStacking->chromeExposedAt(
                    hit->containerId, position)) {
                return std::optional<ChromePointerHit>{};
            }
            return hit;
        },
        QApplication::startDragDistance());
    m_dockPreview = std::make_unique<KWinDockPreview>(
        [this](const HybridInput::DockTarget &target) {
            return dockTargetFrame(target);
        });
    m_inputFilter = std::make_unique<KWinInteractionFilter>(
        KWin::input(), *m_interactionController,
        [this](const HybridInput::InteractionIntent &intent) { dispatchIntent(intent); },
        m_chromePointerRouter.get(),
        [this](const ChromePointerDecision &decision) {
            dispatchChromePointerDecision(decision);
        },
        [this](KWin::Window *window) -> HybridInput::HitTarget {
            const auto id = window ? m_registry.windowId(window) : QString{};
            if (id.isEmpty() || m_registry.window(id) != window) {
                // An unmanaged move owner adopts the swallowed no-target
                // grab; the takeover still consumes so KWin never resumes it.
                return {};
            }
            return {HybridInput::HitKind::MemberTitle, m_registry.owner(id), id, {}};
        });
    initializeTaskIdentityAndShortcuts();
    initializeSavedWorkspaces();
    initializeGroupContextMenu();
    initializeChromeSceneLifecycle();
    m_closePrompt = std::make_unique<ContainerClosePrompt>(
        [this](const QString &containerId, ContainerCloseDecision decision) {
            handleCloseDecision(containerId, decision);
        });

    connect(&registry, &ManagedWindowRegistry::managedWindowAdded,
            this, &KWinHybridSession::addManagedWindow);
    connect(&registry, &ManagedWindowRegistry::managedWindowClosed,
            this, [this](const QString &id, const QString &) { forgetManagedWindow(id); });
    connect(&registry, &ManagedWindowRegistry::windowsChanged,
            this, &KWinHybridSession::handleWindowsChanged);
    connect(m_chromeManager.get(), &KWinChromeManager::chromeDragLifecycle,
            this, &KWinHybridSession::handleChromeDrag);
    connect(m_chromeManager.get(), &KWinChromeManager::windowActionRequested,
            this, &KWinHybridSession::handleWindowAction);
    connect(m_chromeManager.get(), &KWinChromeManager::tabActivationRequested,
            this, &KWinHybridSession::handleTabActivation);

    if (!m_runtime->ready()) {
        qWarning("QindaQt Hybrid runtime could not initialize: %s",
                 qPrintable(m_runtime->initializationError()));
    }
    synchronizeChrome();
}

void KWinHybridSession::initializeChromeSceneLifecycle()
{
    auto *const compositor = KWin::Compositor::self();
    m_chromeSceneLifecycle = std::make_unique<KWinChromeSceneLifecycle>(
        [this] {
            // Visibility observers normally rebuild accessibility from the
            // published plans. Suppress that re-entrant path while the scene
            // publication is intentionally empty, then clear its stale roots.
            QScopedValueRollback<bool> synchronizing(m_synchronizingChrome, true);
            invalidateChromePublication();
        },
        [this] { synchronizeChrome(); },
        compositor && compositor->isActive());
    if (compositor) {
        // AGENT-CONTRACT: These pre-teardown connections must remain direct.
        // KWin destroys WindowItems synchronously after these signals return.
        connect(compositor, &KWin::Compositor::aboutToToggleCompositing,
                m_chromeSceneLifecycle.get(),
                &KWinChromeSceneLifecycle::prepareForSceneTeardown,
                Qt::DirectConnection);
        connect(compositor, &KWin::Compositor::aboutToDestroy,
                m_chromeSceneLifecycle.get(),
                &KWinChromeSceneLifecycle::prepareForSceneTeardown,
                Qt::DirectConnection);
        // sceneCreated() is too early: setupCompositing() has not recreated
        // client WindowItems. compositingToggled(true) is the safe boundary.
        connect(compositor, &KWin::Compositor::compositingToggled,
                m_chromeSceneLifecycle.get(),
                &KWinChromeSceneLifecycle::compositingToggled,
                Qt::DirectConnection);
    }
}

KWinHybridSession::~KWinHybridSession()
{
    shutdown();
}

void KWinHybridSession::shutdown() noexcept
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    // Dialog callbacks borrow runtime/app collaborators; release them first.
    shutdownSavedWorkspaces();
    m_groupContextMenu.reset();
    // Disconnect compositor callbacks before explicit shutdown starts clearing
    // the same scene resources and restoring independent client state.
    m_chromeSceneLifecycle.reset();
    m_groupContext.reset();
    disconnect(&m_registry, nullptr, this, nullptr);
    // Destroying the QObject context cancels a queued stack/output resync and
    // disconnects its Workspace sources before releaseAll mutates live state.
    m_chromeSyncScheduler.reset();
    m_shortcuts.reset();
    if (m_closePrompt) {
        m_closePrompt->cancelAll();
    }
    if (m_inputFilter) {
        m_inputFilter->cancel();
    }
    m_inputFilter.reset();
    if (m_dockPreview) {
        m_dockPreview->clear();
    }
    if (m_placement) {
        m_placement->cancelAll();
    }
    if (m_memberPolicy) {
        // Focus mode owns hidden/fullscreen/decoration presentation that the
        // scene restore schema deliberately does not persist. Clear it while
        // the grouped layout baseline is still authoritative, but retain the
        // observer through bounded release recovery.
        m_memberPolicy->restorePresentationForShutdown();
    }
    restoreShadeForShutdown();
    restoreMemberChromeVisibilityForShutdown();

    if (m_runtime && m_sceneFactory) {
        const auto recovered = recoverKWinHybridShutdown(
            *m_runtime, *m_sceneFactory, m_registry);
        if (recovered.fallbackUsed) {
            qWarning("QindaQt Hybrid unload used emergency scene recovery");
        }
        if (!recovered.complete) {
            qWarning("QindaQt Hybrid unload recovery was incomplete: %s",
                     qPrintable(recovered.diagnostics.join(QStringLiteral("; "))));
        }
    }
    // Task, transient, and member observers remain alive through normal and
    // emergency scene release. Disconnect only after independent state and
    // ownership have settled.
    shutdownAccessibility();
    shutdownTaskIdentity();
    m_transientManager.reset();
    if (m_memberPolicy) {
        m_memberPolicy->shutdown();
    }
    m_memberPolicy.reset();
    if (m_chromeManager) {
        m_chromeManager->clear();
    }
    m_minimizedContainers.clear();
    m_appearance.clear();
    m_dockPreview.reset();
    m_closePrompt.reset();
    m_chromePointerRouter.reset();
    m_interactionController.reset();
    m_dragTranslator.reset();
    m_targetResolver.reset();
    m_groupStacking.reset();
    m_chromeManager.reset();
    m_groupedGeometryReconciler.reset();
    m_placement.reset();
    m_runtime.reset();
    m_sceneFactory.reset();
}

QStringList KWinHybridSession::containerStackingOrder() const
{
    QHash<QString, QString> activeMemberOwners;
    const auto containerIds = m_runtime->topology().containerIds();
    for (const auto &containerId : containerIds) {
        const auto layout = m_sceneFactory->committedLayout(containerId);
        if (!layout) {
            continue;
        }
        for (auto member = layout->activePage.members.cbegin();
             member != layout->activePage.members.cend(); ++member) {
            activeMemberOwners.insert(member.key(), containerId);
        }
    }
    QStringList windowsBottomToTop;
    for (auto *window : KWin::workspace()->stackingOrder()) {
        windowsBottomToTop.append(m_registry.windowId(window));
    }
    return topmostActiveMemberContainerOrder(
        windowsBottomToTop, activeMemberOwners, containerIds);
}

std::optional<QRectF> KWinHybridSession::dockTargetFrame(
    const HybridInput::DockTarget &target) const
{
    if (!target.containerId.isEmpty()) {
        const auto layout = m_sceneFactory->committedLayout(target.containerId);
        if (!layout) {
            return std::nullopt;
        }
        if (target.zone != HybridInput::DockZone::Tab) {
            if (target.memberId.isEmpty()) {
                // Container-edge drop: preview half of the whole content area.
                return QRectF(layout->activePage.contentFrame);
            }
            const auto member = layout->activePage.members.constFind(target.memberId);
            if (member != layout->activePage.members.cend()) {
                return QRectF(member->windowFrame);
            }
        }
        return QRectF(layout->outerFrame);
    }
    const auto frame = m_registry.targetFrame(target.memberId);
    return frame.isValid() ? std::optional<QRectF>(frame) : std::nullopt;
}

void KWinHybridSession::invalidateChromePublication()
{
    // AGENT-GUARD: Chrome plans and stack authority form one input-facing
    // publication. A current-snapshot planning/publication failure must revoke
    // every part together; retaining an older same-ID plan lets a later press
    // mutate the new topology through stale geometry. Full cancellation also
    // covers exact-modifier and keyboard state, not just shared-chrome grabs.
    if (m_inputFilter) {
        m_inputFilter->cancel();
    }
    if (m_chromeManager) {
        m_chromeManager->clear();
    }
    if (m_groupStacking) {
        m_groupStacking->clear();
    }
    if (m_accessibility) {
        m_accessibility->clear();
    }
}

void KWinHybridSession::synchronizeChrome()
{
    if (!ready() || m_synchronizingChrome) {
        return;
    }
    QScopedValueRollback<bool> synchronizing(m_synchronizingChrome, true);
    QString error;
    // Native title restoration belongs to topology lifetime, not scene image
    // lifetime. It must still run while compositing is temporarily unavailable.
    synchronizeMemberChromeVisibility();
    if (m_chromeSceneLifecycle && !m_chromeSceneLifecycle->sceneAvailable()) {
        return;
    }
    const auto publishedRevision = m_chromeManager->topologyRevision();
    if (publishedRevision && *publishedRevision != m_runtime->topology().revision()
        && m_chromePointerRouter && m_inputFilter) {
        // AGENT-GUARD: A topology replacement invalidates stable targets held
        // by hover or an in-flight ordinary chrome drag. Reset before overlay
        // removal; placement policy receives Cancel when its source survives.
        m_inputFilter->invalidateChromeTargets();
    }
    KWinChromeManager::ChromePlanMap plans;
    const auto optionsTemplate = chromePlanOptions(m_chromeStyle);
    const QString activeWindowId = m_registry.windowId(
        KWin::workspace()->activeWindow());
    const auto activeOwner = m_runtime->topology().ownerOf(activeWindowId);
    error.clear();
    for (const auto &containerId : m_runtime->topology().containerIds()) {
        const auto *container = m_runtime->topology().container(containerId);
        const auto layout = m_sceneFactory->committedLayout(containerId);
        if (!container || !layout) {
            qWarning("QindaQt Hybrid chrome lacks a committed layout for '%s'",
                     qPrintable(containerId));
            invalidateChromePublication();
            return;
        }
        auto options = optionsTemplate;
        options.devicePixelRatio = containerScale(containerId);
        options.maximized = m_placement && m_placement->isMaximized(containerId);
        options.shaded = m_placement && m_placement->isShaded(containerId);
        if (options.shaded) {
            options.shadedOuterFrame = QRectF(*m_placement->shadedFrame(containerId));
        }
        options.containerFocused = activeOwner && *activeOwner == containerId;
        options.focusedMemberId = options.containerFocused ? activeWindowId : QString{};
        options.memberTitlesVisible = memberTitlesVisible(containerId);
        const auto appearance = m_appearance.appearance(containerId);
        options.containerTitle = appearance.name;
        if (!appearance.colorHex.isEmpty()) {
            options.style.palette.accent = QColor(appearance.colorHex);
        }
        const auto plan = HybridChromePlanBuilder::build(
            *container, layout->activePage, options,
            [this](const QString &windowId) {
                const auto *window = m_registry.window(windowId);
                return window ? window->caption() : QString{};
            },
            &error);
        if (!plan) {
            qWarning("QindaQt Hybrid chrome plan failed: %s", qPrintable(error));
            invalidateChromePublication();
            return;
        }
        plans.insert(containerId, *plan);
    }
    if (!m_chromeManager->updateFromSnapshot(
            m_runtime->topology(), plans, containerStackingOrder(), &error)) {
        qWarning("QindaQt Hybrid chrome publication failed: %s", qPrintable(error));
        invalidateChromePublication();
        return;
    }
    if (m_groupContext) {
        m_groupContext->synchronize(m_runtime->topology());
    }
    QMap<QString, QString> shadedAnchors;
    if (m_shadeController) {
        // AGENT-CONTRACT: tells group stacking which member of a shaded
        // container is the content-preserving anchor (see
        // KWinShadeMemberPlatform::hideAnchorContent) so it does not hold
        // that container's genuinely-hidden siblings to the same-layer/
        // contiguous-stack checks meant for visible members.
        for (const auto &containerId : m_shadeController->shadedContainerIds()) {
            const auto anchorId = m_shadeController->anchorWindowId(containerId);
            if (!anchorId.isEmpty()) {
                shadedAnchors.insert(containerId, anchorId);
            }
        }
    }
    if (!m_groupStacking->synchronize(m_runtime->topology(), shadedAnchors, &error)) {
        m_lastGroupStackingFailure = error;
        qWarning("QindaQt Hybrid group stacking failed: %s", qPrintable(error));
        invalidateChromePublication();
        return;
    }
    m_lastGroupStackingFailure.clear();
    if (m_memberPolicy
        && !m_memberPolicy->synchronize(m_runtime->topology(), *m_sceneFactory, &error)) {
        qWarning("QindaQt member policy synchronization failed: %s", qPrintable(error));
    }
    if (m_transientManager
        && !m_transientManager->synchronize(m_runtime->topology(), &error)) {
        qWarning("QindaQt transient policy synchronization failed: %s", qPrintable(error));
    }
    synchronizeTaskIdentity();
    for (const auto &containerId : std::as_const(m_minimizedContainers)) {
        m_chromeManager->setOverlayVisible(containerId, false);
    }
    if (m_memberPolicy) {
        m_memberPolicy->enforceChromeVisibility();
    }
    synchronizeAccessibility();
}

} // namespace QindaQt::Compositor::KWinIntegration
