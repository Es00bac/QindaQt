// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindockpreview.h"

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QThread>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

class DockPreviewWidget final : public QWidget
{
public:
    DockPreviewWidget()
    {
        setObjectName(QStringLiteral("qindaqt-hybrid-dock-preview"));
        setProperty("qindaqtHybridDockPreview", true);
        // KWin InternalWindow::hitTest recognizes outputOnly. Qt's
        // WindowTransparentForInput flag alone suppresses popup grabs but does
        // not remove an internal QWidget from findToplevel(), which would make
        // this preview occlude its own docking target on release.
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                       | Qt::WindowDoesNotAcceptFocus
                       | Qt::WindowTransparentForInput
                       | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_NativeWindow);
        // `InternalWindow` reads the backing QWindow, not QWidget dynamic
        // properties. Creating the handle here makes outputOnly effective
        // before the first preview enters KWin's stack.
        if (auto *handle = windowHandle()) {
            handle->setProperty("outputOnly", true);
        }
        setFocusPolicy(Qt::NoFocus);
    }

    void setAccent(QColor accent)
    {
        m_accent = std::move(accent);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        auto fill = m_accent;
        fill.setAlpha(72);
        auto border = m_accent;
        border.setAlpha(230);
        painter.setPen(QPen(border, 2.0));
        painter.setBrush(fill);
        painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 12.0, 12.0);
    }

private:
    QColor m_accent = QColor(QStringLiteral("#4daf98"));
};

class WidgetDockPreviewOverlay final : public DockPreviewOverlay
{
public:
    WidgetDockPreviewOverlay()
        : m_widget(std::make_unique<DockPreviewWidget>())
    {
    }

    void showPreview(const QRectF &geometry, const QColor &accent) override
    {
        m_widget->setAccent(accent);
        m_widget->setGeometry(geometry.toAlignedRect());
        m_widget->show();
        m_widget->raise();
    }

    void hidePreview() noexcept override { m_widget->hide(); }

private:
    std::unique_ptr<DockPreviewWidget> m_widget;
};

class WidgetDockPreviewOverlayFactory final : public DockPreviewOverlayFactory
{
public:
    std::unique_ptr<DockPreviewOverlay> create() override
    {
        if (!QApplication::instance()
            || QApplication::instance()->thread() != QThread::currentThread()) {
            return {};
        }
        return std::make_unique<WidgetDockPreviewOverlay>();
    }
};

bool finiteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
        && std::isfinite(rect.width()) && std::isfinite(rect.height())
        && rect.isValid();
}

} // namespace

KWinDockPreview::KWinDockPreview(TargetFrameResolver resolver, QColor accent)
    : m_resolver(std::move(resolver))
    , m_accent(std::move(accent))
    , m_ownedFactory(std::make_unique<WidgetDockPreviewOverlayFactory>())
{
    initialize(*m_ownedFactory);
}

KWinDockPreview::KWinDockPreview(TargetFrameResolver resolver,
                                 DockPreviewOverlayFactory &factory,
                                 QColor accent)
    : m_resolver(std::move(resolver))
    , m_accent(std::move(accent))
{
    initialize(factory);
}

KWinDockPreview::~KWinDockPreview()
{
    clear();
}

void KWinDockPreview::initialize(DockPreviewOverlayFactory &factory)
{
    if (m_accent.isValid()) {
        m_overlay = factory.create();
    }
}

void KWinDockPreview::handleIntent(const HybridInput::InteractionIntent &intent)
{
    if (intent.kind != HybridInput::InteractionKind::MemberDock
        || intent.phase == HybridInput::IntentPhase::Cancel
        || intent.phase == HybridInput::IntentPhase::Commit) {
        clear();
        return;
    }
    if (!m_overlay || !m_resolver || !intent.target.isValid()) {
        clear();
        return;
    }
    const auto targetFrame = m_resolver(intent.target);
    const auto preview = targetFrame
        ? previewGeometry(*targetFrame, intent.target.zone)
        : std::nullopt;
    if (!preview) {
        clear();
        return;
    }
    m_geometry = *preview;
    m_overlay->showPreview(m_geometry, m_accent);
    m_visible = true;
}

void KWinDockPreview::clear() noexcept
{
    if (m_overlay) {
        m_overlay->hidePreview();
    }
    m_geometry = {};
    m_visible = false;
}

std::optional<QRectF> KWinDockPreview::previewGeometry(
    const QRectF &targetFrame,
    HybridInput::DockZone zone)
{
    if (!finiteRect(targetFrame)) {
        return std::nullopt;
    }
    QRectF result = targetFrame;
    switch (zone) {
    case HybridInput::DockZone::Left:
        result.setWidth(targetFrame.width() / 2.0);
        break;
    case HybridInput::DockZone::Right:
        result.setLeft(targetFrame.center().x());
        break;
    case HybridInput::DockZone::Top:
        result.setHeight(targetFrame.height() / 2.0);
        break;
    case HybridInput::DockZone::Bottom:
        result.setTop(targetFrame.center().y());
        break;
    case HybridInput::DockZone::Tab: {
        const qreal inset = std::min<qreal>(12.0,
            std::min(targetFrame.width(), targetFrame.height()) / 8.0);
        result.adjust(inset, inset, -inset, -inset);
        break;
    }
    case HybridInput::DockZone::None:
        return std::nullopt;
    }
    return finiteRect(result) ? std::optional<QRectF>(result) : std::nullopt;
}

} // namespace QindaQt::Compositor::KWinIntegration
