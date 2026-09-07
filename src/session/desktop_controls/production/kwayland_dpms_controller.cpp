// SPDX-License-Identifier: LGPL-3.0-or-later
#include "kwayland_dpms_controller.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/dpms.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/registry.h>

#include <QDeadlineTimer>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <algorithm>

using namespace KWayland::Client;

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr int ConnectTimeoutMilliseconds = 5'000;

} // namespace

KWaylandDpmsController::KWaylandDpmsController(QObject *parent)
    : DpmsController(parent)
{
}

KWaylandDpmsController::~KWaylandDpmsController()
{
    if (m_connectionThread != nullptr) {
        m_connectionThread->quit();
        m_connectionThread->wait();
        m_connectionThread->deleteLater();
    }
}

bool KWaylandDpmsController::start(QString *error)
{
    const QString socketName = qEnvironmentVariable("WAYLAND_DISPLAY");
    if (socketName.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("WAYLAND_DISPLAY is not set");
        }
        return false;
    }

    m_connectionThread = new QThread(this);
    m_connection = new ConnectionThread;
    m_connection->moveToThread(m_connectionThread);
    connect(m_connectionThread, &QThread::started, m_connection,
            &ConnectionThread::initConnection);
    connect(m_connection, &ConnectionThread::connectionDied, this, [this] {
        std::for_each(m_dpmsObjects.cbegin(), m_dpmsObjects.cend(),
                      [](Dpms *dpms) { dpms->destroy(); });
        m_dpmsObjects.clear();
        m_manager = nullptr;
        Q_EMIT availabilityChanged(false);
        Q_EMIT displaysPowerChanged(false);
    });

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    m_registry = new Registry(this);
    m_registry->setEventQueue(m_queue);
    connect(m_registry, &Registry::outputAnnounced, this,
            [this](quint32 name, quint32 version) {
                Output *output = m_registry->createOutput(name, version, this);
                m_announcedOutputs.append(output);
                attachOutput(output);
            });
    // KWayland exposes no dedicated dpms announce signal; match the generic
    // announcement by the protocol name.
    connect(m_registry, &Registry::interfaceAnnounced, this,
            [this](const QByteArray &interface, quint32 name, quint32 version) {
                if (interface != QByteArrayLiteral("org_kde_kwin_dpms_manager")) {
                    return;
                }
                if (m_manager != nullptr) {
                    return;
                }
                m_manager = m_registry->createDpmsManager(name, version, this);
                // AGENT-NOTE: outputs that announced before the manager could
                // not bind a Dpms; rebind them now that one exists.
                for (Output *output : std::as_const(m_announcedOutputs)) {
                    attachOutput(output);
                }
            });
    connect(m_registry, &Registry::interfacesAnnounced, this,
            [this] { m_interfacesAnnounced = true; });

    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_connection, &ConnectionThread::connected, &loop,
            [this, &loop] {
                connect(m_registry, &Registry::interfacesAnnounced, &loop,
                        &QEventLoop::quit);
                m_registry->create(m_connection);
            });
    connect(m_connection, &ConnectionThread::failed, &loop, &QEventLoop::quit);
    watchdog.start(ConnectTimeoutMilliseconds);
    m_connectionThread->start();
    loop.exec();
    watchdog.stop();

    if (!m_interfacesAnnounced) {
        if (error != nullptr) {
            *error = QStringLiteral("Wayland registry announcement timed out on %1")
                         .arg(socketName);
        }
        return false;
    }
    if (m_manager == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "the compositor did not announce org-kde-kwin-dpms");
        }
        return false;
    }
    return true;
}

bool KWaylandDpmsController::available() const
{
    return m_manager != nullptr && !m_dpmsObjects.isEmpty();
}

void KWaylandDpmsController::requestDisplaysOff()
{
    requestModeAll(Dpms::Mode::Off);
}

void KWaylandDpmsController::requestDisplaysOn()
{
    requestModeAll(Dpms::Mode::On);
}

void KWaylandDpmsController::attachOutput(Output *output)
{
    if (output == nullptr || m_manager == nullptr) {
        return;
    }
    for (const Dpms *existing : m_dpmsObjects) {
        if (existing->output() == output) {
            return;
        }
    }
    auto *const dpms = m_manager->getDpms(output, this);
    connect(dpms, &Dpms::modeChanged, this, [this, dpms] {
        const bool off = dpms->mode() == Dpms::Mode::Off;
        const bool allOff = std::all_of(
            m_dpmsObjects.cbegin(), m_dpmsObjects.cend(), [](const Dpms *entry) {
                return entry->mode() == Dpms::Mode::Off;
            });
        Q_EMIT displaysPowerChanged(off && allOff);
    });
    m_dpmsObjects.append(dpms);
    if (m_dpmsObjects.size() == 1) {
        // First controllable output: the controller becomes available.
        Q_EMIT availabilityChanged(true);
    }
}

void KWaylandDpmsController::requestModeAll(Dpms::Mode mode)
{
    for (Dpms *dpms : std::as_const(m_dpmsObjects)) {
        if (dpms->isSupported()) {
            dpms->requestMode(mode);
        }
    }
}

} // namespace QindaQt::Session::DesktopControls
