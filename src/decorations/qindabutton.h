// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KDecoration3/DecorationButton>

#include <QVariantList>

namespace QindaQt::Decoration {

class QindaDecoration;

class QindaButton final : public KDecoration3::DecorationButton
{
    Q_OBJECT

public:
    explicit QindaButton(QObject *parent, const QVariantList &args);

    [[nodiscard]] static QindaButton *create(
        KDecoration3::DecorationButtonType type,
        KDecoration3::Decoration *decoration,
        QObject *parent);

    void paint(QPainter *painter, const QRectF &repaintArea) override;

private:
    QindaButton(KDecoration3::DecorationButtonType type,
                QindaDecoration *decoration,
                QObject *parent);

    [[nodiscard]] QColor fillColor() const;
    void paintGlyph(QPainter &painter, const QRectF &circle) const;
};

} // namespace QindaQt::Decoration
