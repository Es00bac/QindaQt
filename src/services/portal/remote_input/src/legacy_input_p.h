// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <functional>
#include <utility>

struct ei;
struct ei_device;
struct ei_touch;
class QSocketNotifier;
namespace QindaQt::Services::Portal::RemoteInput {
// Module-private libei sender serving the deprecated RemoteDesktop Notify*
// calls over the session's own consented compositor EIS context.
// AGENT-NOTE: xdg-desktop-portal 1.20 forwards Notify* without awaiting a
// reply (remote-desktop.c), so refusing them silently dropped legacy input.
// Upstream KDE drives KWin fake-input; this keeps one compositor injection
// path with the same device limits, owner admission and lock/close teardown.
// Same-thread. attach() takes ownership of the transport FD. Events issued
// before a device with the needed capability resumes wait in a bounded FIFO.
class LegacyInput final : public QObject {
    Q_OBJECT
public:
    enum class Need { Motion, Absolute, Button, Scroll, Keyboard, Touch };
    using QObject::QObject;
    ~LegacyInput() override;
    bool attach(int fd);
    void motion(double dx, double dy);
    void absolute(double x, double y); // compositor logical coordinates
    void button(quint32 code, bool pressed);
    void axis(double dx, double dy, bool finish);
    void discrete(quint32 axis, qint32 steps);
    void key(quint32 code, bool pressed);
    void touchDown(quint32 slot, double x, double y);
    void touchMotion(quint32 slot, double x, double y);
    void touchUp(quint32 slot);
Q_SIGNALS:
    // The compositor disconnected the context; the owner ends the session.
    void lost();
private:
    void dispatch();
    void emitOrQueue(Need, std::function<void(ei_device *)>);
    void flush();
    ei_device *device(Need) const;
    void forget(ei_device *);
    ei *m_ei = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QList<ei_device *> m_devices; // resumed and emulating, referenced
    QHash<quint32, ei_touch *> m_touches;
    QList<std::pair<Need, std::function<void(ei_device *)>>> m_pending;
    quint32 m_sequence = 0;
};
} // namespace QindaQt::Services::Portal::RemoteInput
