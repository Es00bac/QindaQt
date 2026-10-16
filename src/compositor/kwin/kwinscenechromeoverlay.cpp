// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinscenechromeoverlay.h"

#include "kwinchromemanager.h"
#include "chromeoverlayraster.h"
#include "chromeplanlocalizer.h"
#include "managedwindowregistry.h"

#include "qindaqt/hybrid_chrome/chromesplitdeck.h"

#include <compositor.h>
#include <scene/imageitem.h>
#include <scene/itemrenderer.h>
#include <scene/windowitem.h>
#include <scene/workspacescene.h>
#include <window.h>

#include <QEasingCurve>
#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QTimer>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

class KWinSceneChromeOverlay final : public ChromeOverlay
{
public:
    KWinSceneChromeOverlay(QString containerId, ManagedWindowRegistry &registry)
        : m_containerId(std::move(containerId))
        , m_registry(registry)
    {
    }

    void setRenderPlan(HybridChrome::ChromeRenderPlan plan) override
    {
        m_globalGeometry = plan.outerFrame;
        auto localized = localizeChromeRenderPlan(std::move(plan), m_globalGeometry.topLeft());
        // ADR-0281: a split-deck carousel glides to its new active card over
        // the theme's motion duration (0 under reduced motion). Paint only:
        // the published plan, and so every hit test, is already final.
        if (localized.style.deckMotionMs > 0
            && HybridChrome::ChromeSplitDeck::canAnimate(m_raster.plan(), localized)) {
            startDeckMotion(std::move(localized));
            return;
        }
        stopDeckMotion();
        // ADR-0383: the raster compares plans. An identical plan repaints
        // nothing; a title/tab-only change repaints the title band alone.
        m_raster.setPlan(std::move(localized));
        updateItems();
    }

    void setPointerHoverTarget(HybridChrome::ChromeHitTarget target) override
    {
        auto state = m_raster.paintState();
        state.controlsHovered = target.kind == HybridChrome::HitKind::WindowButton;
        state.hoveredTarget = std::move(target);
        if (m_raster.setPaintState(state) != ChromeOverlayRaster::Repaint::None) {
            updateItems();
        }
    }

    void invalidateRendering() noexcept override
    {
        m_raster.invalidate();
    }

    bool setStackingAnchor(const QString &windowId, QString *error) override
    {
        auto *window = m_registry.window(windowId);
        auto *windowItem = window ? window->windowItem() : nullptr;
        auto *scene = KWin::Compositor::self() ? KWin::Compositor::self()->scene() : nullptr;
        if (!window || window->isDeleted() || !windowItem || !scene
            || !scene->renderer()) {
            return fail(error,
                        QStringLiteral("container '%1' cannot anchor chrome to '%2'")
                            .arg(m_containerId, windowId));
        }
        if (!m_item) {
            m_item = scene->renderer()->createImageItem(windowItem);
            if (!m_item) {
                return fail(error,
                            QStringLiteral("container '%1' could not create a scene image")
                                .arg(m_containerId));
            }
            // WindowItem's windowContainer uses z=0. A positive child paints
            // after the anchor's surface/decoration, while the parent window's
            // own stack slot keeps later unrelated windows above this image.
            m_item->setZ(1);
            // The title band is a child of the body item, so reparenting the
            // body to a new anchor carries the band with it.
            m_bandItem = scene->renderer()->createImageItem(m_item.get());
            if (!m_bandItem) {
                // Without its band the chrome would show no title row at all.
                m_item->setParentItem(nullptr);
                m_item.reset();
                return fail(error,
                            QStringLiteral("container '%1' could not create a title band image")
                                .arg(m_containerId));
            }
            m_pushedBodyKey = 0;
            m_pushedBandKey = 0;
        } else {
            m_item->setParentItem(windowItem);
        }
        m_anchor = window;
        m_anchorId = windowId;
        updateItems();
        return true;
    }

    void showOverlay() override
    {
        m_visible = true;
        updateItems();
    }

    void hideOverlay() noexcept override
    {
        m_visible = false;
        updateItems();
    }

    bool isVisible() const noexcept override
    {
        return m_visible && m_anchor && m_item && m_item->isVisible();
    }

    bool hasStackingAnchor() const noexcept override
    {
        return m_anchor && m_item;
    }

    void closeOverlay() noexcept override
    {
        stopDeckMotion();
        m_visible = false;
        if (m_bandItem) {
            m_bandItem->setVisible(false);
            m_bandItem->setParentItem(nullptr);
            m_bandItem.reset();
        }
        if (m_item) {
            m_item->setVisible(false);
            m_item->setParentItem(nullptr);
            m_item.reset();
        }
        m_pushedBodyKey = 0;
        m_pushedBandKey = 0;
        m_anchor.clear();
        m_anchorId.clear();
    }

private:
    void startDeckMotion(HybridChrome::ChromeRenderPlan target)
    {
        // Starting from what is on screen keeps a fast wheel smooth: a new
        // step mid-glide continues from the current card positions.
        m_motionFrom = m_raster.plan();
        m_motionTarget = std::move(target);
        m_raster.setPlan(
            HybridChrome::ChromeSplitDeck::interpolate(m_motionFrom, m_motionTarget, 0.0));
        updateItems();
        if (!m_motionTimer) {
            m_motionTimer = std::make_unique<QTimer>();
            m_motionTimer->setInterval(16);
            QObject::connect(m_motionTimer.get(), &QTimer::timeout, m_motionTimer.get(),
                             [this] { advanceDeckMotion(); });
        }
        m_motionClock.start();
        m_motionTimer->start();
    }

    void advanceDeckMotion()
    {
        const int duration = std::max(1, m_motionTarget.style.deckMotionMs);
        const qreal progress = std::min(1.0, qreal(m_motionClock.elapsed()) / duration);
        if (progress >= 1.0) {
            stopDeckMotion();
        } else {
            // Glide frames move only the deck cards, so each repaints the
            // title band rather than the whole frame.
            m_raster.setPlan(HybridChrome::ChromeSplitDeck::interpolate(
                m_motionFrom, m_motionTarget,
                QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress)));
        }
        updateItems();
    }

    // Lands any glide in flight on its target.
    void stopDeckMotion()
    {
        if (m_motionTimer && m_motionTimer->isActive()) {
            m_motionTimer->stop();
            m_raster.setPlan(m_motionTarget);
        }
    }

    // AGENT-GUARD: ImageItem::setImage() always discards quads and repaints
    // the item, and uploads a texture when the cache key changed. Push an
    // image only when the raster replaced it, so an unchanged plan costs no
    // repaint and a title change re-uploads only the band texture.
    void updateItems() noexcept
    {
        if (!m_item) {
            return;
        }
        const QImage &body = m_raster.body();
        const QImage &band = m_raster.band();
        const bool renderable = m_visible && m_anchor && !body.isNull();
        m_item->setVisible(renderable);
        if (m_bandItem) {
            m_bandItem->setVisible(renderable && !band.isNull());
        }
        if (!renderable) {
            return;
        }
        if (body.cacheKey() != m_pushedBodyKey) {
            m_item->setImage(body);
            m_pushedBodyKey = body.cacheKey();
        }
        m_item->setSize(m_raster.bodyLogicalSize());
        m_item->setPosition(m_globalGeometry.topLeft() - m_anchor->pos());
        if (m_bandItem && !band.isNull()) {
            if (band.cacheKey() != m_pushedBandKey) {
                m_bandItem->setImage(band);
                m_pushedBandKey = band.cacheKey();
            }
            m_bandItem->setPosition(QPointF(0.0, 0.0));
            m_bandItem->setSize(m_raster.bandLogicalSize());
        }
    }

    QString m_containerId;
    ManagedWindowRegistry &m_registry;
    // What is painted: the published plan, or a frame of a deck glide.
    ChromeOverlayRaster m_raster;
    HybridChrome::ChromeRenderPlan m_motionFrom;
    HybridChrome::ChromeRenderPlan m_motionTarget;
    std::unique_ptr<QTimer> m_motionTimer;
    QElapsedTimer m_motionClock;
    QRectF m_globalGeometry;
    std::unique_ptr<KWin::ImageItem> m_item;
    std::unique_ptr<KWin::ImageItem> m_bandItem;
    qint64 m_pushedBodyKey = 0;
    qint64 m_pushedBandKey = 0;
    QPointer<KWin::Window> m_anchor;
    QString m_anchorId;
    bool m_visible = false;
};

class KWinSceneChromeOverlayFactory final : public ChromeOverlayFactory
{
public:
    explicit KWinSceneChromeOverlayFactory(ManagedWindowRegistry &registry)
        : m_registry(registry)
    {
    }

    std::unique_ptr<ChromeOverlay> create(const QString &containerId) override
    {
        return std::make_unique<KWinSceneChromeOverlay>(containerId, m_registry);
    }

private:
    ManagedWindowRegistry &m_registry;
};

} // namespace

std::unique_ptr<ChromeOverlayFactory>
createKWinSceneChromeOverlayFactory(ManagedWindowRegistry &registry)
{
    return std::make_unique<KWinSceneChromeOverlayFactory>(registry);
}

KWinChromeManager::KWinChromeManager(ManagedWindowRegistry &registry,
                                     QObject *parent)
    : QObject(parent)
    , m_ownedFactory(createKWinSceneChromeOverlayFactory(registry))
    , m_factory(m_ownedFactory.get())
{
    qRegisterMetaType<HybridChrome::WindowAction>();
    qRegisterMetaType<HybridChrome::ChromeDragEvent>();
    qRegisterMetaType<ChromeWindowActionRequest>();
}

} // namespace QindaQt::Compositor::KWinIntegration
