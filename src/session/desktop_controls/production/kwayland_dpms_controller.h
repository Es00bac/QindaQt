// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/session/desktop_controls/dpms_controller.h"

#include <KWayland/Client/dpms.h>

#include <QList>
#include <QString>

namespace KWayland {
namespace Client {
class ConnectionThread;
class EventQueue;
class Output;
class Registry;
} // namespace Client
} // namespace KWayland

class QThread;

namespace QindaQt::Session::DesktopControls {

// Production display power control through org-kde-kwin-dpms on a dedicated
// Wayland client connection to the session display. The compositor wakes
// outputs on input; observed mode changes are reported so the policy never
// keeps a stale off state.
class KWaylandDpmsController final : public DpmsController {
    Q_OBJECT

public:
    explicit KWaylandDpmsController(QObject *parent = nullptr);
    ~KWaylandDpmsController() override;

    KWaylandDpmsController(const KWaylandDpmsController &) = delete;
    KWaylandDpmsController &operator=(const KWaylandDpmsController &) = delete;

    // Bounded blocking connect against the WAYLAND_DISPLAY socket. A missing
    // socket, a failed bind, or an absent dpms global leaves the controller
    // honestly unavailable; it never retries on its own.
    [[nodiscard]] bool start(QString *error = nullptr);

    [[nodiscard]] bool available() const override;
    void requestDisplaysOff() override;
    void requestDisplaysOn() override;

private:
    void attachOutput(KWayland::Client::Output *output);
    void requestModeAll(KWayland::Client::Dpms::Mode mode);

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::DpmsManager *m_manager = nullptr;
    QList<KWayland::Client::Output *> m_announcedOutputs;
    QList<KWayland::Client::Dpms *> m_dpmsObjects;
    QThread *m_connectionThread = nullptr;
    bool m_interfacesAnnounced = false;
};

} // namespace QindaQt::Session::DesktopControls
