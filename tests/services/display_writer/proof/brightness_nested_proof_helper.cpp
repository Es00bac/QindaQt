// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0150 private nested-KWin proof helper. It runs only under
// run_brightness_nested_proof.sh against a disposable kwin_wayland --virtual
// socket, never against a host display, and is not a ctest row. Every result
// is one "key=value" line on stdout for the runner to assert.

#include <qindaqt/services/display_writer/production_output_management_port.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include "wayland-kde-output-device-v2-client-protocol.h"
#include "wayland-kde-output-management-v2-client-protocol.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QFile>
#include <QtCore/QThread>

#include <wayland-client.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

using namespace QindaQt;
using DisplayService::BrightnessApplyOutcome;
using DisplayService::BrightnessSubmitStatus;
using DisplayService::DeviceBrightnessFrame;

namespace
{

void emitLine(const QString &text)
{
    std::fprintf(stdout, "%s\n", qPrintable(text));
    std::fflush(stdout);
}

class NullJournal final : public DisplayWriter::JournalStore
{
public:
    DisplayTransaction::JournalMutationOutcome store(const DisplayTransaction::Journal &) override
    {
        return DisplayTransaction::JournalMutationOutcome::Unchanged;
    }
    DisplayTransaction::JournalMutationOutcome clear() override
    {
        return DisplayTransaction::JournalMutationOutcome::Unchanged;
    }
};

class Recorder final : public DisplayService::TransactionPortObserver
{
public:
    void applyCompleted(quint64, quint64, DisplayTransaction::ApplyOutcome) override {}
    void brightnessDevicesObserved(const DeviceBrightnessFrame &frame) override
    {
        frames.push_back(frame);
    }
    void brightnessCompleted(quint64, BrightnessApplyOutcome) override { ++completions; }

    QList<DeviceBrightnessFrame> frames;
    int completions = 0;
};

bool waitUntil(const std::function<bool()> &condition, const int milliseconds)
{
    const QDeadlineTimer deadline(milliseconds);
    while (!condition()) {
        if (deadline.hasExpired()) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return true;
}

QString statusName(const BrightnessSubmitStatus status)
{
    switch (status) {
    case BrightnessSubmitStatus::Accepted: return QStringLiteral("accepted");
    case BrightnessSubmitStatus::Unavailable: return QStringLiteral("unavailable");
    case BrightnessSubmitStatus::Busy: return QStringLiteral("busy");
    case BrightnessSubmitStatus::Unsupported: return QStringLiteral("unsupported");
    case BrightnessSubmitStatus::Malformed: return QStringLiteral("malformed");
    }
    return QStringLiteral("invalid");
}

void printFrame(const QString &prefix, const DeviceBrightnessFrame &frame)
{
    emitLine(QStringLiteral("%1.generation=%2 devices=%3")
                 .arg(prefix)
                 .arg(frame.ownerGeneration)
                 .arg(frame.devices.size()));
    for (const DisplayService::DeviceBrightness &device : frame.devices) {
        emitLine(QStringLiteral("%1.device name=%2 uuid=%3 enabled=%4 capable=%5 observed=%6 value=%7")
                     .arg(prefix, device.connectorName, device.runtimeUuid)
                     .arg(device.enabled ? 1 : 0)
                     .arg(device.capable ? 1 : 0)
                     .arg(device.observed ? 1 : 0)
                     .arg(device.value));
    }
}

struct Writer {
    Writer()
        : port(DisplayWriter::makeProductionOutputManagementPort(),
               std::make_unique<NullJournal>())
    {
        port.setObserver(&recorder);
    }

    [[nodiscard]] bool startAndObserve(const QString &prefix)
    {
        if (port.start() != DisplayWriter::PortStartStatus::Started) {
            emitLine(prefix + QStringLiteral(".start=failed"));
            return false;
        }
        emitLine(QStringLiteral("%1.peer-pid=%2").arg(prefix).arg(port.compositorProcessId()));
        if (!waitUntil([this] {
                return !recorder.frames.isEmpty()
                    && recorder.frames.constLast().ownerGeneration != 0;
            }, 10'000)) {
            emitLine(prefix + QStringLiteral(".frame=timeout"));
            return false;
        }
        // Let every initial device done boundary settle into one frame.
        waitUntil([] { return false; }, 300);
        printFrame(prefix, recorder.frames.constLast());
        return true;
    }

    [[nodiscard]] BrightnessSubmitStatus request(const quint64 id, const quint64 generation,
                                                 const QString &connector, const QString &uuid,
                                                 const quint32 value)
    {
        return port.requestBrightness({.requestId = id,
                                       .ownerGeneration = generation,
                                       .connectorName = connector,
                                       .runtimeUuid = uuid,
                                       .value = value});
    }

    Recorder recorder;
    DisplayWriter::WriterTransactionPort port;
};

int observe()
{
    Writer writer;
    if (!writer.startAndObserve(QStringLiteral("observe"))) {
        return 3;
    }
    const DeviceBrightnessFrame frame = writer.recorder.frames.constLast();
    quint64 id = 1;
    for (const DisplayService::DeviceBrightness &device : frame.devices) {
        const BrightnessSubmitStatus status = writer.request(
            id++, frame.ownerGeneration, device.connectorName, device.runtimeUuid, 2'500);
        emitLine(QStringLiteral("observe.request name=%1 status=%2")
                     .arg(device.connectorName, statusName(status)));
    }
    if (!frame.devices.isEmpty()) {
        const DisplayService::DeviceBrightness &first = frame.devices.constFirst();
        emitLine(QStringLiteral("observe.stale-generation status=%1")
                     .arg(statusName(writer.request(id++, frame.ownerGeneration + 1,
                                                    first.connectorName, first.runtimeUuid,
                                                    2'500))));
    }
    waitUntil([] { return false; }, 300);
    emitLine(QStringLiteral("observe.completions=%1").arg(writer.recorder.completions));
    writer.port.stop();
    return 0;
}

int holdUntilOwnerLoss(const QString &identityPath)
{
    Writer writer;
    if (!writer.startAndObserve(QStringLiteral("hold"))) {
        return 3;
    }
    const DeviceBrightnessFrame frame = writer.recorder.frames.constLast();
    if (frame.devices.isEmpty()) {
        emitLine(QStringLiteral("hold.devices=none"));
        return 4;
    }
    QFile identity(identityPath);
    if (!identity.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emitLine(QStringLiteral("hold.identity=unwritable"));
        return 5;
    }
    const DisplayService::DeviceBrightness &first = frame.devices.constFirst();
    identity.write(QStringLiteral("%1\n%2\n%3\n")
                       .arg(first.connectorName, first.runtimeUuid)
                       .arg(frame.ownerGeneration)
                       .toUtf8());
    identity.close();
    emitLine(QStringLiteral("hold.ready"));
    if (!waitUntil([&writer] {
            return writer.recorder.frames.constLast().ownerGeneration == 0;
        }, 30'000)) {
        emitLine(QStringLiteral("hold.owner-lost=timeout"));
        return 6;
    }
    printFrame(QStringLiteral("hold.after-loss"), writer.recorder.frames.constLast());
    emitLine(QStringLiteral("hold.after-loss.request status=%1")
                 .arg(statusName(writer.request(1, frame.ownerGeneration, first.connectorName,
                                                first.runtimeUuid, 2'500))));
    emitLine(QStringLiteral("hold.after-loss.available=%1")
                 .arg(writer.port.isOutputManagementAvailable() ? 1 : 0));
    return 0;
}

int reuse(const QString &identityPath)
{
    QFile identity(identityPath);
    if (!identity.open(QIODevice::ReadOnly)) {
        emitLine(QStringLiteral("reuse.identity=unreadable"));
        return 5;
    }
    const QStringList prior = QString::fromUtf8(identity.readAll()).split(u'\n');
    if (prior.size() < 3) {
        emitLine(QStringLiteral("reuse.identity=malformed"));
        return 5;
    }
    Writer writer;
    if (!writer.startAndObserve(QStringLiteral("reuse"))) {
        return 3;
    }
    const DeviceBrightnessFrame frame = writer.recorder.frames.constLast();
    const bool uuidPresent = std::ranges::any_of(frame.devices, [&prior](const auto &device) {
        return device.runtimeUuid == prior.at(1);
    });
    emitLine(QStringLiteral("reuse.prior name=%1 uuid=%2 generation=%3")
                 .arg(prior.at(0), prior.at(1), prior.at(2)));
    emitLine(QStringLiteral("reuse.prior-uuid-present=%1").arg(uuidPresent ? 1 : 0));
    emitLine(QStringLiteral("reuse.prior-identity.request status=%1")
                 .arg(statusName(writer.request(1, frame.ownerGeneration, prior.at(0),
                                                prior.at(1), 2'500))));
    writer.port.stop();
    return 0;
}

// Raw protocol client: bypasses the writer's refusal boundary so the runner can
// record what pinned KWin itself does with the requests the writer refuses.
struct RawDevice {
    kde_output_device_v2 *proxy = nullptr;
    quint32 version = 0;
    QString name;
    QString uuid;
    quint32 capabilities = 0;
    std::optional<quint32> brightness;
    int enabled = -1;
    int brightnessEvents = 0;
};

struct RawClient {
    wl_display *display = nullptr;
    kde_output_management_v2 *management = nullptr;
    quint32 managementVersion = 0;
    std::vector<std::unique_ptr<RawDevice>> devices;
};

struct RawOutcome {
    int applied = 0;
    int failed = 0;
    QString reason;
};

int deviceEvent(const void *, void *target, uint32_t, const wl_message *message,
                wl_argument *arguments)
{
    auto *device = static_cast<RawDevice *>(wl_proxy_get_user_data(static_cast<wl_proxy *>(target)));
    const QByteArrayView event(message->name);
    if (event == "name") {
        device->name = QString::fromUtf8(arguments[0].s);
    } else if (event == "uuid") {
        device->uuid = QString::fromUtf8(arguments[0].s);
    } else if (event == "enabled") {
        device->enabled = arguments[0].i;
    } else if (event == "capabilities") {
        device->capabilities = arguments[0].u;
    } else if (event == "brightness") {
        device->brightness = arguments[0].u;
        ++device->brightnessEvents;
    }
    return 0;
}

int configurationEvent(const void *, void *target, uint32_t, const wl_message *message,
                       wl_argument *arguments)
{
    auto *outcome =
        static_cast<RawOutcome *>(wl_proxy_get_user_data(static_cast<wl_proxy *>(target)));
    const QByteArrayView event(message->name);
    if (event == "applied") {
        ++outcome->applied;
    } else if (event == "failed") {
        ++outcome->failed;
    } else if (event == "failure_reason") {
        outcome->reason = QString::fromUtf8(arguments[0].s);
    }
    return 0;
}

void rawGlobal(void *data, wl_registry *registry, const uint32_t name, const char *interface,
               const uint32_t version)
{
    auto *client = static_cast<RawClient *>(data);
    if (qstrcmp(interface, kde_output_management_v2_interface.name) == 0) {
        client->managementVersion = std::min<quint32>(version, 19);
        client->management = static_cast<kde_output_management_v2 *>(wl_registry_bind(
            registry, name, &kde_output_management_v2_interface, client->managementVersion));
    } else if (qstrcmp(interface, kde_output_device_v2_interface.name) == 0) {
        auto device = std::make_unique<RawDevice>();
        device->version = std::min<quint32>(version, 20);
        device->proxy = static_cast<kde_output_device_v2 *>(
            wl_registry_bind(registry, name, &kde_output_device_v2_interface, device->version));
        wl_proxy_add_dispatcher(static_cast<wl_proxy *>(static_cast<void *>(device->proxy)),
                                &deviceEvent, nullptr, device.get());
        client->devices.push_back(std::move(device));
    }
}

void rawGlobalRemoved(void *, wl_registry *, uint32_t) {}

void settle(RawClient &client)
{
    for (int round = 0; round < 6; ++round) {
        wl_display_roundtrip(client.display);
        QThread::msleep(40);
    }
}

RawOutcome applyConfiguration(RawClient &client,
                              const std::function<void(kde_output_configuration_v2 *)> &requests)
{
    RawOutcome outcome;
    kde_output_configuration_v2 *configuration =
        kde_output_management_v2_create_configuration(client.management);
    wl_proxy_add_dispatcher(static_cast<wl_proxy *>(static_cast<void *>(configuration)),
                            &configurationEvent, nullptr, &outcome);
    requests(configuration);
    kde_output_configuration_v2_apply(configuration);
    for (int round = 0; round < 50 && outcome.applied + outcome.failed == 0; ++round) {
        wl_display_roundtrip(client.display);
    }
    settle(client);
    kde_output_configuration_v2_destroy(configuration);
    wl_display_roundtrip(client.display);
    return outcome;
}

QString brightnessText(const std::optional<quint32> &value)
{
    return value ? QString::number(*value) : QStringLiteral("none");
}

void printOutcome(const QString &prefix, const RawDevice &device, const RawOutcome &outcome,
                  const std::optional<quint32> &before, const int eventsBefore)
{
    emitLine(QStringLiteral("%1 name=%2 applied=%3 failed=%4 reason=\"%5\" brightness-before=%6 "
                            "brightness-after=%7 brightness-events=%8 enabled=%9")
                 .arg(prefix, device.name)
                 .arg(outcome.applied)
                 .arg(outcome.failed)
                 .arg(outcome.reason, brightnessText(before), brightnessText(device.brightness))
                 .arg(device.brightnessEvents - eventsBefore)
                 .arg(device.enabled));
}

int raw()
{
    RawClient client;
    client.display = wl_display_connect(nullptr);
    if (client.display == nullptr) {
        emitLine(QStringLiteral("raw.connect=failed"));
        return 2;
    }
    wl_registry *registry = wl_display_get_registry(client.display);
    static const wl_registry_listener listener{&rawGlobal, &rawGlobalRemoved};
    wl_registry_add_listener(registry, &listener, &client);
    settle(client);
    if (client.management == nullptr || client.devices.size() < 2) {
        emitLine(QStringLiteral("raw.globals=insufficient devices=%1").arg(client.devices.size()));
        return 3;
    }
    emitLine(QStringLiteral("raw.management version=%1").arg(client.managementVersion));
    for (const auto &device : client.devices) {
        emitLine(QStringLiteral("raw.device name=%1 uuid=%2 version=%3 capability-brightness=%4 "
                                "brightness=%5 enabled=%6")
                     .arg(device->name, device->uuid)
                     .arg(device->version)
                     .arg((device->capabilities & KDE_OUTPUT_DEVICE_V2_CAPABILITY_BRIGHTNESS) != 0U
                              ? 1
                              : 0)
                     .arg(brightnessText(device->brightness))
                     .arg(device->enabled));
    }
    RawDevice &first = *client.devices.at(0);
    RawDevice &second = *client.devices.at(1);

    std::optional<quint32> before = first.brightness;
    int events = first.brightnessEvents;
    RawOutcome outcome = applyConfiguration(client, [&first](kde_output_configuration_v2 *c) {
        kde_output_configuration_v2_set_brightness(c, first.proxy, 2'500);
    });
    printOutcome(QStringLiteral("raw.non-capable"), first, outcome, before, events);

    before = first.brightness;
    events = first.brightnessEvents;
    outcome = applyConfiguration(client, [&first](kde_output_configuration_v2 *c) {
        kde_output_configuration_v2_set_brightness(c, first.proxy, 20'000);
    });
    printOutcome(QStringLiteral("raw.out-of-scale"), first, outcome, before, events);

    outcome = applyConfiguration(client, [&second](kde_output_configuration_v2 *c) {
        kde_output_configuration_v2_enable(c, second.proxy, 0);
    });
    printOutcome(QStringLiteral("raw.disable"), second, outcome, second.brightness,
                 second.brightnessEvents);

    before = second.brightness;
    events = second.brightnessEvents;
    outcome = applyConfiguration(client, [&second](kde_output_configuration_v2 *c) {
        kde_output_configuration_v2_set_brightness(c, second.proxy, 2'500);
    });
    printOutcome(QStringLiteral("raw.disabled"), second, outcome, before, events);

    wl_display_disconnect(client.display);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    const QString mode = arguments.value(1);
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        || qEnvironmentVariable("WAYLAND_DISPLAY").contains(u'/')) {
        emitLine(QStringLiteral("refused=needs-a-private-socket-name"));
        return 64;
    }
    if (mode == QStringLiteral("observe")) {
        return observe();
    }
    if (mode == QStringLiteral("raw")) {
        return raw();
    }
    if (mode == QStringLiteral("hold") && arguments.size() == 3) {
        return holdUntilOwnerLoss(arguments.at(2));
    }
    if (mode == QStringLiteral("reuse") && arguments.size() == 3) {
        return reuse(arguments.at(2));
    }
    emitLine(QStringLiteral("usage=observe|raw|hold FILE|reuse FILE"));
    return 64;
}
