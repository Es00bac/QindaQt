// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include <optional>

class QDBusConnection;

namespace QindaQt::Test {

struct DesktopNotificationBinding final {
    QString componentUniqueName;
    QString uniqueName;
    QString componentObjectPath;
    bool componentResolved = false;
    bool componentActiveReplyValid = false;
    bool componentActive = false;
    QList<int> defaultKeys;
    QList<int> activeKeys;
};

enum class DesktopNotificationActivationEdge {
    Pressed,
    Released,
};

struct DesktopNotificationActivationEvent final {
    DesktopNotificationActivationEdge edge;
    QString componentUniqueName;
    QString actionUniqueName;
};

[[nodiscard]] QString desktopNotificationComponentId();
[[nodiscard]] QString desktopNotificationActionId();
[[nodiscard]] int desktopNotificationMetaN();
[[nodiscard]] bool desktopNotificationBindingReady(
    const DesktopNotificationBinding &binding);
[[nodiscard]] bool desktopNotificationActivationComplete(
    const QList<DesktopNotificationActivationEvent> &events);
[[nodiscard]] QString desktopNotificationActivationDiagnostic(
    const QList<DesktopNotificationActivationEvent> &events);
[[nodiscard]] std::optional<DesktopNotificationBinding>
queryDesktopNotificationBinding(QString *error);

class DesktopNotificationActivationObserver final : public QObject {
    Q_OBJECT

public:
    explicit DesktopNotificationActivationObserver(QObject *parent = nullptr);

    [[nodiscard]] bool start(QDBusConnection connection,
                             const DesktopNotificationBinding &binding,
                             QString *error);
    [[nodiscard]] bool complete() const;
    [[nodiscard]] QString diagnostic() const;

private Q_SLOTS:
    void shortcutPressed(const QString &componentUniqueName,
                         const QString &actionUniqueName, qlonglong timestamp);
    void shortcutReleased(const QString &componentUniqueName,
                          const QString &actionUniqueName, qlonglong timestamp);

private:
    QList<DesktopNotificationActivationEvent> m_events;
};

} // namespace QindaQt::Test
