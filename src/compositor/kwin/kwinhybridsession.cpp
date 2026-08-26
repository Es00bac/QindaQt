// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridchromedragtranslator.h"
#include "hybridchromeaccessibilityregistry.h"
#include "hybridchromepointerrouter.h"
#include "hybridchromesyncscheduler.h"
#include "hybridchromeplanbuilder.h"
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include "hybridshortcutmanager.h"
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
#include "hybridstackingorder.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_input/interactioncontroller.h"

#include <core/output.h>
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

HybridConstraints::LayoutMetrics sceneMetrics()
{
    const auto chrome = chromeMetrics();
    return {
        .contentInsets = QMargins(qRound(chrome.outerBorder),
                                  qRound(chrome.outerBorder
                                         + chrome.titleBarHeight
                                         + chrome.tabStripHeight),
                                  qRound(chrome.outerBorder),
                                  qRound(chrome.outerBorder)),
        .dividerThickness = qRound(chrome.dividerVisualThickness),
    };
}

HybridChromePlanOptions chromePlanOptions()
{
    HybridChromePlanOptions options;
    options.metrics = chromeMetrics();
    options.style = HybridChrome::ChromeStyle::qindaMacOS({});
    return options;
}

QString firstWindowId(const Core::LayoutNode &root)
{
    const auto *node = &root;
    while (node->isSplit()) {
        node = node->firstChild();
    }
    return node->windowId();
}

QString activeRepresentative(const Core::WindowContainer &container)
{
    const auto *page = container.page(container.activePageId());
    return page ? firstWindowId(page->root()) : QString{};
}

} // namespace

KWinHybridSession::KWinHybridSession(ManagedWindowRegistry &registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    m_sceneFactory = std::make_unique<KWinHybridSceneFactory>(registry, sceneMetrics());
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
    m_chromeManager = std::make_unique<KWinChromeManager>(registry);
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
               const QString &excludedWindowId) {
            return m_groupStacking
                && m_groupStacking->chromeExposedAt(
                    containerId, position, excludedWindowId);
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
        });
    initializeTaskIdentityAndShortcuts();
    initializeGroupContextMenu();
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
    m_dockPreview.reset();
    m_closePrompt.reset();
    m_chromePointerRouter.reset();
    m_interactionController.reset();
    m_dragTranslator.reset();
    m_targetResolver.reset();
    m_groupStacking.reset();
    m_chromeManager.reset();
    m_placement.reset();
    m_runtime.reset();
    m_sceneFactory.reset();
}

qreal KWinHybridSession::containerScale(const QString &containerId) const
{
    const auto *container = m_runtime->topology().container(containerId);
    auto *window = container
        ? m_registry.window(activeRepresentative(*container)) : nullptr;
    return window && window->output() ? window->output()->scale() : 1.0;
}

QRect KWinHybridSession::workArea(const QString &containerId) const
{
    const auto *container = m_runtime->topology().container(containerId);
    auto *window = container
        ? m_registry.window(activeRepresentative(*container)) : nullptr;
    if (!window) {
        return {};
    }
    const auto area = KWin::workspace()->clientArea(KWin::MaximizeArea, window)
                          .toAlignedRect();
    return static_cast<QRect>(area);
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
        if (target.zone != HybridInput::DockZone::Tab
            && !target.memberId.isEmpty()) {
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
    if (!ready()
        || (m_chromeSceneLifecycle && !m_chromeSceneLifecycle->sceneAvailable())
        || m_synchronizingChrome) {
        return;
    }
    QScopedValueRollback<bool> synchronizing(m_synchronizingChrome, true);
    const auto publishedRevision = m_chromeManager->topologyRevision();
    if (publishedRevision && *publishedRevision != m_runtime->topology().revision()
        && m_chromePointerRouter && m_inputFilter) {
        // AGENT-GUARD: A topology replacement invalidates stable targets held
        // by hover or an in-flight ordinary chrome drag. Reset before overlay
        // removal; placement policy receives Cancel when its source survives.
        m_inputFilter->invalidateChromeTargets();
    }
    KWinChromeManager::ChromePlanMap plans;
    const auto optionsTemplate = chromePlanOptions();
    QString error;
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
    if (!m_groupStacking->synchronize(m_runtime->topology(), &error)) {
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
