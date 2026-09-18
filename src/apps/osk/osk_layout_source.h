// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Osk {

// Follows the compositor's keyboard layout over `org.kde.KeyboardLayouts` so
// the on-screen keyboard shows the letters a hardware keyboard would type,
// and switches layouts through the same interface.
class OskLayoutSource final : public QObject {
    Q_OBJECT

public:
    explicit OskLayoutSource(QDBusConnection bus, QObject *parent = nullptr);

    [[nodiscard]] QString currentLayout() const;
    [[nodiscard]] QStringList layouts() const { return m_layouts; }
    [[nodiscard]] bool available() const { return m_available; }

    void refresh();
    void switchToNext();

Q_SIGNALS:
    void currentLayoutChanged();

private Q_SLOTS:
    void handleLayoutChanged(uint index);
    void handleLayoutListChanged();

private:
    void fetchLayouts();
    void fetchIndex();

    QDBusConnection m_bus;
    QStringList m_layouts;
    uint m_index = 0;
    bool m_available = false;
};

} // namespace QindaQt::Apps::Osk
