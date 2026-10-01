// SPDX-License-Identifier: GPL-3.0-or-later
#include "dpms_wayland_fixture.h"

#include <QMetaObject>
#include <QTimer>

#include <sys/socket.h>
#include <unistd.h>

namespace {
const struct wl_output_interface s_outputImplementation{};
const struct org_kde_kwin_dpms_interface s_dpmsImplementation{
    &DpmsWaylandFixture::setMode,
    &DpmsWaylandFixture::releaseDpms,
};
const struct org_kde_kwin_dpms_manager_interface s_managerImplementation{
    &DpmsWaylandFixture::getDpms,
};

extern const wl_interface s_dpmsInterface;
const wl_interface *s_dpmsManagerGetTypes[] = {&s_dpmsInterface, &wl_output_interface};
const wl_message s_dpmsManagerRequests[] = {
    {"get", "no", s_dpmsManagerGetTypes}};
const wl_message s_dpmsRequests[] = {{"set", "u", nullptr}, {"release", "", nullptr}};
const wl_message s_dpmsEvents[] = {{"supported", "u", nullptr},
                                   {"mode", "u", nullptr}, {"done", "", nullptr}};
const wl_interface s_dpmsInterface{"org_kde_kwin_dpms", 1, 2,
                                   s_dpmsRequests, 3, s_dpmsEvents};
const wl_interface s_dpmsManagerInterface{"org_kde_kwin_dpms_manager", 1, 1,
                                          s_dpmsManagerRequests, 0, nullptr};
} // namespace

DpmsWaylandFixture::DpmsWaylandFixture()
    : m_worker(new QObject)
{
    m_worker->moveToThread(&m_serverThread);
    QObject::connect(&m_serverThread, &QThread::started, m_worker, [this] {
        m_display = wl_display_create();
        m_loop = wl_display_get_event_loop(m_display);
        m_managerGlobal = wl_global_create(m_display, &s_dpmsManagerInterface,
                                           1, this, &bindManager);
        m_watcher = std::make_unique<QSocketNotifier>(wl_event_loop_get_fd(m_loop),
                                                       QSocketNotifier::Read);
        QObject::connect(m_watcher.get(), &QSocketNotifier::activated, m_watcher.get(),
                         [this] { dispatch(); });
    });
    m_serverThread.start();
    runOnServerThread([] {});
}

DpmsWaylandFixture::~DpmsWaylandFixture()
{
    runOnServerThread([this] {
        m_watcher.reset();
        wl_display_destroy_clients(m_display);
        for (wl_global *global : m_outputs) wl_global_destroy(global);
        for (OutputBinding *binding : m_outputBindings) delete binding;
        wl_global_destroy(m_managerGlobal);
        wl_display_destroy(m_display);
        m_display = nullptr;
        m_loop = nullptr;
    });
    m_serverThread.quit();
    m_serverThread.wait();
    delete m_worker;
}

void DpmsWaylandFixture::runOnServerThread(std::function<void()> action)
{
    QMetaObject::invokeMethod(m_worker, std::move(action), Qt::BlockingQueuedConnection);
}

int DpmsWaylandFixture::openClientFd()
{
    int result = -1;
    runOnServerThread([this, &result] {
        int fds[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) return;
        if (wl_client_create(m_display, fds[0]) == nullptr) {
            close(fds[0]);
            close(fds[1]);
            return;
        }
        result = fds[1];
    });
    return result;
}

quint32 DpmsWaylandFixture::addOutput()
{
    quint32 result = 0;
    runOnServerThread([this, &result] {
        const quint32 name = m_nextOutput++;
        auto *const binding = new OutputBinding{this, name};
        wl_global *const global = wl_global_create(m_display, &wl_output_interface, 1,
                                                    binding, &bindOutput);
        if (global == nullptr) {
            delete binding;
            return;
        }
        m_outputBindings.insert(name, binding);
        m_outputs.insert(name, global);
        wl_display_flush_clients(m_display);
        result = name;
    });
    return result;
}

void DpmsWaylandFixture::removeOutput(const quint32 name)
{
    runOnServerThread([this, name] {
        const auto dpmsResources = m_dpmsResources;
        for (wl_resource *resource : dpmsResources) {
            auto *const binding = static_cast<DpmsBinding *>(wl_resource_get_user_data(resource));
            if (binding->outputName == name) wl_resource_destroy(resource);
        }
        const auto outputResources = m_outputResources.take(name);
        for (wl_resource *resource : outputResources) wl_resource_destroy(resource);
        wl_global *const global = m_outputs.take(name);
        if (global != nullptr) wl_global_destroy(global);
        delete m_outputBindings.take(name);
        wl_display_flush_clients(m_display);
    });
}

void DpmsWaylandFixture::pauseDispatch(const int milliseconds)
{
    runOnServerThread([this, milliseconds] {
        m_watcher->setEnabled(false);
        QTimer::singleShot(milliseconds, m_worker, [this] {
            m_watcher->setEnabled(true);
            dispatch();
        });
    });
}

void DpmsWaylandFixture::disconnectClients()
{
    runOnServerThread([this] { wl_display_destroy_clients(m_display); });
}

void DpmsWaylandFixture::dispatch()
{
    wl_event_loop_dispatch(m_loop, 0);
    wl_display_flush_clients(m_display);
}

void DpmsWaylandFixture::bindOutput(wl_client *client, void *data, const uint32_t version,
                                    const uint32_t id)
{
    auto *const globalBinding = static_cast<OutputBinding *>(data);
    wl_resource *const resource = wl_resource_create(client, &wl_output_interface,
                                                     static_cast<int>(version), id);
    if (resource == nullptr) return;
    auto *const resourceBinding = new OutputBinding{globalBinding->fixture,
                                                    globalBinding->name};
    wl_resource_set_implementation(resource, &s_outputImplementation,
                                   resourceBinding, &destroyOutputResource);
    globalBinding->fixture->m_outputResources[globalBinding->name].append(resource);
}

void DpmsWaylandFixture::bindManager(wl_client *client, void *data, const uint32_t version,
                                     const uint32_t id)
{
    auto *const fixture = static_cast<DpmsWaylandFixture *>(data);
    wl_resource *const resource = wl_resource_create(
        client, &s_dpmsManagerInterface, static_cast<int>(version), id);
    if (resource != nullptr)
        wl_resource_set_implementation(resource, &s_managerImplementation, fixture, nullptr);
}

void DpmsWaylandFixture::getDpms(wl_client *client, wl_resource *resource,
                                 const uint32_t id, wl_resource *output)
{
    auto *const fixture = static_cast<DpmsWaylandFixture *>(wl_resource_get_user_data(resource));
    auto *const outputBinding = static_cast<OutputBinding *>(wl_resource_get_user_data(output));
    wl_resource *const dpms = wl_resource_create(client, &s_dpmsInterface, 1, id);
    if (dpms == nullptr) return;
    auto *const dpmsBinding = new DpmsBinding{fixture, outputBinding->name};
    wl_resource_set_implementation(dpms, &s_dpmsImplementation,
                                   dpmsBinding, &destroyDpmsResource);
    fixture->m_dpmsResources.append(dpms);
    org_kde_kwin_dpms_send_supported(dpms, 1);
    org_kde_kwin_dpms_send_mode(dpms, ORG_KDE_KWIN_DPMS_MODE_ON);
    org_kde_kwin_dpms_send_done(dpms);
}

void DpmsWaylandFixture::setMode(wl_client *, wl_resource *resource, const uint32_t mode)
{
    auto *const binding = static_cast<DpmsBinding *>(wl_resource_get_user_data(resource));
    auto *const fixture = binding->fixture;
    fixture->m_setRequestCount.fetch_add(1);
    fixture->m_lastRequestedMode.store(mode);
    for (wl_resource *const dpms : std::as_const(fixture->m_dpmsResources)) {
        org_kde_kwin_dpms_send_mode(dpms, mode);
        org_kde_kwin_dpms_send_done(dpms);
    }
}

void DpmsWaylandFixture::releaseDpms(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void DpmsWaylandFixture::destroyDpmsResource(wl_resource *resource)
{
    auto *const binding = static_cast<DpmsBinding *>(wl_resource_get_user_data(resource));
    binding->fixture->m_dpmsResources.removeOne(resource);
    delete binding;
}

void DpmsWaylandFixture::destroyOutputResource(wl_resource *resource)
{
    auto *const binding = static_cast<OutputBinding *>(wl_resource_get_user_data(resource));
    auto &resources = binding->fixture->m_outputResources[binding->name];
    resources.removeOne(resource);
    if (resources.isEmpty()) binding->fixture->m_outputResources.remove(binding->name);
    delete binding;
}
