// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QColor>
#include <QRectF>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

class DockPreviewOverlay
{
public:
    virtual ~DockPreviewOverlay() = default;
    virtual void showPreview(const QRectF &geometry, const QColor &accent) = 0;
    virtual void hidePreview() noexcept = 0;
};

class DockPreviewOverlayFactory
{
public:
    virtual ~DockPreviewOverlayFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<DockPreviewOverlay> create() = 0;
};

class KWinDockPreview final
{
public:
    using TargetFrameResolver =
        std::function<std::optional<QRectF>(const HybridInput::DockTarget &)>;

    explicit KWinDockPreview(TargetFrameResolver resolver,
                             QColor accent = QColor(QStringLiteral("#4daf98")));
    KWinDockPreview(TargetFrameResolver resolver,
                    DockPreviewOverlayFactory &factory,
                    QColor accent = QColor(QStringLiteral("#4daf98")));
    ~KWinDockPreview();

    KWinDockPreview(const KWinDockPreview &) = delete;
    KWinDockPreview &operator=(const KWinDockPreview &) = delete;

    void handleIntent(const HybridInput::InteractionIntent &intent);
    void clear() noexcept;

    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    [[nodiscard]] QRectF geometry() const noexcept { return m_geometry; }

    [[nodiscard]] static std::optional<QRectF> previewGeometry(
        const QRectF &targetFrame,
        HybridInput::DockZone zone);

private:
    void initialize(DockPreviewOverlayFactory &factory);

    TargetFrameResolver m_resolver;
    QColor m_accent;
    std::unique_ptr<DockPreviewOverlayFactory> m_ownedFactory;
    std::unique_ptr<DockPreviewOverlay> m_overlay;
    QRectF m_geometry;
    bool m_visible = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
