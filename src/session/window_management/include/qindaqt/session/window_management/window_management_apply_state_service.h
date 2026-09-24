// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Session::WindowManagement {

class WindowManagementBridge;
struct WindowManagementApplyState;

// Public, session-owned readback boundary for the Windows Settings route.
// The bridge and bus connection are borrowed and must outlive this object;
// all access is confined to the session event-loop thread. start() exports the
// versioned ADR-0254 endpoint and reports registration errors without leaving
// a partial export; stop() is idempotent and releases both path and name.
class WindowManagementApplyStateService final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.WindowManagement1")
public:
    explicit WindowManagementApplyStateService(WindowManagementBridge &bridge,
                                               QDBusConnection bus,
                                               QObject *parent = nullptr);
    ~WindowManagementApplyStateService() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();

    static constexpr auto ServiceName = "org.qindaqt.WindowManagement1";
    static constexpr auto ObjectPath = "/org/qindaqt/WindowManagement1";
    static constexpr auto InterfaceName = "org.qindaqt.WindowManagement1";

public Q_SLOTS:
    [[nodiscard]] QVariantMap GetState() const;
    void RetryApply();

Q_SIGNALS:
    void StateChanged(const QVariantMap &state);

private:
    [[nodiscard]] static QVariantMap encodeState(
        const QindaQt::Session::WindowManagement::WindowManagementApplyState &state);

    WindowManagementBridge &m_bridge;
    QDBusConnection m_bus;
    bool m_started = false;
};

} // namespace QindaQt::Session::WindowManagement
