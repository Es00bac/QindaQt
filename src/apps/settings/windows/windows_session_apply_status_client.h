// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_windows/windows_values.h"

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusServiceWatcher;

namespace QindaQt::Apps::SettingsWindows {

class WindowsSessionApplyStatusClient final : public QObject {
    Q_OBJECT
public:
    explicit WindowsSessionApplyStatusClient(QDBusConnection bus, QObject *parent = nullptr);
    void start();
    void retryApply();
    [[nodiscard]] const SessionApplyStatus &status() const noexcept { return m_status; }

Q_SIGNALS:
    void statusChanged(const QindaQt::Apps::SettingsWindows::SessionApplyStatus &status);

private Q_SLOTS:
    void handleStateChanged(const QVariantMap &state);

private:
    void handleOwnerChanged(const QString &newOwner);
    void requestState();
    void publishStatus(SessionApplyStatus status);
    void publishUnavailable(const QString &message);
    [[nodiscard]] bool decodeState(const QVariantMap &wire,
                                   SessionApplyStatus *status,
                                   QString *error = nullptr) const;

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QString m_owner;
    SessionApplyStatus m_status;
    quint64 m_requestGeneration = 0;
    bool m_started = false;
};

} // namespace QindaQt::Apps::SettingsWindows
