// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_data_control_server.h"

#include "wayland-ext-data-control-v1-server-protocol.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace QindaQt::Tests {

struct FakeDataControlServer::OfferState {
    FakeDataControlServer *server = nullptr;
    wl_resource *resource = nullptr;
    QHash<QString, QByteArray> payloads;
};

struct FakeDataControlServer::PendingWrite {
    int fd = -1;
    QByteArray payload;
    qsizetype offset = 0;
};

namespace {

void ignoreSeatObject(wl_client *, wl_resource *, uint32_t) {}

const struct wl_seat_interface seatImplementation{
    &ignoreSeatObject,
    &ignoreSeatObject,
    &ignoreSeatObject,
    [](wl_client *, wl_resource *resource) { wl_resource_destroy(resource); }};

const struct ext_data_control_manager_v1_interface managerImplementation{
    &FakeDataControlServer::createDataSource,
    &FakeDataControlServer::getDataDevice,
    &FakeDataControlServer::destroyResource};

const struct ext_data_control_device_v1_interface deviceImplementation{
    &FakeDataControlServer::setSelection,
    &FakeDataControlServer::destroyResource,
    &FakeDataControlServer::setSelection};

const struct ext_data_control_offer_v1_interface offerImplementation{
    &FakeDataControlServer::receiveOffer,
    &FakeDataControlServer::destroyResource};

const struct ext_data_control_source_v1_interface sourceImplementation{
    &FakeDataControlServer::sourceOffer,
    &FakeDataControlServer::destroyResource};

} // namespace

FakeDataControlServer::FakeDataControlServer(QObject *parent) : QObject(parent)
{
    m_dispatchTimer.setInterval(1);
    m_writeTimer.setInterval(1);
    connect(&m_dispatchTimer, &QTimer::timeout, this, &FakeDataControlServer::dispatch);
    connect(&m_writeTimer, &QTimer::timeout, this, &FakeDataControlServer::pumpWrites);
}

FakeDataControlServer::~FakeDataControlServer()
{
    m_dispatchTimer.stop();
    m_writeTimer.stop();
    for (const auto &write : m_writes) {
        if (write->fd >= 0) {
            ::close(write->fd);
        }
    }
    m_writes.clear();
    m_offers.clear();
    if (m_display != nullptr) {
        wl_display_destroy_clients(m_display);
        wl_display_destroy(m_display);
    }
}

bool FakeDataControlServer::start()
{
    m_display = wl_display_create();
    if (m_display == nullptr) {
        return false;
    }
    m_seatGlobal = wl_global_create(m_display, &wl_seat_interface, 1, this, &bindSeat);
    m_managerGlobal = wl_global_create(m_display, &ext_data_control_manager_v1_interface,
                                       1, this, &bindManager);
    if (m_seatGlobal == nullptr || m_managerGlobal == nullptr) {
        return false;
    }
    m_socketName = QStringLiteral("wayland-clipboard-test");
    if (wl_display_add_socket(m_display, m_socketName.toUtf8().constData()) != 0) {
        return false;
    }
    m_dispatchTimer.start();
    return true;
}

void FakeDataControlServer::bindSeat(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_seat_interface,
                                               static_cast<int>(qMin(version, 1U)), id);
    wl_resource_set_implementation(resource, &seatImplementation, nullptr, nullptr);
}

void FakeDataControlServer::bindManager(wl_client *client, void *data,
                                       uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &ext_data_control_manager_v1_interface,
        static_cast<int>(qMin(version, 1U)), id);
    wl_resource_set_implementation(resource, &managerImplementation, data, nullptr);
}

void FakeDataControlServer::createDataSource(wl_client *client, wl_resource *resource,
                                            uint32_t id)
{
    wl_resource *source = wl_resource_create(client, &ext_data_control_source_v1_interface,
                                             wl_resource_get_version(resource), id);
    wl_resource_set_implementation(source, &sourceImplementation,
                                   wl_resource_get_user_data(resource), nullptr);
}

void FakeDataControlServer::getDataDevice(wl_client *client, wl_resource *resource,
                                         uint32_t id, wl_resource *)
{
    auto *server = static_cast<FakeDataControlServer *>(wl_resource_get_user_data(resource));
    server->m_device = wl_resource_create(client, &ext_data_control_device_v1_interface,
                                          wl_resource_get_version(resource), id);
    wl_resource_set_implementation(server->m_device, &deviceImplementation, server, nullptr);
}

void FakeDataControlServer::destroyResource(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void FakeDataControlServer::receiveOffer(wl_client *, wl_resource *resource,
                                        const char *mediaType, int32_t fd)
{
    auto *offer = static_cast<OfferState *>(wl_resource_get_user_data(resource));
    const QString type = QString::fromUtf8(mediaType);
    offer->server->m_receivedTypes.append(type);
    offer->server->queueWrite(fd, offer->payloads.value(type));
}

void FakeDataControlServer::setSelection(wl_client *, wl_resource *, wl_resource *) {}

void FakeDataControlServer::sourceOffer(wl_client *, wl_resource *, const char *) {}

void FakeDataControlServer::sendOffer(const QHash<QString, QByteArray> &payloads, bool primary)
{
    if (m_device == nullptr) {
        return;
    }
    wl_client *client = wl_resource_get_client(m_device);
    auto state = std::make_unique<OfferState>();
    state->server = this;
    state->payloads = payloads;
    state->resource = wl_resource_create(client, &ext_data_control_offer_v1_interface, 1, 0);
    wl_resource_set_implementation(state->resource, &offerImplementation, state.get(), nullptr);
    ext_data_control_device_v1_send_data_offer(m_device, state->resource);
    QStringList types = payloads.keys();
    types.sort();
    for (const QString &type : types) {
        ext_data_control_offer_v1_send_offer(state->resource, type.toUtf8().constData());
    }
    if (primary) {
        ext_data_control_device_v1_send_primary_selection(m_device, state->resource);
    } else {
        ext_data_control_device_v1_send_selection(m_device, state->resource);
    }
    m_offers.push_back(std::move(state));
    wl_display_flush_clients(m_display);
}

void FakeDataControlServer::dispatch()
{
    if (m_display != nullptr) {
        wl_event_loop_dispatch(wl_display_get_event_loop(m_display), 0);
        wl_display_flush_clients(m_display);
    }
}

void FakeDataControlServer::queueWrite(int fd, QByteArray payload)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    m_writes.push_back(std::make_unique<PendingWrite>(
        PendingWrite{fd, std::move(payload), 0}));
    m_writeTimer.start();
}

void FakeDataControlServer::pumpWrites()
{
    for (size_t reverseIndex = m_writes.size(); reverseIndex > 0; --reverseIndex) {
        const size_t index = reverseIndex - 1;
        auto &pending = m_writes[index];
        while (pending->offset < pending->payload.size()) {
            const ssize_t count = ::write(
                pending->fd, pending->payload.constData() + pending->offset,
                static_cast<size_t>(pending->payload.size() - pending->offset));
            if (count > 0) {
                pending->offset += static_cast<qsizetype>(count);
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        if (pending->offset == pending->payload.size()) {
            ::close(pending->fd);
            m_writes.erase(m_writes.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    if (m_writes.empty()) {
        m_writeTimer.stop();
    }
}

} // namespace QindaQt::Tests
