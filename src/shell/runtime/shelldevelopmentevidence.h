// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QJsonObject>
#include <QObject>

namespace QindaQt::Services::NotificationPresentationModel {
class NotificationPresentationController;
}
namespace QindaQt::Services::NotificationPresentationPolicy {
class NotificationPrivacyPolicy;
}
namespace QindaQt::Services::SettingsClient {
class DoNotDisturbController;
}

namespace QindaQt::Shell {

class NotificationWindowController;

// A development-session-only, read-only view of production notification state.
// The endpoint intentionally has no mutation slots: input and user operations
// must continue through KWin and the real QML controls during live qualification.
class ShellDevelopmentEvidence final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.ShellDevelopment1")

public:
    static constexpr auto ServiceName = "org.qindaqt.ShellDevelopment";
    static constexpr auto ObjectPath = "/org/qindaqt/ShellDevelopment";

    ShellDevelopmentEvidence(
        Services::NotificationPresentationModel::NotificationPresentationController &
            presentation,
        Services::SettingsClient::DoNotDisturbController &quieting,
        Services::NotificationPresentationPolicy::NotificationPrivacyPolicy &privacy,
        NotificationWindowController &windows, QObject *parent = nullptr);
    ~ShellDevelopmentEvidence() override;

    // Registration requires both the launcher's development marker and an
    // authenticated compositor service owned by the supervisor-provided PID.
    [[nodiscard]] bool start(qint64 compositorProcessId,
                             qint64 predecessorShellProcessId = 0,
                             QString *error = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE [[nodiscard]] QByteArray Snapshot() const;

private:
    [[nodiscard]] QJsonObject snapshotObject() const;
    [[nodiscard]] bool registerServiceAfterPredecessor(
        qint64 predecessorShellProcessId, QString *error);
    void observeCenterChange();
    void observeBusyChange();
    void observeErrorChange();
    void observePrivacyChange(bool allowed);
    void observeQuietingChange();

    Services::NotificationPresentationModel::NotificationPresentationController &
        m_presentation;
    Services::SettingsClient::DoNotDisturbController &m_quieting;
    Services::NotificationPresentationPolicy::NotificationPrivacyPolicy &m_privacy;
    NotificationWindowController &m_windows;
    QDBusConnection m_bus;
    quint64 m_centerOpenedCount = 0;
    quint64 m_centerClosedCount = 0;
    quint64 m_busyVisibleCount = 0;
    quint64 m_errorVisibleCount = 0;
    quint64 m_privacyDeniedClearCount = 0;
    quint64 m_quietingStateChangeCount = 0;
    // These counters mean the UI produced by a quieting state edge became
    // visible on the following event turn; they are not current-state flags.
    quint64 m_quietingSavingVisibleCount = 0;
    quint64 m_quietingErrorVisibleCount = 0;
    quint64 m_quietingUnavailableVisibleCount = 0;
    bool m_registeredService = false;
    bool m_registeredObject = false;
};

} // namespace QindaQt::Shell
