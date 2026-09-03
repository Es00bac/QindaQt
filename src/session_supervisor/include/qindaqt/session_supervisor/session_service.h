// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::SessionSupervisor {

class SessionProcessSupervisor;

// Owns the fixed Session1 name/object on one injected session bus. The
// supervisor remains process-lifecycle authority and must outlive this object.
// Name or object registration failure rolls back completely.
class SessionService final : public QObject {
    Q_OBJECT

public:
    SessionService(SessionProcessSupervisor &supervisor,
                   QDBusConnection connection,
                   QObject *parent = nullptr);
    ~SessionService() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    class Endpoint;
    SessionProcessSupervisor &m_supervisor;
    QDBusConnection m_connection;
    std::unique_ptr<Endpoint> m_endpoint;
    bool m_active = false;
};

} // namespace QindaQt::SessionSupervisor
