// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/session/idle_policy/attached_display_power_port.h>
#include <QHash>
#include <QList>
#include <functional>

namespace KWayland::Client {
class ConnectionThread;
class Dpms;
class DpmsManager;
class EventQueue;
class Output;
class Registry;
}
class QThread;

namespace QindaQt::Session::IdlePolicy {

// KWin DPMS client bound to one transferred, already-connected ordinary
// compositor FD. It never reconnects by WAYLAND_DISPLAY. The FD opener and
// lineage predicate are borrowed and must outlive this controller.
class KWaylandDpmsController final : public AttachedDisplayPowerPort {
    Q_OBJECT
public:
    explicit KWaylandDpmsController(QObject *parent = nullptr);
    ~KWaylandDpmsController() override;
    bool start(OpenAdmittedFd openAdmittedFd,
               LineageLive lineageLive, QString *error = nullptr) override;
    bool available() const override;
    void requestOff() override;
    void requestOn() override;
    void restoreAndStop() override;
    void stop();
private:
    void attachOutput(KWayland::Client::Output *output);
    void requestModeAll(bool on, bool finalRestore = false);
    void destroyProtocolObjects();
    std::function<bool()> m_lineageLive;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::DpmsManager *m_manager = nullptr;
    QHash<quint32, KWayland::Client::Output *> m_announcedOutputs;
    QList<KWayland::Client::Dpms *> m_dpmsObjects;
    QThread *m_connectionThread = nullptr;
    bool m_interfacesAnnounced = false;
    bool m_stopping = false;
    bool m_connected = false;
    quint64 m_generation = 0;
};

} // namespace QindaQt::Session::IdlePolicy
