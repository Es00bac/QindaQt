// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QRegion>

#include <memory>

class QQuickWindow;

namespace QindaQt::PanelBlur {

class BlurManagerExtension;

// AGENT-CONTRACT: one blur-behind request per panel window, driven from the
// QML-painted translucent material bounds. The compositor owns the effect;
// this class only translates the published translucency truth into the
// org_kde_kwin_blur protocol. Calling on a non-Wayland platform (offscreen
// tests, the preview surface) is a guarded no-op, so presentation code never
// needs platform branches. The region must track the material bounds — a
// stale region would blur desktop pixels that no panel pixel covers.
class PanelSurfaceBlur final : public QObject {
    Q_OBJECT

public:
    explicit PanelSurfaceBlur(QObject *parent = nullptr);
    ~PanelSurfaceBlur() override;

    // Binds the helper to one window and pushes the initial region.
    void attach(QQuickWindow *window);

    // Painted material bounds in window coordinates; an empty rect clears.
    void setRegion(const QRectF &bounds);

    // Removes the blur request without unbinding the window.
    void clear();

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] QRectF bounds() const noexcept { return m_bounds; }

    // Pure geometry rule shared with the tests: the blur region is the
    // painted material bounds intersected with the window rect, in device
    //-independent pixels (Qt converts at flush time like the input mask).
    [[nodiscard]] static QRegion regionForBounds(const QRectF &bounds,
                                                 const QSize &windowSize);

    // Live org_kde_kwin_blur_manager registry bindings in this process. The
    // invariant is that this never exceeds one however many panel windows
    // exist: the manager is a global, and the shell republishes every panel
    // window on each output-generation change, so a per-window binding grew
    // without bound. Exposed because a registry binding has no other
    // observable footprint. Always 0 in a build without the protocol.
    [[nodiscard]] static int liveManagerBindings() noexcept;

private:
    void pushRegion();

    QPointer<QQuickWindow> m_window;
    // Shared process-wide: one org_kde_kwin_blur_manager binding serves every
    // panel window. Kept as a shared_ptr so the last panel destroys it, before
    // QGuiApplication goes away.
    std::shared_ptr<BlurManagerExtension> m_manager;
    struct BlurObject;
    std::unique_ptr<BlurObject> m_blur;
    QRectF m_bounds;
    bool m_attached = false;
};

} // namespace QindaQt::PanelBlur
