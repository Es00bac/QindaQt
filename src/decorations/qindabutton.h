// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/decoration_painter/decoration_painter.h"

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
    // A button for one painter kind (ADR-0264). More and RollUp are both
    // KDecoration Custom buttons, so the kind is kept, never re-derived.
    [[nodiscard]] static QindaButton *createForKind(
        DecorationButtonKind kind,
        KDecoration3::Decoration *decoration,
        QObject *parent);

    void paint(QPainter *painter, const QRectF &repaintArea) override;

private:
    QindaButton(KDecoration3::DecorationButtonType type,
                QindaDecoration *decoration,
                QObject *parent);

    [[nodiscard]] QColor fillColor() const;

    DecorationButtonKind m_kind = DecorationButtonKind::Close;
};

} // namespace QindaQt::Decoration
