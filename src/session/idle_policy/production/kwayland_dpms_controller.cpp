// SPDX-License-Identifier: LGPL-3.0-or-later
#include "kwayland_dpms_controller.h"

#include <memory>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/dpms.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/registry.h>

#include <QDeadlineTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <wayland-client-core.h>

#include <algorithm>
#include <cerrno>
#include <poll.h>
#include <unistd.h>

using namespace KWayland::Client;

namespace QindaQt::Session::IdlePolicy {
namespace {

constexpr int ConnectTimeoutMilliseconds = 5'000;

} // namespace

KWaylandDpmsController::KWaylandDpmsController(QObject *parent)
    : AttachedDisplayPowerPort(parent)
{
}

KWaylandDpmsController::~KWaylandDpmsController() { stop(); }

bool KWaylandDpmsController::start(OpenAdmittedFd openAdmittedFd,
                                   LineageLive lineageLive,
                                   QString *error)
{
    stop();
    const quint64 generation = ++m_generation;
    m_stopping = false;
    if (!openAdmittedFd || !lineageLive || !lineageLive()) {
        if (error != nullptr) *error = QStringLiteral("compositor attachment is not live");
        return false;
    }
    const int admittedFd = openAdmittedFd();
    if (admittedFd < 0 || !lineageLive()) {
        if (admittedFd >= 0) ::close(admittedFd);
        if (error != nullptr) *error = QStringLiteral("admitted compositor FD is unavailable");
        return false;
    }
    m_lineageLive = std::move(lineageLive);

    m_connectionThread = new QThread(this);
    m_connection = new ConnectionThread;
    // AGENT-GUARD: this transferred FD is the admitted socket peer. Never
    // replace it with an environment-derived socket path (ADR-0305).
    m_connection->setSocketFd(admittedFd);
    m_connection->moveToThread(m_connectionThread);
    connect(m_connectionThread, &QThread::started, m_connection,
            &ConnectionThread::initConnection);
    connect(m_connection, &ConnectionThread::connectionDied, this, [this, generation] {
        if (generation != m_generation || m_stopping) return;
        m_connected = false;
        destroyProtocolObjects();
        Q_EMIT availabilityChanged(false);
        Q_EMIT powerChanged(false);
    });

    m_queue = new EventQueue(this);

    m_registry = new Registry(this);
    m_registry->setEventQueue(m_queue);
    connect(m_registry, &Registry::outputAnnounced, this,
            [this, generation](quint32 name, quint32 version) {
                if (generation != m_generation || m_stopping || m_registry == nullptr) return;
                Output *output = m_registry->createOutput(name, version, this);
                m_announcedOutputs.insert(name, output);
                attachOutput(output);
            });
    connect(m_registry, &Registry::outputRemoved, this, [this, generation](quint32 name) {
        if (generation != m_generation || m_stopping) return;
        Output *const output = m_announcedOutputs.take(name);
        if (output == nullptr) return;
        const auto dpmsObjects = m_dpmsObjects;
        for (Dpms *dpms : dpmsObjects) {
            if (dpms->output() == output) {
                m_dpmsObjects.removeOne(dpms);
                dpms->destroy();
                delete dpms;
            }
        }
        output->destroy();
        delete output;
        // Reset requested state before availability re-evaluates the new
        // output set, so an already-idle session powers newly added outputs.
        Q_EMIT powerChanged(false);
        Q_EMIT availabilityChanged(available());
    });
    // KWayland exposes no dedicated dpms announce signal; match the generic
    // announcement by the protocol name.
    connect(m_registry, &Registry::interfaceAnnounced, this,
            [this, generation](const QByteArray &interface, quint32 name, quint32 version) {
                if (generation != m_generation || m_stopping || m_registry == nullptr) return;
                if (interface != QByteArrayLiteral("org_kde_kwin_dpms_manager")) {
                    return;
                }
                if (m_manager != nullptr) {
                    return;
                }
                m_manager = m_registry->createDpmsManager(name, version, this);
                connect(m_manager, &DpmsManager::removed, this, [this, generation] {
                    if (generation != m_generation || m_stopping) return;
                    const auto dpmsObjects = m_dpmsObjects;
                    for (Dpms *dpms : dpmsObjects) {
                        m_dpmsObjects.removeOne(dpms);
                        dpms->destroy();
                        delete dpms;
                    }
                    m_dpmsObjects.clear();
                    m_manager->destroy();
                    delete m_manager;
                    m_manager = nullptr;
                    Q_EMIT powerChanged(false);
                    Q_EMIT availabilityChanged(false);
                });
                // AGENT-NOTE: outputs that announced before the manager could
                // not bind a Dpms; rebind them now that one exists.
                for (Output *output : std::as_const(m_announcedOutputs)) {
                    attachOutput(output);
                }
            });
    connect(m_registry, &Registry::interfacesAnnounced, this,
            [this, generation] {
                if (generation == m_generation && !m_stopping) m_interfacesAnnounced = true;
            });

    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_connection, &ConnectionThread::connected, &loop,
            [this, &loop, generation] {
                if (generation != m_generation || m_stopping || m_registry == nullptr) return;
                m_connected = true;
                // The display does not exist until ConnectionThread::connected;
                // setting up the event queue earlier binds it to no display.
                m_queue->setup(m_connection);
                connect(m_registry, &Registry::interfacesAnnounced, &loop,
                        &QEventLoop::quit);
                m_registry->create(m_connection);
                m_registry->setup();
            });
    connect(m_connection, &ConnectionThread::failed, &loop, &QEventLoop::quit);
    watchdog.start(ConnectTimeoutMilliseconds);
    m_connectionThread->start();
    loop.exec();
    watchdog.stop();

    if (!m_interfacesAnnounced || !m_lineageLive || !m_lineageLive()) {
        if (error != nullptr) *error = QStringLiteral("admitted Wayland registry is unavailable");
        stop();
        return false;
    }
    if (m_manager == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "the compositor did not announce org-kde-kwin-dpms");
        }
        stop();
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

bool KWaylandDpmsController::available() const
{
    const bool supportedOutput = std::any_of(
        m_dpmsObjects.cbegin(), m_dpmsObjects.cend(),
        [](const Dpms *entry) { return entry->isSupported(); });
    return m_manager != nullptr && supportedOutput &&
           m_lineageLive && m_lineageLive();
}

void KWaylandDpmsController::requestOff() { requestModeAll(false); }
void KWaylandDpmsController::requestOn() { requestModeAll(true); }

void KWaylandDpmsController::restoreAndStop()
{
    // The final On is sent on the already-admitted peer. Flush it on the
    // ConnectionThread before proxy/display teardown; quitting first can drop
    // KWayland's buffered request and leave a monitor powered down.
    requestModeAll(true, true);
    if (m_connection != nullptr && m_connectionThread != nullptr &&
        m_connectionThread->isRunning() && m_connected) {
        ConnectionThread *const connection = m_connection;
        QMetaObject::invokeMethod(
            connection, [connection] {
                connection->flush();
                wl_display *const display = connection->display();
                if (display == nullptr) return;
                const int fd = wl_display_get_fd(display);
                int remainingMilliseconds = 250;
                while (remainingMilliseconds >= 0) {
                    if (wl_display_flush(display) >= 0 || errno != EAGAIN) return;
                    pollfd writable{fd, POLLOUT, 0};
                    const int waited = poll(&writable, 1, remainingMilliseconds);
                    if (waited <= 0) return;
                    remainingMilliseconds = 0;
                }
            }, Qt::BlockingQueuedConnection);
    }
    stop();
}

void KWaylandDpmsController::destroyProtocolObjects()
{
    // All wrappers are owned by this thread and use the retained connection's
    // event queue. Destroy them before ConnectionThread disconnects so no
    // QObject or wl_proxy outlives its queue/display.
    for (Dpms *dpms : std::as_const(m_dpmsObjects)) {
        if (dpms != nullptr) dpms->destroy();
        delete dpms;
    }
    m_dpmsObjects.clear();
    if (m_manager != nullptr) {
        m_manager->destroy();
        delete m_manager;
        m_manager = nullptr;
    }
    for (Output *output : std::as_const(m_announcedOutputs)) {
        if (output != nullptr) output->destroy();
        delete output;
    }
    m_announcedOutputs.clear();
    if (m_registry != nullptr) {
        m_registry->destroy();
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_queue != nullptr) {
        m_queue->destroy();
        delete m_queue;
        m_queue = nullptr;
    }
    m_interfacesAnnounced = false;
}

void KWaylandDpmsController::stop()
{
    if (m_stopping) return;
    m_stopping = true;
    m_connected = false;
    ++m_generation;
    m_lineageLive = {};

    // Disconnect producers before destroying wrappers so already queued events
    // cannot mutate the next start() generation.
    if (m_registry != nullptr) QObject::disconnect(m_registry, nullptr, this, nullptr);
    if (m_connection != nullptr) QObject::disconnect(m_connection, nullptr, this, nullptr);
    destroyProtocolObjects();

    if (m_connection != nullptr) {
        // KWayland documents deleteLater followed by quit/wait: this deletes
        // the connection in its owning worker thread before disconnecting.
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_connectionThread != nullptr) {
        m_connectionThread->quit();
        m_connectionThread->wait();
        delete m_connectionThread;
        m_connectionThread = nullptr;
    }
    m_stopping = false;
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
    const quint64 generation = m_generation;
    connect(dpms, &Dpms::supportedChanged, this, [this, generation] {
        if (generation != m_generation || m_stopping) return;
        // Capability arrives asynchronously after the registry handshake.
        Q_EMIT powerChanged(false);
        Q_EMIT availabilityChanged(available());
    });
    connect(dpms, &Dpms::modeChanged, this, [this, generation] {
        if (generation != m_generation || m_stopping) return;
        const bool allOff = !m_dpmsObjects.isEmpty() &&
            std::all_of(m_dpmsObjects.cbegin(), m_dpmsObjects.cend(),
                        [](const Dpms *entry) {
                            return !entry->isSupported() ||
                                   entry->mode() == Dpms::Mode::Off;
                        });
        Q_EMIT powerChanged(allOff);
    });
    m_dpmsObjects.append(dpms);
    Q_EMIT availabilityChanged(available());
}

void KWaylandDpmsController::requestModeAll(const bool on,
                                                    const bool finalRestore)
{
    if (m_stopping || m_connection == nullptr ||
        (!finalRestore && (!m_lineageLive || !m_lineageLive()))) return;
    const Dpms::Mode mode = on ? Dpms::Mode::On : Dpms::Mode::Off;
    for (Dpms *dpms : std::as_const(m_dpmsObjects)) {
        if (dpms->isSupported()) {
            dpms->requestMode(mode);
        }
    }
}

std::unique_ptr<AttachedDisplayPowerPort> makeAttachedDisplayPowerPort()
{
    return std::make_unique<KWaylandDpmsController>();
}

} // namespace QindaQt::Session::IdlePolicy
