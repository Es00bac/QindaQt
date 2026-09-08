// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

namespace QindaQt::Controls {
// QML visual adapter only. Domain identity and accessible command naming stay
// with the containing button/list item. A valid color requests alpha-preserving
// symbolic tint; an invalid color preserves the icon's original artwork.
class IconItem final : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(Icon)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit IconItem(QQuickItem *parent = nullptr);
    QString name() const { return m_name; }
    QColor color() const { return m_color; }
    void setName(const QString &name);
    void setColor(const QColor &color);
    void paint(QPainter *painter) override;
Q_SIGNALS:
    void nameChanged();
    void colorChanged();
private:
    QString m_name;
    QColor m_color;
};
}
