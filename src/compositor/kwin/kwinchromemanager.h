// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridgroupcontext.h"
#include "hybridchromepointerrouter.h"
#include "kwininteractiontargetresolver.h"

#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>

class QWidget;

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

// Platform-neutral lifetime seam for tests and future scene-graph adapters.
// Implementations are owned by KWinChromeManager and are called only from the
// manager's QObject thread. Methods must not throw or retain plan references.
class ChromeOverlay
{
public:
    virtual ~ChromeOverlay() = default;
    virtual void setRenderPlan(HybridChrome::ChromeRenderPlan plan) = 0;
    virtual void setPointerHoverTarget(HybridChrome::ChromeHitTarget target) = 0;
    // Reparents scene chrome to the member that owns the group's topmost stack
    // slot. Implementations must reject dead/unpaintable anchors.
    [[nodiscard]] virtual bool setStackingAnchor(const QString &windowId,
                                                 QString *error = nullptr) = 0;
    // Visibility is presentation state, not QWidget reach-through. Focus mode
    // and group minimize use this boundary for scene and test overlays alike.
    virtual void showOverlay() = 0;
    virtual void hideOverlay() noexcept = 0;
    [[nodiscard]] virtual bool isVisible() const noexcept = 0;
    // True only after the paint object is attached to a live member stack
    // slot. Production diagnostics combine this with isVisible() to prove the
    // ImageItem is renderable, rather than merely publishing a chrome plan.
    [[nodiscard]] virtual bool hasStackingAnchor() const noexcept = 0;
    virtual void closeOverlay() noexcept = 0;
    [[nodiscard]] virtual QWidget *widget() const noexcept { return nullptr; }
};

class ChromeOverlayFactory
{
public:
    virtual ~ChromeOverlayFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<ChromeOverlay> create(
        const QString &containerId) = 0;
};

struct ChromeWindowActionRequest final
{
    QString containerId;
    HybridChrome::WindowAction action = HybridChrome::WindowAction::Close;

    friend bool operator==(const ChromeWindowActionRequest &,
                           const ChromeWindowActionRequest &) = default;
};

class KWinChromeManager final : public QObject, public ChromeHitProvider
{
    Q_OBJECT

public:
    using ChromePlanMap = QMap<QString, HybridChrome::ChromeRenderPlan>;

    // The production factory creates paint-only ImageItems parented into
    // KWin's window scene. Pointer ownership stays in the compositor filter.
    explicit KWinChromeManager(ManagedWindowRegistry &registry,
                               QObject *parent = nullptr);
    // The injected factory is borrowed and must outlive this manager.
    explicit KWinChromeManager(ChromeOverlayFactory &factory, QObject *parent = nullptr);
    ~KWinChromeManager() override;

    KWinChromeManager(const KWinChromeManager &) = delete;
    KWinChromeManager &operator=(const KWinChromeManager &) = delete;

    // Reconciles an immutable topology snapshot with already-derived global
    // logical chrome plans. stackingOrder is back-to-front and must name every
    // container exactly once; an empty list uses stable container-ID order.
    [[nodiscard]] bool updateFromSnapshot(const Hybrid::WindowTopology &snapshot,
                                          const ChromePlanMap &plans,
                                          QStringList stackingOrder = {},
                                          QString *error = nullptr);
    // Quarantine is process-lifetime safety state, not scene-item visibility.
    // It survives clear() across compositor restarts and blocks all input and
    // show paths until context adoption succeeds or topology drops the group.
    void quarantineContainer(const QString &containerId);
    void markContainerContextCoherent(const QString &containerId);
    [[nodiscard]] bool containerQuarantined(
        const QString &containerId) const noexcept;
    void clear() noexcept;

    [[nodiscard]] HybridInput::HitTarget hitTestChrome(
        const QPointF &position) const override;
    // Ordinary input routing protects native member-decoration rectangles even
    // where a widened divider/edge affordance geometrically overlaps them.
    [[nodiscard]] std::optional<ChromePointerHit> pointerTargetAt(
        const QPointF &position) const;
    // Repaints the input-transparent overlay from compositor-owned hover state.
    // nullopt clears every overlay, including when the pointer leaves chrome.
    void setPointerHover(std::optional<ChromePointerHit> hit);
    [[nodiscard]] bool setStackingAnchor(const QString &containerId,
                                         const QString &windowId,
                                         QString *error = nullptr);
    void setOverlayVisible(const QString &containerId, bool visible) noexcept;
    [[nodiscard]] bool overlayVisible(const QString &containerId) const noexcept;
    // Revalidates identity against the currently published plan before
    // emitting the existing session policy signals.
    [[nodiscard]] bool dispatchPointerActivation(const ChromePointerHit &hit);
    [[nodiscard]] std::optional<ChromeWindowActionRequest> windowActionAt(
        const QPointF &position) const;
    // Emits a policy request; the integration owner decides whether the action
    // targets the active member or the entire topology container.
    [[nodiscard]] bool requestWindowActionAt(const QPointF &position);

    [[nodiscard]] qsizetype overlayCount() const noexcept;
    [[nodiscard]] qsizetype visibleOverlayCount() const noexcept;
    [[nodiscard]] qsizetype anchoredOverlayCount() const noexcept;
    [[nodiscard]] qsizetype visibleAnchoredOverlayCount() const noexcept;
    [[nodiscard]] qsizetype quarantinedContainerCount() const noexcept;
    [[nodiscard]] bool contains(const QString &containerId) const;
    [[nodiscard]] std::optional<quint64> topologyRevision() const noexcept;
    [[nodiscard]] std::optional<HybridChrome::ChromeRenderPlan> plan(
        const QString &containerId) const;
    // Exact page-to-member identity derived from the same validated topology
    // snapshot as plan(). Value return prevents accessibility/semantic clients
    // from retaining references across the next reconciliation.
    [[nodiscard]] QMap<QString, QString> tabRepresentatives(
        const QString &containerId) const;
    // Preview/test factories may expose a QWidget; production scene overlays
    // deliberately return null because they create no input-addressable window.
    [[nodiscard]] QWidget *overlayWidget(const QString &containerId) const noexcept;

Q_SIGNALS:
    // AGENT-CONTRACT: Emitted only after isVisible() actually changes.
    // Consumers must read the manager snapshot so a transition cannot publish
    // stale plans.
    void overlayVisibilityChanged(const QString &containerId, bool visible);
    void windowActionRequested(const QString &containerId,
                               QindaQt::HybridChrome::WindowAction action);
    void tabActivationRequested(const QString &containerId, const QString &pageId);
    // Value-only bridge into HybridInput/topology policy. The manager never
    // exposes QWidget pointers or decides what a drag mutation means.
    void chromeDragLifecycle(const QString &containerId,
                             const QindaQt::HybridChrome::ChromeDragEvent &event);

private:
    struct Entry final
    {
        std::unique_ptr<ChromeOverlay> overlay;
        HybridChrome::ChromeRenderPlan plan;
        QMap<QString, QString> tabRepresentatives;
    };

    [[nodiscard]] std::optional<ChromePointerHit> pointerHitAt(
        const QPointF &position) const;
    void applyPointerHover();

    std::unique_ptr<ChromeOverlayFactory> m_ownedFactory;
    ChromeOverlayFactory *m_factory = nullptr;
    std::map<QString, Entry> m_entries;
    QStringList m_stackingOrder;
    QByteArray m_topologyShape;
    std::optional<ChromePointerHit> m_pointerHover;
    HybridGroupContextQuarantine m_contextQuarantine;
    quint64 m_topologyRevision = 0;
    bool m_hasSnapshot = false;
};

inline qsizetype KWinChromeManager::overlayCount() const noexcept
{
    return static_cast<qsizetype>(m_entries.size());
}

inline qsizetype KWinChromeManager::visibleAnchoredOverlayCount() const noexcept
{
    return static_cast<qsizetype>(std::count_if(
        m_entries.cbegin(), m_entries.cend(), [](const auto &entry) {
            return entry.second.overlay->isVisible()
                && entry.second.overlay->hasStackingAnchor();
        }));
}

inline qsizetype KWinChromeManager::visibleOverlayCount() const noexcept
{
    return static_cast<qsizetype>(std::count_if(
        m_entries.cbegin(), m_entries.cend(), [](const auto &entry) {
            return entry.second.overlay->isVisible();
        }));
}

inline qsizetype KWinChromeManager::anchoredOverlayCount() const noexcept
{
    return static_cast<qsizetype>(std::count_if(
        m_entries.cbegin(), m_entries.cend(), [](const auto &entry) {
            return entry.second.overlay->hasStackingAnchor();
        }));
}

inline qsizetype KWinChromeManager::quarantinedContainerCount() const noexcept
{
    return m_contextQuarantine.size();
}

inline bool KWinChromeManager::contains(const QString &containerId) const
{
    return m_entries.contains(containerId);
}

inline std::optional<quint64> KWinChromeManager::topologyRevision() const noexcept
{
    return m_hasSnapshot ? std::optional<quint64>(m_topologyRevision) : std::nullopt;
}

inline std::optional<HybridChrome::ChromeRenderPlan> KWinChromeManager::plan(
    const QString &containerId) const
{
    const auto found = m_entries.find(containerId);
    return found == m_entries.end()
        ? std::nullopt : std::optional<HybridChrome::ChromeRenderPlan>(found->second.plan);
}

inline QMap<QString, QString> KWinChromeManager::tabRepresentatives(
    const QString &containerId) const
{
    const auto found = m_entries.find(containerId);
    return found == m_entries.end() ? QMap<QString, QString>{}
                                    : found->second.tabRepresentatives;
}

inline QWidget *KWinChromeManager::overlayWidget(const QString &containerId) const noexcept
{
    const auto found = m_entries.find(containerId);
    return found == m_entries.end() ? nullptr : found->second.overlay->widget();
}

inline bool KWinChromeManager::overlayVisible(const QString &containerId) const noexcept
{
    const auto found = m_entries.find(containerId);
    return found != m_entries.end() && found->second.overlay->isVisible();
}

inline bool KWinChromeManager::containerQuarantined(
    const QString &containerId) const noexcept
{
    return m_contextQuarantine.contains(containerId);
}

} // namespace QindaQt::Compositor::KWinIntegration

Q_DECLARE_METATYPE(QindaQt::Compositor::KWinIntegration::ChromeWindowActionRequest)
