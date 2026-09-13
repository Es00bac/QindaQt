// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/display_writer/production_output_management_port.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include "support/display_writer_test_support.h"
#include "wayland-kde-output-device-v2-server-protocol.h"
#include "wayland-kde-output-management-v2-server-protocol.h"

#include <QtCore/QSocketNotifier>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <wayland-server-core.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace QindaQt::DisplayWriter;
using QindaQt::DisplayService::BrightnessApplyOutcome;
using QindaQt::DisplayService::BrightnessApplyRequest;
using QindaQt::DisplayService::BrightnessSubmitStatus;
using QindaQt::DisplayService::DeviceBrightness;
using QindaQt::DisplayService::DeviceBrightnessFrame;

namespace {

inline constexpr quint32 kCapable = KDE_OUTPUT_DEVICE_V2_CAPABILITY_BRIGHTNESS;

struct FakeDevice {
    QByteArray name;
    QByteArray uuid;
    quint32 version = 20;
    quint32 capabilities = 0;
    quint32 brightness = 0;
    bool enabled = true;
    std::vector<wl_resource *> resources;
};

struct SetBrightnessCall {
    QByteArray device;
    quint32 value = 0;

    friend bool operator==(const SetBrightnessCall &, const SetBrightnessCall &) = default;
};

// A private libwayland-server peer for the KDE requests this writer may send.
// It records every configuration request and, like KWin 6.6.6, republishes
// the device state before acknowledging an applied configuration.
class BrightnessOutputServer final : public QObject {
public:
    ~BrightnessOutputServer() override
    {
        m_notifier.reset();
        if (m_display != nullptr) {
            wl_display_destroy_clients(m_display);
            wl_display_destroy(m_display);
        }
    }

    bool start(const QByteArray &socketPath)
    {
        m_display = wl_display_create();
        if (m_display == nullptr || wl_display_add_socket(m_display, socketPath.constData()) != 0
            || wl_global_create(m_display, &kde_output_management_v2_interface, 19, this,
                                &bindManagement) == nullptr) {
            return false;
        }
        auto *loop = wl_display_get_event_loop(m_display);
        m_notifier = std::make_unique<QSocketNotifier>(wl_event_loop_get_fd(loop),
                                                       QSocketNotifier::Read, this);
        connect(m_notifier.get(), &QSocketNotifier::activated, this, [this, loop] {
            if (wl_event_loop_dispatch(loop, 0) < 0) {
                dispatchFailed = true;
            }
            wl_display_flush_clients(m_display);
        });
        return true;
    }

    bool addDevice(FakeDevice device)
    {
        m_devices.push_back(std::make_unique<FakeDevice>(std::move(device)));
        FakeDevice *added = m_devices.back().get();
        const wl_global *global = wl_global_create(m_display, &kde_output_device_v2_interface,
                                                   static_cast<int>(added->version), added,
                                                   &bindDevice);
        wl_display_flush_clients(m_display);
        return global != nullptr;
    }

    QList<SetBrightnessCall> setBrightnessCalls;
    int configurations = 0;
    int applies = 0;
    bool failApply = false;
    bool holdApply = false;
    bool dispatchFailed = false;

private:
    struct Configuration {
        BrightnessOutputServer *server = nullptr;
        FakeDevice *target = nullptr;
        quint32 value = 0;
    };

    static void bindManagement(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *resource = wl_resource_create(client, &kde_output_management_v2_interface,
                                            static_cast<int>(version), id);
        // The protocol variable hides the request-table struct of the same
        // name, so the struct needs its elaborated type specifier.
        static const struct kde_output_management_v2_interface implementation = [] {
            struct kde_output_management_v2_interface value{};
            value.create_configuration = &createConfiguration;
            return value;
        }();
        wl_resource_set_implementation(resource, &implementation, data, nullptr);
    }

    static void createConfiguration(wl_client *client, wl_resource *manager, uint32_t id)
    {
        auto *server = static_cast<BrightnessOutputServer *>(wl_resource_get_user_data(manager));
        ++server->configurations;
        auto *resource = wl_resource_create(client, &kde_output_configuration_v2_interface,
                                            wl_resource_get_version(manager), id);
        static const struct kde_output_configuration_v2_interface implementation = [] {
            struct kde_output_configuration_v2_interface value{};
            value.set_brightness = &setBrightness;
            value.apply = &apply;
            value.destroy = [](wl_client *, wl_resource *configuration) {
                wl_resource_destroy(configuration);
            };
            return value;
        }();
        wl_resource_set_implementation(
            resource, &implementation, new Configuration{.server = server},
            [](wl_resource *configuration) {
                delete static_cast<Configuration *>(wl_resource_get_user_data(configuration));
            });
    }

    static void setBrightness(wl_client *, wl_resource *resource, wl_resource *outputdevice,
                              uint32_t brightness)
    {
        auto *configuration = static_cast<Configuration *>(wl_resource_get_user_data(resource));
        auto *device = static_cast<FakeDevice *>(wl_resource_get_user_data(outputdevice));
        configuration->server->setBrightnessCalls.push_back({device->name, brightness});
        configuration->target = device;
        configuration->value = brightness;
    }

    static void apply(wl_client *, wl_resource *resource)
    {
        auto *configuration = static_cast<Configuration *>(wl_resource_get_user_data(resource));
        BrightnessOutputServer *server = configuration->server;
        ++server->applies;
        if (server->holdApply) {
            return;
        }
        if (server->failApply) {
            kde_output_configuration_v2_send_failed(resource);
            return;
        }
        if (configuration->target != nullptr) {
            configuration->target->brightness = configuration->value;
            for (wl_resource *device : configuration->target->resources) {
                kde_output_device_v2_send_brightness(device, configuration->value);
                kde_output_device_v2_send_done(device);
            }
        }
        kde_output_configuration_v2_send_applied(resource);
    }

    static void bindDevice(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *device = static_cast<FakeDevice *>(data);
        auto *resource = wl_resource_create(client, &kde_output_device_v2_interface,
                                            static_cast<int>(version), id);
        wl_resource_set_implementation(resource, nullptr, device, [](wl_resource *bound) {
            std::erase(static_cast<FakeDevice *>(wl_resource_get_user_data(bound))->resources,
                       bound);
        });
        device->resources.push_back(resource);
        kde_output_device_v2_send_name(resource, device->name.constData());
        kde_output_device_v2_send_uuid(resource, device->uuid.constData());
        kde_output_device_v2_send_enabled(resource, device->enabled ? 1 : 0);
        kde_output_device_v2_send_capabilities(resource, device->capabilities);
        if (version >= KDE_OUTPUT_DEVICE_V2_BRIGHTNESS_SINCE_VERSION) {
            kde_output_device_v2_send_brightness(resource, device->brightness);
        }
        kde_output_device_v2_send_done(resource);
    }

    wl_display *m_display = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
    std::vector<std::unique_ptr<FakeDevice>> m_devices;
};

class ScopedWaylandDisplay final {
public:
    explicit ScopedWaylandDisplay(const QByteArray &path)
        : m_present(qEnvironmentVariableIsSet("WAYLAND_DISPLAY")), m_prior(qgetenv("WAYLAND_DISPLAY")),
          m_socketPresent(qEnvironmentVariableIsSet("WAYLAND_SOCKET")), m_socket(qgetenv("WAYLAND_SOCKET"))
    {
        qunsetenv("WAYLAND_SOCKET");
        qputenv("WAYLAND_DISPLAY", path);
    }
    ~ScopedWaylandDisplay()
    {
        if (m_present) qputenv("WAYLAND_DISPLAY", m_prior);
        else qunsetenv("WAYLAND_DISPLAY");
        if (m_socketPresent) qputenv("WAYLAND_SOCKET", m_socket);
        else qunsetenv("WAYLAND_SOCKET");
    }
private:
    bool m_present;
    QByteArray m_prior;
    bool m_socketPresent;
    QByteArray m_socket;
};

BrightnessApplyRequest request(const quint64 requestId, const quint64 generation,
                               const QString &connector, const QString &uuid, const quint32 value)
{
    return {.requestId = requestId,
            .ownerGeneration = generation,
            .connectorName = connector,
            .runtimeUuid = uuid,
            .value = value};
}

} // namespace

class DisplayWriterWaylandBrightnessTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void publishesDeviceFactsAndSubmitsExactSetBrightness()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray socket = directory.filePath(QStringLiteral("brightness-socket")).toUtf8();
        BrightnessOutputServer server;
        QVERIFY(server.start(socket));
        QVERIFY(server.addDevice({"DP-1", "uuid-dp", 20, kCapable, 6'000, true, {}}));
        ScopedWaylandDisplay environment(socket);
        WriterTransactionPort writer(makeProductionOutputManagementPort(),
            std::make_unique<TestSupport::FakeJournalStore>());
        TestSupport::RecordingObserver observer;
        writer.setObserver(&observer);
        QCOMPARE(writer.start(), PortStartStatus::Started);
        QTRY_VERIFY_WITH_TIMEOUT(!observer.frames.isEmpty()
                                     && observer.frames.constLast().ownerGeneration != 0,
                                 1500);
        const DeviceBrightnessFrame published = observer.frames.constLast();
        const QList<DeviceBrightness> expected{{.connectorName = QStringLiteral("DP-1"),
                                                .runtimeUuid = QStringLiteral("uuid-dp"),
                                                .enabled = true,
                                                .capable = true,
                                                .observed = true,
                                                .value = 6'000}};
        QCOMPARE(published.devices, expected);

        QCOMPARE(writer.requestBrightness(request(41, published.ownerGeneration,
                                                  QStringLiteral("DP-1"),
                                                  QStringLiteral("uuid-dp"), 2'500)),
                 BrightnessSubmitStatus::Accepted);
        // Topology applies cannot overlap the unresolved brightness write.
        writer.beginMachineLineage(3);
        writer.requestApply(TestSupport::completeRequest(12));
        QTRY_COMPARE_WITH_TIMEOUT(observer.brightnessCompletions.size(), 1, 1500);
        QCOMPARE(observer.brightnessCompletions.at(0),
                 (std::pair{quint64{41}, BrightnessApplyOutcome::Applied}));
        QCOMPARE(observer.completions,
                 (QList<TestSupport::Completion>{
                     {3, 12, QindaQt::DisplayTransaction::ApplyOutcome::Rejected}}));
        QCOMPARE(server.setBrightnessCalls, (QList<SetBrightnessCall>{{"DP-1", 2'500}}));
        QCOMPARE(server.configurations, 1);
        QCOMPARE(server.applies, 1);
        QCOMPARE(observer.frames.constLast().ownerGeneration, published.ownerGeneration);
        QCOMPARE(observer.frames.constLast().devices.at(0).value, quint32{2'500});
        QVERIFY(!server.dispatchFailed);

        writer.stop();
        QTRY_COMPARE_WITH_TIMEOUT(observer.frames.constLast().ownerGeneration, quint64{0}, 1500);
        QVERIFY(observer.frames.constLast().devices.isEmpty());
    }

    void refusesVersionCapabilityAndIdentityWithoutProtocolRequests()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray socket = directory.filePath(QStringLiteral("brightness-socket")).toUtf8();
        BrightnessOutputServer server;
        QVERIFY(server.start(socket));
        QVERIFY(server.addDevice({"DP-1", "uuid-1", 20, 0, 5'000, true, {}}));
        QVERIFY(server.addDevice({"DP-2", "uuid-2", 8, kCapable, 4'000, true, {}}));
        QVERIFY(server.addDevice({"DP-3", "uuid-3", 20, kCapable, 3'000, false, {}}));
        QVERIFY(server.addDevice({"DP-4", "uuid-4", 7, kCapable, 2'000, true, {}}));
        QVERIFY(server.addDevice({"DP-5", "uuid-5", 20, kCapable, 12'000, true, {}}));
        QVERIFY(server.addDevice({"DP-6", "uuid-6", 20, kCapable, 1'000, true, {}}));
        ScopedWaylandDisplay environment(socket);
        WriterTransactionPort writer(makeProductionOutputManagementPort(),
            std::make_unique<TestSupport::FakeJournalStore>());
        TestSupport::RecordingObserver observer;
        writer.setObserver(&observer);
        QCOMPARE(writer.start(), PortStartStatus::Started);
        QTRY_VERIFY_WITH_TIMEOUT(!observer.frames.isEmpty()
                                     && observer.frames.constLast().devices.size() == 6,
                                 1500);
        const DeviceBrightnessFrame published = observer.frames.constLast();
        const auto fact = [](const char *name, const char *uuid, bool enabled, bool capable,
                             bool observed, quint32 value) {
            return DeviceBrightness{.connectorName = QString::fromLatin1(name),
                                    .runtimeUuid = QString::fromLatin1(uuid),
                                    .enabled = enabled,
                                    .capable = capable,
                                    .observed = observed,
                                    .value = value};
        };
        const QList<DeviceBrightness> expected{
            fact("DP-1", "uuid-1", true, false, true, 5'000),  // capability not advertised
            fact("DP-2", "uuid-2", true, false, true, 4'000),  // capability bit below v9
            fact("DP-3", "uuid-3", false, true, true, 3'000),  // disabled
            fact("DP-4", "uuid-4", true, false, false, 0),     // no brightness event below v8
            fact("DP-5", "uuid-5", true, true, false, 0),      // value outside the scale
            fact("DP-6", "uuid-6", true, true, true, 1'000)};
        QCOMPARE(published.devices, expected);

        const quint64 generation = published.ownerGeneration;
        struct Refusal {
            BrightnessApplyRequest request;
            BrightnessSubmitStatus status;
        };
        const QList<Refusal> refusals{
            {request(1, generation, QStringLiteral("DP-1"), QStringLiteral("uuid-1"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(2, generation, QStringLiteral("DP-2"), QStringLiteral("uuid-2"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(3, generation, QStringLiteral("DP-3"), QStringLiteral("uuid-3"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(4, generation, QStringLiteral("DP-4"), QStringLiteral("uuid-4"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(5, generation, QStringLiteral("DP-6"), QStringLiteral("uuid-stale"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(6, generation, QStringLiteral("DP-9"), QStringLiteral("uuid-6"), 900),
             BrightnessSubmitStatus::Unsupported},
            {request(7, generation, QStringLiteral("DP-6"), QStringLiteral("uuid-6"), 10'001),
             BrightnessSubmitStatus::Malformed},
            {request(8, generation + 1, QStringLiteral("DP-6"), QStringLiteral("uuid-6"), 900),
             BrightnessSubmitStatus::Unavailable},
        };
        for (const Refusal &refusal : refusals) {
            QCOMPARE(writer.requestBrightness(refusal.request), refusal.status);
        }
        QCOMPARE(writer.requestBrightness(
                     request(9, generation, QStringLiteral("DP-6"), QStringLiteral("uuid-6"), 900)),
                 BrightnessSubmitStatus::Accepted);
        QTRY_COMPARE_WITH_TIMEOUT(observer.brightnessCompletions.size(), 1, 1500);
        QCOMPARE(observer.brightnessCompletions.at(0),
                 (std::pair{quint64{9}, BrightnessApplyOutcome::Applied}));
        QCOMPARE(server.setBrightnessCalls, (QList<SetBrightnessCall>{{"DP-6", 900}}));
        QCOMPARE(server.configurations, 1);
        QVERIFY(!server.dispatchFailed);
    }

    void typesCompositorFailureAndOwnerChange()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray socket = directory.filePath(QStringLiteral("brightness-socket")).toUtf8();
        BrightnessOutputServer server;
        QVERIFY(server.start(socket));
        QVERIFY(server.addDevice({"DP-1", "uuid-dp", 20, kCapable, 6'000, true, {}}));
        ScopedWaylandDisplay environment(socket);
        WriterTransactionPort writer(makeProductionOutputManagementPort(),
            std::make_unique<TestSupport::FakeJournalStore>());
        TestSupport::RecordingObserver observer;
        writer.setObserver(&observer);
        QCOMPARE(writer.start(), PortStartStatus::Started);
        QTRY_VERIFY_WITH_TIMEOUT(!observer.frames.isEmpty()
                                     && observer.frames.constLast().ownerGeneration != 0,
                                 1500);
        const quint64 generation = observer.frames.constLast().ownerGeneration;

        server.failApply = true;
        QCOMPARE(writer.requestBrightness(request(7, generation, QStringLiteral("DP-1"),
                                                  QStringLiteral("uuid-dp"), 2'000)),
                 BrightnessSubmitStatus::Accepted);
        QTRY_COMPARE_WITH_TIMEOUT(observer.brightnessCompletions.size(), 1, 1500);
        QCOMPARE(observer.brightnessCompletions.at(0),
                 (std::pair{quint64{7}, BrightnessApplyOutcome::Rejected}));

        server.failApply = false;
        server.holdApply = true;
        QCOMPARE(writer.requestBrightness(request(8, generation, QStringLiteral("DP-1"),
                                                  QStringLiteral("uuid-dp"), 3'000)),
                 BrightnessSubmitStatus::Accepted);
        QTRY_COMPARE_WITH_TIMEOUT(server.applies, 2, 1500);
        const qsizetype framesBeforeHotplug = observer.frames.size();
        QVERIFY(server.addDevice({"DP-2", "uuid-dp2", 20, kCapable, 6'000, true, {}}));
        QTRY_COMPARE_WITH_TIMEOUT(observer.brightnessCompletions.size(), 2, 1500);
        QCOMPARE(observer.brightnessCompletions.at(1),
                 (std::pair{quint64{8}, BrightnessApplyOutcome::TransportUncertain}));
        QTRY_VERIFY_WITH_TIMEOUT(observer.frames.constLast().devices.size() == 2, 1500);
        QVERIFY(observer.frames.constLast().ownerGeneration > generation);
        const auto cleared = std::ranges::any_of(
            observer.frames.cbegin() + framesBeforeHotplug, observer.frames.cend(),
            [](const DeviceBrightnessFrame &frame) { return frame.ownerGeneration == 0; });
        QVERIFY(cleared);
        QCOMPARE(writer.requestBrightness(request(9, generation, QStringLiteral("DP-1"),
                                                  QStringLiteral("uuid-dp"), 3'000)),
                 BrightnessSubmitStatus::Unavailable);
        QVERIFY(!server.dispatchFailed);
    }
};

QTEST_GUILESS_MAIN(DisplayWriterWaylandBrightnessTests)
#include "tst_display_writer_wayland_brightness.moc"
