// SPDX-License-Identifier: GPL-3.0-or-later
// A private nested-session client probe for the installed power stack.

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <KScreenDpms/Dpms>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QVariantMap>

#include <wayland-client.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <poll.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr auto PowerService = "org.kde.Solid.PowerManagement";
constexpr auto PolicyPath = "/org/kde/Solid/PowerManagement/PolicyAgent";
constexpr auto PolicyInterface = "org.kde.Solid.PowerManagement.PolicyAgent";
constexpr auto ScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto ScreenSaverPath = "/ScreenSaver";
constexpr auto ScreenSaverInterface = "org.freedesktop.ScreenSaver";
constexpr auto PortalService = "org.freedesktop.portal.Desktop";
constexpr auto PortalPath = "/org/freedesktop/portal/desktop";
constexpr auto PortalInterface = "org.freedesktop.portal.Inhibit";
constexpr uint32_t ChangeScreenSettings = 4;

struct NativeSurface final {
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    wl_surface *surface = nullptr;
    wl_buffer *buffer = nullptr;
    xdg_wm_base *xdgBase = nullptr;
    xdg_surface *xdgSurface = nullptr;
    xdg_toplevel *toplevel = nullptr;
    zwp_idle_inhibit_manager_v1 *manager = nullptr;
    zwp_idle_inhibitor_v1 *inhibitor = nullptr;
    void *mapping = nullptr;
    std::size_t mappingLength = 0;
    int descriptor = -1;
    bool mapped = false;
    bool closed = false;
    QString error;

    ~NativeSurface()
    {
        releaseInhibitor();
        if (toplevel) {
            xdg_toplevel_destroy(toplevel);
        }
        if (xdgSurface) {
            xdg_surface_destroy(xdgSurface);
        }
        if (surface) {
            wl_surface_destroy(surface);
        }
        if (buffer) {
            wl_buffer_destroy(buffer);
        }
        if (manager) {
            zwp_idle_inhibit_manager_v1_destroy(manager);
        }
        if (xdgBase) {
            xdg_wm_base_destroy(xdgBase);
        }
        if (shm) {
            wl_shm_destroy(shm);
        }
        if (compositor) {
            wl_compositor_destroy(compositor);
        }
        if (registry) {
            wl_registry_destroy(registry);
        }
        if (mapping) {
            munmap(mapping, mappingLength);
        }
        if (descriptor >= 0) {
            close(descriptor);
        }
        if (display) {
            wl_display_disconnect(display);
        }
    }

    void releaseInhibitor()
    {
        if (inhibitor) {
            zwp_idle_inhibitor_v1_destroy(inhibitor);
            inhibitor = nullptr;
            wl_display_flush(display);
        }
    }
};

void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                    const char *interface, uint32_t version)
{
    auto &surface = *static_cast<NativeSurface *>(data);
    if (std::strcmp(interface, "wl_compositor") == 0 && !surface.compositor) {
        surface.compositor = static_cast<wl_compositor *>(wl_registry_bind(
            registry, name, &wl_compositor_interface, std::min(version, 4u)));
    } else if (std::strcmp(interface, "wl_shm") == 0 && !surface.shm) {
        surface.shm = static_cast<wl_shm *>(wl_registry_bind(
            registry, name, &wl_shm_interface, std::min(version, 1u)));
    } else if (std::strcmp(interface, "xdg_wm_base") == 0 && !surface.xdgBase) {
        surface.xdgBase = static_cast<xdg_wm_base *>(wl_registry_bind(
            registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    } else if (std::strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0
               && !surface.manager) {
        surface.manager = static_cast<zwp_idle_inhibit_manager_v1 *>(wl_registry_bind(
            registry, name, &zwp_idle_inhibit_manager_v1_interface, std::min(version, 1u)));
    }
}

void registryGlobalRemove(void *, wl_registry *, uint32_t) {}
const wl_registry_listener RegistryListener = {registryGlobal, registryGlobalRemove};

void xdgPing(void *, xdg_wm_base *base, uint32_t serial)
{
    xdg_wm_base_pong(base, serial);
}
const xdg_wm_base_listener XdgBaseListener = {xdgPing};

void surfaceConfigure(void *data, xdg_surface *xdgSurface, uint32_t serial)
{
    auto &surface = *static_cast<NativeSurface *>(data);
    xdg_surface_ack_configure(xdgSurface, serial);
    if (!surface.mapped && surface.buffer) {
        wl_surface_attach(surface.surface, surface.buffer, 0, 0);
        wl_surface_damage(surface.surface, 0, 0, 160, 120);
        wl_surface_commit(surface.surface);
        surface.mapped = true;
    }
}
const xdg_surface_listener XdgSurfaceListener = {surfaceConfigure};

void toplevelConfigure(void *, xdg_toplevel *, int32_t, int32_t, wl_array *) {}
void toplevelClose(void *data, xdg_toplevel *)
{
    static_cast<NativeSurface *>(data)->closed = true;
}
void toplevelConfigureBounds(void *, xdg_toplevel *, int32_t, int32_t) {}
void toplevelCapabilities(void *, xdg_toplevel *, wl_array *) {}
const xdg_toplevel_listener ToplevelListener = {
    toplevelConfigure, toplevelClose, toplevelConfigureBounds, toplevelCapabilities};

void bufferRelease(void *, wl_buffer *) {}
const wl_buffer_listener BufferListener = {bufferRelease};

bool createBuffer(NativeSurface &surface)
{
    char name[64];
    std::snprintf(name, sizeof(name), "/qindaqt-powerdevil-%ld",
                  static_cast<long>(getpid()));
    surface.descriptor = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (surface.descriptor < 0) {
        surface.error = QStringLiteral("shm_open failed: %1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    shm_unlink(name);
    constexpr std::size_t stride = 160 * 4;
    surface.mappingLength = stride * 120;
    if (ftruncate(surface.descriptor, static_cast<off_t>(surface.mappingLength)) != 0) {
        surface.error = QStringLiteral("ftruncate failed");
        return false;
    }
    surface.mapping = mmap(nullptr, surface.mappingLength, PROT_READ | PROT_WRITE,
                           MAP_SHARED, surface.descriptor, 0);
    if (surface.mapping == MAP_FAILED) {
        surface.mapping = nullptr;
        surface.error = QStringLiteral("mmap failed");
        return false;
    }
    std::fill_n(static_cast<uint32_t *>(surface.mapping),
                surface.mappingLength / sizeof(uint32_t), 0xff30485f);
    wl_shm_pool *pool = wl_shm_create_pool(
        surface.shm, surface.descriptor, static_cast<int32_t>(surface.mappingLength));
    if (!pool) {
        surface.error = QStringLiteral("wl_shm_create_pool failed");
        return false;
    }
    surface.buffer = wl_shm_pool_create_buffer(
        pool, 0, 160, 120, static_cast<int32_t>(stride), WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    if (!surface.buffer) {
        surface.error = QStringLiteral("wl_shm_pool_create_buffer failed");
        return false;
    }
    wl_buffer_add_listener(surface.buffer, &BufferListener, &surface);
    return true;
}

bool createNativeInhibitor(NativeSurface &surface)
{
    surface.display = wl_display_connect(nullptr);
    if (!surface.display) {
        surface.error = QStringLiteral("could not connect a native Wayland client");
        return false;
    }
    surface.registry = wl_display_get_registry(surface.display);
    wl_registry_add_listener(surface.registry, &RegistryListener, &surface);
    if (wl_display_roundtrip(surface.display) < 0 || !surface.compositor || !surface.shm
        || !surface.xdgBase || !surface.manager) {
        surface.error = QStringLiteral("nested KWin omitted a required idle-inhibit global");
        return false;
    }
    xdg_wm_base_add_listener(surface.xdgBase, &XdgBaseListener, &surface);
    surface.surface = wl_compositor_create_surface(surface.compositor);
    if (!surface.surface || !createBuffer(surface)) {
        return false;
    }
    surface.xdgSurface = xdg_wm_base_get_xdg_surface(surface.xdgBase, surface.surface);
    surface.toplevel = xdg_surface_get_toplevel(surface.xdgSurface);
    if (!surface.xdgSurface || !surface.toplevel) {
        surface.error = QStringLiteral("could not create the native toplevel");
        return false;
    }
    xdg_surface_add_listener(surface.xdgSurface, &XdgSurfaceListener, &surface);
    xdg_toplevel_add_listener(surface.toplevel, &ToplevelListener, &surface);
    xdg_toplevel_set_title(surface.toplevel, "QindaQt PowerDevil inhibition probe");
    xdg_toplevel_set_app_id(surface.toplevel, "org.qindaqt.powerdevil-probe");
    wl_surface_commit(surface.surface);
    if (wl_display_roundtrip(surface.display) < 0 || !surface.mapped) {
        surface.error = QStringLiteral("native inhibitor surface was not mapped");
        return false;
    }
    surface.inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        surface.manager, surface.surface);
    if (!surface.inhibitor || wl_display_roundtrip(surface.display) < 0) {
        surface.error = QStringLiteral("native idle inhibitor creation failed");
        return false;
    }
    return true;
}

bool processUntil(const std::function<bool()> &predicate, int timeoutMilliseconds,
                  NativeSurface *native = nullptr)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (native && native->display) {
            pollfd descriptor{wl_display_get_fd(native->display), POLLIN, 0};
            if (poll(&descriptor, 1, 0) > 0 && (descriptor.revents & POLLIN)) {
                if (wl_display_dispatch(native->display) < 0) {
                    native->error = QStringLiteral("native Wayland connection failed");
                    return false;
                }
            } else {
                wl_display_dispatch_pending(native->display);
                wl_display_flush(native->display);
            }
            if (native->closed) {
                native->error = QStringLiteral("native inhibitor surface was closed");
                return false;
            }
        }
        if (predicate()) {
            return true;
        }
        QThread::msleep(20);
    }
    return predicate();
}

QDBusInterface policyAgent()
{
    return QDBusInterface(QString::fromLatin1(PowerService), QString::fromLatin1(PolicyPath),
                          QString::fromLatin1(PolicyInterface),
                          QDBusConnection::sessionBus());
}

bool hasPolicyInhibition(QString *error)
{
    error->clear();
    QDBusInterface policy = policyAgent();
    const QDBusReply<bool> reply = policy.call(QStringLiteral("HasInhibition"),
                                                ChangeScreenSettings);
    if (!reply.isValid()) {
        *error = QStringLiteral("HasInhibition failed: %1").arg(reply.error().message());
        return false;
    }
    return reply.value();
}

bool simulateUserActivity(QString *error)
{
    error->clear();
    QDBusInterface screenSaver(QString::fromLatin1(ScreenSaverService),
                               QString::fromLatin1(ScreenSaverPath),
                               QString::fromLatin1(ScreenSaverInterface),
                               QDBusConnection::sessionBus());
    const QDBusMessage reply = screenSaver.call(QStringLiteral("SimulateUserActivity"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("SimulateUserActivity failed: %1").arg(reply.errorMessage());
        return false;
    }
    return true;
}

int fail(QJsonObject &document, const QString &message)
{
    document.insert(QStringLiteral("outcome"), QStringLiteral("failure"));
    document.insert(QStringLiteral("failure"), message);
    std::puts(QJsonDocument(document).toJson(QJsonDocument::Compact).constData());
    return 1;
}

} // namespace

int runMode(QGuiApplication &application, const QString &mode)
{
    QJsonObject document{{QStringLiteral("schemaVersion"), 1},
                         {QStringLiteral("mode"), mode}};
    KScreen::Dpms dpms;
    if (!processUntil([&dpms] { return dpms.isSupported(); }, 5000)) {
        return fail(document, QStringLiteral("KScreen DPMS is unsupported on nested KWin"));
    }

    QElapsedTimer phaseClock;
    phaseClock.start();
    QJsonArray dpmsEvents;
    QObject::connect(&dpms, &KScreen::Dpms::modeChanged, &application,
                     [&dpmsEvents, &phaseClock](KScreen::Dpms::Mode value, QScreen *screen) {
                         dpmsEvents.append(QJsonObject{
                             {QStringLiteral("mode"), static_cast<int>(value)},
                             {QStringLiteral("screen"), screen ? screen->name() : QString()},
                             {QStringLiteral("elapsedMs"), phaseClock.elapsed()},
                         });
                     });
    dpms.switchMode(KScreen::Dpms::On);
    if (!processUntil([&dpms] { return !dpms.hasPendingChanges(); }, 5000)) {
        return fail(document, QStringLiteral("nested output did not reach initial DPMS On"));
    }

    QString error;
    NativeSurface native;
    QDBusObjectPath portalRequest;
    uint legacyCookie = 0;
    if (mode == QStringLiteral("native")) {
        if (!createNativeInhibitor(native)) {
            return fail(document, native.error);
        }
    } else if (mode == QStringLiteral("portal")) {
        QDBusInterface portal(QString::fromLatin1(PortalService),
                              QString::fromLatin1(PortalPath),
                              QString::fromLatin1(PortalInterface),
                              QDBusConnection::sessionBus());
        QVariantMap options{{QStringLiteral("handle_token"),
                             QStringLiteral("qindaqt_powerdevil_probe")},
                            {QStringLiteral("reason"),
                             QStringLiteral("QindaQt nested idle qualification")}};
        const QDBusReply<QDBusObjectPath> reply = portal.call(
            QStringLiteral("Inhibit"), QString(), uint(8), options);
        if (!reply.isValid()) {
            return fail(document,
                        QStringLiteral("portal Idle inhibition failed: %1")
                            .arg(reply.error().message()));
        }
        portalRequest = reply.value();
        document.insert(QStringLiteral("portalRequest"), portalRequest.path());
    } else {
        QDBusInterface screenSaver(QString::fromLatin1(ScreenSaverService),
                                   QString::fromLatin1(ScreenSaverPath),
                                   QString::fromLatin1(ScreenSaverInterface),
                                   QDBusConnection::sessionBus());
        const QDBusReply<uint> reply = screenSaver.call(
            QStringLiteral("Inhibit"), QStringLiteral("QindaQt qualification"),
            QStringLiteral("Verify legacy idle inhibition"));
        if (!reply.isValid() || reply.value() == 0) {
            return fail(document,
                        QStringLiteral("legacy ScreenSaver inhibition failed: %1")
                            .arg(reply.error().message()));
        }
        legacyCookie = reply.value();
        document.insert(QStringLiteral("legacyCookie"), static_cast<int>(legacyCookie));
    }

    const bool expectsPolicy = mode != QStringLiteral("native");
    bool policyWhileHeld = false;
    if (expectsPolicy) {
        if (!processUntil([&] { policyWhileHeld = hasPolicyInhibition(&error); return policyWhileHeld; },
                          7000, mode == QStringLiteral("native") ? &native : nullptr)) {
            return fail(document, error.isEmpty()
                                      ? QStringLiteral("PowerDevil did not publish policy type 4")
                                      : error);
        }
    } else {
        policyWhileHeld = hasPolicyInhibition(&error);
        if (!error.isEmpty()) {
            return fail(document, error);
        }
        if (policyWhileHeld) {
            return fail(document, QStringLiteral("native inhibition unexpectedly used PolicyAgent"));
        }
    }
    document.insert(QStringLiteral("policyWhileHeld"), policyWhileHeld);
    if (!simulateUserActivity(&error)) {
        return fail(document, error);
    }

    phaseClock.start();
    const auto offCount = [&dpmsEvents] {
        return std::count_if(dpmsEvents.begin(), dpmsEvents.end(), [](const QJsonValue &value) {
            return value.toObject().value(QStringLiteral("mode")).toInt()
                == static_cast<int>(KScreen::Dpms::Off);
        });
    };
    processUntil([] { return false; }, 35000,
                 mode == QStringLiteral("native") ? &native : nullptr);
    if (mode == QStringLiteral("native") && !native.error.isEmpty()) {
        return fail(document, native.error);
    }
    if (offCount() != 0) {
        document.insert(QStringLiteral("dpmsEvents"), dpmsEvents);
        return fail(document, QStringLiteral("display powered off while inhibition was held"));
    }

    if (mode == QStringLiteral("native")) {
        native.releaseInhibitor();
    } else if (mode == QStringLiteral("portal")) {
        QDBusInterface request(QString::fromLatin1(PortalService), portalRequest.path(),
                               QStringLiteral("org.freedesktop.portal.Request"),
                               QDBusConnection::sessionBus());
        const QDBusMessage reply = request.call(QStringLiteral("Close"));
        if (reply.type() == QDBusMessage::ErrorMessage) {
            return fail(document,
                        QStringLiteral("portal request release failed: %1")
                            .arg(reply.errorMessage()));
        }
    } else {
        QDBusInterface screenSaver(QString::fromLatin1(ScreenSaverService),
                                   QString::fromLatin1(ScreenSaverPath),
                                   QString::fromLatin1(ScreenSaverInterface),
                                   QDBusConnection::sessionBus());
        const QDBusMessage reply = screenSaver.call(QStringLiteral("UnInhibit"), legacyCookie);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            return fail(document,
                        QStringLiteral("legacy ScreenSaver release failed: %1")
                            .arg(reply.errorMessage()));
        }
    }

    bool policyAfterRelease = true;
    if (expectsPolicy
        && !processUntil([&] { policyAfterRelease = hasPolicyInhibition(&error); return !policyAfterRelease; },
                         7000, mode == QStringLiteral("native") ? &native : nullptr)) {
        return fail(document, error.isEmpty()
                                  ? QStringLiteral("PowerDevil retained policy type 4 after release")
                                  : error);
    }
    if (!expectsPolicy) {
        policyAfterRelease = hasPolicyInhibition(&error);
        if (!error.isEmpty()) {
            return fail(document, error);
        }
    }
    document.insert(QStringLiteral("policyAfterRelease"), policyAfterRelease);

    if (!processUntil([&] { return offCount() == 1; }, 35000,
                      mode == QStringLiteral("native") ? &native : nullptr)) {
        document.insert(QStringLiteral("dpmsEvents"), dpmsEvents);
        return fail(document, QStringLiteral("display did not power off once after release"));
    }
    const auto onCount = [&dpmsEvents] {
        return std::count_if(dpmsEvents.begin(), dpmsEvents.end(), [](const QJsonValue &value) {
            return value.toObject().value(QStringLiteral("mode")).toInt()
                == static_cast<int>(KScreen::Dpms::On);
        });
    };
    const qsizetype priorOnCount = onCount();
    dpms.switchMode(KScreen::Dpms::On);
    if (!processUntil([&] { return onCount() > priorOnCount && !dpms.hasPendingChanges(); },
                      5000, mode == QStringLiteral("native") ? &native : nullptr)) {
        document.insert(QStringLiteral("dpmsEvents"), dpmsEvents);
        return fail(document, QStringLiteral("nested output did not restore DPMS On"));
    }

    document.insert(QStringLiteral("dpmsEvents"), dpmsEvents);
    document.insert(QStringLiteral("offTransitions"), offCount());
    document.insert(QStringLiteral("outcome"), QStringLiteral("success"));
    std::puts(QJsonDocument(document).toJson(QJsonDocument::Compact).constData());
    return 0;
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() != 3 || arguments.at(1) != QStringLiteral("--mode")) {
        std::fprintf(stderr, "usage: %s --mode native|portal|legacy\n", argv[0]);
        return 2;
    }
    const QString mode = arguments.at(2);
    if (mode != QStringLiteral("native") && mode != QStringLiteral("portal")
        && mode != QStringLiteral("legacy")) {
        std::fprintf(stderr, "unknown inhibition mode\n");
        return 2;
    }
    return runMode(application, mode);
}
