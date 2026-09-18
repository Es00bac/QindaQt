// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

namespace QindaQt::Session::WindowManagement {

// Seam between the bridge and KWin: production asks the compositor to
// re-read its configuration over the session bus; tests count the asks.
class KWinReconfigureRequester {
public:
    virtual ~KWinReconfigureRequester() = default;
    virtual void requestReconfigure() = 0;
};

// `org.kde.KWin /KWin org.kde.KWin.reconfigure`, asynchronous: a missing or
// slow compositor never blocks the session process. Failures are logged.
class DBusKWinReconfigureRequester final : public QObject, public KWinReconfigureRequester {
    Q_OBJECT
public:
    explicit DBusKWinReconfigureRequester(QDBusConnection bus, QObject *parent = nullptr);
    void requestReconfigure() override;

    [[nodiscard]] int requestCount() const noexcept { return m_requests; }

private:
    QDBusConnection m_bus;
    int m_requests = 0;
};

} // namespace QindaQt::Session::WindowManagement
