// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindadecorationvisuals.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>

#include <QPalette>
#include <QVariantList>

#include <memory>
#include <optional>

class QEvent;
class QMouseEvent;

namespace KDecoration3 {
class DecorationButtonGroup;
}

namespace QindaQt::Decoration {

class QindaWindowContextMenu;

class QindaDecoration final : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    explicit QindaDecoration(QObject *parent = nullptr,
                             const QVariantList &args = {});
    ~QindaDecoration() override;

    [[nodiscard]] bool init() override;
    void paint(QPainter *painter, const QRectF &repaintArea) override;
    bool event(QEvent *event) override;

    [[nodiscard]] bool controlsHovered() const noexcept
    {
        return m_controlsHovered;
    }
    [[nodiscard]] bool memberFocusMaximized() const;
    // Worn Luna chrome is active only when the resolved theme authors a
    // titleBar color and selects the glyph button style; every other theme
    // takes the classic code paths unchanged.
    [[nodiscard]] bool glyphChrome() const;
    [[nodiscard]] bool wornLunaChrome() const;
    // Live window state gathered for the shared painter (ADR-0127).
    [[nodiscard]] DecorationChrome chromeState() const;
    [[nodiscard]] DecorationFrameVisual frameState() const;
    [[nodiscard]] static DecorationButtonKind buttonKind(
        KDecoration3::DecorationButtonType type);
    [[nodiscard]] QColor glyphChromeColor(KDecoration3::DecorationButtonType type) const;
    [[nodiscard]] QColor buttonColor(KDecoration3::DecorationButtonType type) const;
    [[nodiscard]] QColor buttonGlyphColor(KDecoration3::DecorationButtonType type) const;

public Q_SLOTS:
    void updateControlHover();

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void createButtons();
    void createContextMenu();
    void showContextMenu(const QPointF &position);
    void reconcileButtons();
    void updateGeometry();
    void updateVisualStyle();
    [[nodiscard]] QColor titleColor() const;
    [[nodiscard]] QColor captionColor() const;
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QColor authoredColor(const char *key) const;
    [[nodiscard]] QColor paletteColor(const char *key,
                                      QPalette::ColorRole fallbackRole,
                                      QPalette::ColorGroup group) const;
    [[nodiscard]] quint32 wearSeed() const;

    KDecoration3::DecorationButtonGroup *m_leftButtons = nullptr;
    KDecoration3::DecorationButtonGroup *m_rightButtons = nullptr;
    std::unique_ptr<QindaWindowContextMenu> m_contextMenu;
    std::optional<QPointF> m_contextPressPosition;
    bool m_controlsHovered = false;
    bool m_initialized = false;
};

} // namespace QindaQt::Decoration
