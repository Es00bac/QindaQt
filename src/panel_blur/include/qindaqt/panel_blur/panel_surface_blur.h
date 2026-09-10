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

private:
    void pushRegion();

    QPointer<QQuickWindow> m_window;
    std::unique_ptr<BlurManagerExtension> m_manager;
    struct BlurObject;
    std::unique_ptr<BlurObject> m_blur;
    QRectF m_bounds;
    bool m_attached = false;
};

} // namespace QindaQt::PanelBlur
