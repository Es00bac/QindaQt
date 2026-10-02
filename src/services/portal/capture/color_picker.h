// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QImage>
#include <QWidget>
namespace QindaQt::Services::Portal {
// Frozen owned image; mouse and keyboard select real pixels, never a fabricated
// default success. The helper clears the widget on native authority loss.
class ColorPicker final : public QWidget {
    Q_OBJECT
public:
    explicit ColorPicker(QImage image, QWidget *parent = nullptr);
Q_SIGNALS:
    void chosen(const QColor &);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    QRect imageRect() const;
    QImage m_image; QPoint m_pixel;
};
}
