// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinscenechromeoverlay.h"

#include "kwinchromemanager.h"
#include "managedwindowregistry.h"

#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <compositor.h>
#include <scene/imageitem.h>
#include <scene/itemrenderer.h>
#include <scene/windowitem.h>
#include <scene/workspacescene.h>
#include <window.h>

#include <QImage>
#include <QPainter>
#include <QPointer>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void translateRect(QRectF *rect, const QPointF &offset)
{
    rect->translate(offset);
}

HybridChrome::ChromeRenderPlan localizedPlan(
    HybridChrome::ChromeRenderPlan plan, const QPointF &origin)
{
    const QPointF offset = -origin;
    translateRect(&plan.outerFrame, offset);
    translateRect(&plan.outerTitleBar, offset);
    translateRect(&plan.outerTitleDragRect, offset);
    translateRect(&plan.tabStrip, offset);
    translateRect(&plan.contentRect, offset);
    for (auto &button : plan.buttons) {
        translateRect(&button.rect, offset);
    }
    for (auto &tab : plan.tabs) {
        translateRect(&tab.rect, offset);
    }
    for (auto &member : plan.members) {
        translateRect(&member.windowRect, offset);
        translateRect(&member.titleDragRect, offset);
    }
    for (auto &divider : plan.dividers) {
        translateRect(&divider.visualRect, offset);
        translateRect(&divider.hitRect, offset);
    }
    return plan;
}

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
        m_devicePixelRatio = plan.devicePixelRatio;
        m_plan = localizedPlan(std::move(plan), m_globalGeometry.topLeft());
        render();
        updateItem();
    }

    void setPointerHoverTarget(HybridChrome::ChromeHitTarget target) override
    {
        const bool controlsHovered = target.kind == HybridChrome::HitKind::WindowButton;
        if (m_state.hoveredTarget == target
            && m_state.controlsHovered == controlsHovered) {
            return;
        }
        m_state.hoveredTarget = std::move(target);
        m_state.controlsHovered = controlsHovered;
        render();
        updateItem();
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
        } else {
            m_item->setParentItem(windowItem);
        }
        m_anchor = window;
        m_anchorId = windowId;
        updateItem();
        return true;
    }

    void showOverlay() override
    {
        m_visible = true;
        updateItem();
    }

    void hideOverlay() noexcept override
    {
        m_visible = false;
        updateItem();
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
        m_visible = false;
        if (m_item) {
            m_item->setVisible(false);
            m_item->setParentItem(nullptr);
            m_item.reset();
        }
        m_anchor.clear();
        m_anchorId.clear();
    }

private:
    void render()
    {
        if (!m_plan.outerFrame.isValid() || !std::isfinite(m_devicePixelRatio)
            || m_devicePixelRatio <= 0.0) {
            m_image = {};
            return;
        }
        const QSize physicalSize(
            qCeil(m_plan.outerFrame.width() * m_devicePixelRatio),
            qCeil(m_plan.outerFrame.height() * m_devicePixelRatio));
        if (physicalSize.isEmpty()) {
            m_image = {};
            return;
        }
        m_image = QImage(physicalSize, QImage::Format_ARGB32_Premultiplied);
        m_image.setDevicePixelRatio(m_devicePixelRatio);
        m_image.fill(Qt::transparent);
        QPainter painter(&m_image);
        HybridChrome::ChromeRenderer::paint(painter, m_plan, m_state);
    }

    void updateItem() noexcept
    {
        if (!m_item) {
            return;
        }
        const bool renderable = m_visible && m_anchor && !m_image.isNull();
        m_item->setVisible(renderable);
        if (!renderable) {
            return;
        }
        m_item->setImage(m_image);
        m_item->setSize(m_globalGeometry.size());
        m_item->setPosition(m_globalGeometry.topLeft() - m_anchor->pos());
    }

    QString m_containerId;
    ManagedWindowRegistry &m_registry;
    HybridChrome::ChromeRenderPlan m_plan;
    HybridChrome::ChromePaintState m_state;
    QRectF m_globalGeometry;
    QImage m_image;
    std::unique_ptr<KWin::ImageItem> m_item;
    QPointer<KWin::Window> m_anchor;
    QString m_anchorId;
    qreal m_devicePixelRatio = 1.0;
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
