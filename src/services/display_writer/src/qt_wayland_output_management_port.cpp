// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_writer/production_output_management_port.h>

#include "qt_wayland_output_objects_p.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSocketNotifier>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace QindaQt::DisplayWriter
{
namespace
{

inline constexpr quint32 kRequiredManagementVersion = 13;
inline constexpr quint32 kMaximumManagementVersion = 19;
inline constexpr quint32 kMaximumDeviceVersion = 20;

using Private::DeviceMode;
using Private::Management;
using Private::OutputDevice;

class QtWaylandOutputManagementPort;

class ConfigurationProxy final : public QObject,
                                 public QtWayland::kde_output_configuration_v2
{
public:
    ConfigurationProxy(::kde_output_configuration_v2 *object,
                       QtWaylandOutputManagementPort *owner,
                       quint64 ownerGeneration, quint64 requestId);
    ~ConfigurationProxy() override;

protected:
    void kde_output_configuration_v2_applied() override;
    void kde_output_configuration_v2_failed() override;

private:
    QtWaylandOutputManagementPort *m_owner = nullptr;
    quint64 m_ownerGeneration = 0;
    quint64 m_requestId = 0;
};

class QtWaylandOutputManagementPort final : public QObject,
                                            public OutputManagementPort
{
public:
    void setObserver(OutputManagementObserver *observer) override
    {
        m_observer = observer;
    }

    [[nodiscard]] PortStartStatus start() override
    {
        if (m_running) {
            return PortStartStatus::AlreadyStarted;
        }
        m_display = wl_display_connect(nullptr);
        if (m_display == nullptr) {
            return PortStartStatus::ConnectionUnavailable;
        }
        m_registry = wl_display_get_registry(m_display);
        if (m_registry == nullptr) {
            wl_display_disconnect(m_display);
            m_display = nullptr;
            return PortStartStatus::ConnectionUnavailable;
        }
        static const wl_registry_listener listener{&globalAdded, &globalRemoved};
        if (wl_registry_add_listener(m_registry, &listener, this) < 0) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
            wl_display_disconnect(m_display);
            m_display = nullptr;
            return PortStartStatus::ConnectionUnavailable;
        }
        const int displayFd = wl_display_get_fd(m_display);
        if (displayFd < 0) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
            wl_display_disconnect(m_display);
            m_display = nullptr;
            return PortStartStatus::ConnectionUnavailable;
        }
        m_readNotifier = std::make_unique<QSocketNotifier>(
            displayFd, QSocketNotifier::Read, this);
        m_writeNotifier = std::make_unique<QSocketNotifier>(
            displayFd, QSocketNotifier::Write, this);
        m_writeNotifier->setEnabled(false);
        QObject::connect(m_readNotifier.get(), &QSocketNotifier::activated, this,
                         [this] { dispatchReadable(); });
        QObject::connect(m_writeNotifier.get(), &QSocketNotifier::activated, this,
                         [this] { flush(); });
        m_running = true;
        flush();
        return PortStartStatus::Started;
    }

    void stop() override
    {
        if (!m_running) {
            return;
        }
        m_running = false;
        m_available = false;
        m_readNotifier.reset();
        m_writeNotifier.reset();
        delete std::exchange(m_pending, nullptr);
        // AGENT-GUARD: deleteLater() is callback-safe during Wayland dispatch,
        // but the wrappers must die while their wl_display is still alive.
        // Otherwise their destructor would send destroy on a disconnected
        // transport when the Qt deferred-delete event is eventually drained.
        for (const QPointer<ConfigurationProxy> &retired : m_retired) {
            delete retired.data();
        }
        m_retired.clear();
        m_devices.clear();
        m_management.reset();
        m_managementGlobal = 0;
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        if (m_display != nullptr) {
            wl_display_disconnect(m_display);
        }
        m_display = nullptr;
        m_observer = nullptr;
    }

    [[nodiscard]] SubmitStatus submit(
        const Configuration &configuration) override
    {
        if (!m_running || !m_available || m_management == nullptr) {
            return SubmitStatus::Unavailable;
        }
        if (m_pending != nullptr) {
            return SubmitStatus::Busy;
        }
        if (!validateConfiguration(configuration)) {
            return SubmitStatus::Malformed;
        }
        if (kde_output_management_v2_get_version(m_management->object())
            < kRequiredManagementVersion) {
            return SubmitStatus::Unsupported;
        }

        QHash<QString, OutputDevice *> byName;
        for (const auto &device : m_devices) {
            if (!device->ready() || byName.contains(device->connectorName())) {
                return SubmitStatus::Malformed;
            }
            byName.insert(device->connectorName(), device.get());
        }
        if (configuration.scope == ConfigurationScope::CompleteTopology
            && byName.size() != configuration.outputs.size()) {
            return SubmitStatus::Unsupported;
        }

        for (const OutputChange &output : configuration.outputs) {
            OutputDevice *device = byName.value(output.connectorName);
            if (device == nullptr || (output.enabled && device->mode(output.mode) == nullptr)) {
                return SubmitStatus::Unsupported;
            }
            if (!output.replicationSourceConnector.isEmpty()
                && byName.value(output.replicationSourceConnector) == nullptr) {
                return SubmitStatus::Unsupported;
            }
            if (configuration.scope == ConfigurationScope::SurvivingProperties
                && !device->enabled()) {
                return SubmitStatus::Unsupported;
            }
        }

        auto *raw = m_management->create_configuration();
        if (raw == nullptr) {
            return SubmitStatus::Unavailable;
        }
        auto *proxy = new ConfigurationProxy(raw, this, m_ownerGeneration,
                                             configuration.requestId);
        m_pending = proxy;

        OutputDevice *primary = nullptr;
        for (const OutputChange &output : configuration.outputs) {
            OutputDevice *device = byName.value(output.connectorName);
            DeviceMode *mode = device->mode(output.mode);
            if (configuration.scope == ConfigurationScope::CompleteTopology) {
                proxy->enable(device->object(), output.enabled ? 1 : 0);
                if (!output.enabled) {
                    continue;
                }
                proxy->position(device->object(), output.position.x(),
                                output.position.y());
                proxy->set_priority(device->object(), output.priority);
                const QString sourceUuid = output.replicationSourceConnector.isEmpty()
                    ? QString{}
                    : byName.value(output.replicationSourceConnector)->uuid();
                proxy->set_replication_source(device->object(), sourceUuid);
                if (output.primary) {
                    primary = device;
                }
            }
            proxy->mode(device->object(), mode->object());
            proxy->scale(device->object(), wl_fixed_from_double(output.scale));
            proxy->transform(device->object(),
                             static_cast<int32_t>(output.transform));
        }
        if (configuration.scope == ConfigurationScope::CompleteTopology) {
            if (primary == nullptr) {
                finishConfiguration(m_ownerGeneration, configuration.requestId,
                                    CompletionOutcome::Malformed, proxy);
                return SubmitStatus::Accepted;
            }
            proxy->set_primary_output(primary->object());
        }
        proxy->apply();
        flush();
        return SubmitStatus::Accepted;
    }

    void finishConfiguration(const quint64 ownerGeneration,
                             const quint64 requestId,
                             const CompletionOutcome outcome,
                             ConfigurationProxy *proxy)
    {
        if (proxy != m_pending) {
            return;
        }
        m_pending = nullptr;
        retireProxy(proxy);
        if (m_running && m_observer != nullptr) {
            m_observer->outputManagementCompleted(ownerGeneration, requestId,
                                                  outcome);
        }
    }

private:
    static void globalAdded(void *data, wl_registry *registry, const uint32_t name,
                            const char *interface, const uint32_t version)
    {
        auto *self = static_cast<QtWaylandOutputManagementPort *>(data);
        if (!self->m_running) {
            return;
        }
        if (qstrcmp(interface, kde_output_management_v2_interface.name) == 0) {
            if (self->m_management != nullptr) {
                self->advanceOwner(false);
                return;
            }
            self->m_management = std::make_unique<Management>();
            self->m_managementGlobal = name;
            self->m_management->init(registry, name,
                                     static_cast<int>(std::min(
                                         version, kMaximumManagementVersion)));
            self->advanceOwner(false);
            self->publishAvailability();
            return;
        }
        if (qstrcmp(interface, kde_output_device_v2_interface.name) == 0) {
            auto device = std::make_unique<OutputDevice>(name);
            device->doneCallback = [self] { self->publishAvailability(); };
            device->init(registry, name,
                         static_cast<int>(std::min(version,
                                                   kMaximumDeviceVersion)));
            self->m_devices.push_back(std::move(device));
            self->advanceOwner(false);
            self->publishAvailability();
        }
    }

    static void globalRemoved(void *data, wl_registry *, const uint32_t name)
    {
        auto *self = static_cast<QtWaylandOutputManagementPort *>(data);
        if (!self->m_running) {
            return;
        }
        if (name == self->m_managementGlobal) {
            self->m_management.reset();
            self->m_managementGlobal = 0;
            self->advanceOwner(false);
            self->publishAvailability();
            return;
        }
        const auto found = std::find_if(
            self->m_devices.begin(), self->m_devices.end(),
            [name](const auto &device) { return device->globalName() == name; });
        if (found != self->m_devices.end()) {
            self->m_devices.erase(found);
            self->advanceOwner(false);
            self->publishAvailability();
        }
    }

    void advanceOwner(const bool available)
    {
        abandonPending();
        m_ownerGeneration = m_ownerGeneration == std::numeric_limits<quint64>::max()
            ? 1
            : m_ownerGeneration + 1;
        m_available = available;
        if (m_observer != nullptr) {
            m_observer->outputManagementOwnerChanged(m_ownerGeneration, available);
        }
    }

    void dispatchReadable()
    {
        if (!m_running || m_display == nullptr) {
            return;
        }
        if (wl_display_dispatch(m_display) < 0) {
            transportLost();
        }
    }

    void flush()
    {
        if (!m_running || m_display == nullptr) {
            return;
        }
        if (wl_display_flush(m_display) >= 0) {
            m_writeNotifier->setEnabled(false);
            return;
        }
        if (errno == EAGAIN) {
            m_writeNotifier->setEnabled(true);
            return;
        }
        transportLost();
    }

    void transportLost()
    {
        if (!m_running) {
            return;
        }
        abandonPending();
        m_available = false;
        m_ownerGeneration = m_ownerGeneration == std::numeric_limits<quint64>::max()
            ? 1
            : m_ownerGeneration + 1;
        if (m_observer != nullptr) {
            m_observer->outputManagementOwnerChanged(m_ownerGeneration, false);
        }
        m_readNotifier->setEnabled(false);
        m_writeNotifier->setEnabled(false);
    }

    void abandonPending()
    {
        // AGENT-GUARD: A protocol configuration belongs to one exact global
        // set. Hotplug, registry replacement, or transport loss must release
        // the concrete busy slot before a later owner can accept work; its
        // queued destruction also fences any already-buffered late reply.
        if (ConfigurationProxy *pending = std::exchange(m_pending, nullptr)) {
            retireProxy(pending);
        }
    }

    void retireProxy(ConfigurationProxy *proxy)
    {
        std::erase_if(m_retired,
                      [](const QPointer<ConfigurationProxy> &retired) {
                          return retired.isNull();
                      });
        m_retired.emplace_back(proxy);
        proxy->deleteLater();
    }

    void publishAvailability()
    {
        bool available = m_running && m_management != nullptr && !m_devices.empty()
            && kde_output_management_v2_get_version(m_management->object())
                >= kRequiredManagementVersion;
        QHash<QString, QString> uuidsByName;
        for (const auto &device : m_devices) {
            if (!device->ready() || uuidsByName.contains(device->connectorName())
                || uuidsByName.values().contains(device->uuid())) {
                available = false;
                break;
            }
            uuidsByName.insert(device->connectorName(), device->uuid());
        }
        if (available != m_available) {
            if (!available) {
                abandonPending();
            }
            m_available = available;
            if (m_observer != nullptr) {
                m_observer->outputManagementOwnerChanged(m_ownerGeneration,
                                                        available);
            }
        }
    }

    OutputManagementObserver *m_observer = nullptr;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    std::unique_ptr<Management> m_management;
    std::vector<std::unique_ptr<OutputDevice>> m_devices;
    std::unique_ptr<QSocketNotifier> m_readNotifier;
    std::unique_ptr<QSocketNotifier> m_writeNotifier;
    ConfigurationProxy *m_pending = nullptr;
    std::vector<QPointer<ConfigurationProxy>> m_retired;
    quint32 m_managementGlobal = 0;
    quint64 m_ownerGeneration = 0;
    bool m_running = false;
    bool m_available = false;
};

ConfigurationProxy::ConfigurationProxy(::kde_output_configuration_v2 *object,
                                       QtWaylandOutputManagementPort *owner,
                                       const quint64 ownerGeneration,
                                       const quint64 requestId)
    : QObject(owner)
    , QtWayland::kde_output_configuration_v2(object)
    , m_owner(owner)
    , m_ownerGeneration(ownerGeneration)
    , m_requestId(requestId)
{
}

ConfigurationProxy::~ConfigurationProxy()
{
    if (object() != nullptr) {
        destroy();
    }
}

void ConfigurationProxy::kde_output_configuration_v2_applied()
{
    m_owner->finishConfiguration(m_ownerGeneration, m_requestId,
                                 CompletionOutcome::Applied, this);
}

void ConfigurationProxy::kde_output_configuration_v2_failed()
{
    m_owner->finishConfiguration(m_ownerGeneration, m_requestId,
                                 CompletionOutcome::Rejected, this);
}

} // namespace

std::unique_ptr<OutputManagementPort> makeProductionOutputManagementPort()
{
    return std::make_unique<QtWaylandOutputManagementPort>();
}

} // namespace QindaQt::DisplayWriter
