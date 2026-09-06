// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KDecoration3/Decoration>

#include <QPalette>
#include <QVariantList>

namespace KDecoration3 {
class DecorationButtonGroup;
}

namespace QindaQt::Decoration {

class QindaDecoration final : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    explicit QindaDecoration(QObject *parent = nullptr,
                             const QVariantList &args = {});
    ~QindaDecoration() override;

    [[nodiscard]] bool init() override;
    void paint(QPainter *painter, const QRectF &repaintArea) override;

    [[nodiscard]] bool controlsHovered() const noexcept
    {
        return m_controlsHovered;
    }
    [[nodiscard]] bool memberFocusMaximized() const;
    [[nodiscard]] QColor buttonColor(KDecoration3::DecorationButtonType type) const;
    [[nodiscard]] QColor buttonGlyphColor(KDecoration3::DecorationButtonType type) const;

public Q_SLOTS:
    void updateControlHover();

private:
    void createButtons();
    void updateGeometry();
    [[nodiscard]] QColor titleColor() const;
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QColor paletteColor(const char *key,
                                      QPalette::ColorRole fallbackRole,
                                      QPalette::ColorGroup group) const;

    KDecoration3::DecorationButtonGroup *m_leftButtons = nullptr;
    bool m_controlsHovered = false;
};

} // namespace QindaQt::Decoration
