// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVariantMap>

namespace QindaQt::Compositor::KWinIntegration {
// Borrowed by the bundled switcher's optional bridge on the compositor thread.
// Owns only values. Replacing the map atomically removes dissolved containers;
// every member UUID is included so native primary changes need no caption rewrite.
class ContainerIdentityProjection final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap byWindow READ byWindow NOTIFY changed)
public:
    explicit ContainerIdentityProjection(QObject *parent = nullptr) : QObject(parent) {}
    [[nodiscard]] QVariantMap byWindow() const { return m_byWindow; }
    void publish(const QVariantMap &values) {
        if (values != m_byWindow) {
            m_byWindow = values;
            Q_EMIT changed();
        }
    }
Q_SIGNALS:
    void changed();
private:
    QVariantMap m_byWindow;
};
} // namespace QindaQt::Compositor::KWinIntegration
