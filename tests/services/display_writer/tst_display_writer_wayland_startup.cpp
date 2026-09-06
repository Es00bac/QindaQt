// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/display_writer/production_output_management_port.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include "support/display_writer_test_support.h"
#include "wayland-kde-output-device-v2-server-protocol.h"
#include "wayland-kde-output-management-v2-server-protocol.h"

#include <QtCore/QSocketNotifier>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <wayland-server-core.h>

#include <memory>

using namespace QindaQt::DisplayWriter;

namespace {

// The peer sends only protocol responses, with no timer/heartbeat capable of
// accidentally waking a client that forgot to flush callback-enqueued binds.
class QuietOutputServer final : public QObject {
public:
    ~QuietOutputServer() override
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
        if (m_display == nullptr || wl_display_add_socket(m_display, socketPath.constData()) != 0) {
            return false;
        }
        if (wl_global_create(m_display, &kde_output_management_v2_interface, 19,
                             this, &bindManagement) == nullptr || !addDevice()) {
            return false;
        }
        auto *loop = wl_display_get_event_loop(m_display);
        m_notifier = std::make_unique<QSocketNotifier>(
            wl_event_loop_get_fd(loop), QSocketNotifier::Read, this);
        connect(m_notifier.get(), &QSocketNotifier::activated, this, [this, loop] {
            const int result = wl_event_loop_dispatch(loop, 0);
            if (result < 0) {
                dispatchFailed = true;
            }
            wl_display_flush_clients(m_display);
        });
        return true;
    }

    bool addDevice()
    {
        auto *global = wl_global_create(m_display, &kde_output_device_v2_interface,
                                        20, this, &bindDevice);
        if (global == nullptr) {
            return false;
        }
        wl_display_flush_clients(m_display);
        return true;
    }

    int managementBinds = 0;
    int deviceBinds = 0;
    int configurationRequests = 0;
    bool dispatchFailed = false;

private:
    static void bindManagement(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *server = static_cast<QuietOutputServer *>(data);
        ++server->managementBinds;
        auto *resource = wl_resource_create(client, &kde_output_management_v2_interface,
                                             static_cast<int>(version), id);
        static const struct kde_output_management_v2_interface implementation{
            [](wl_client *, wl_resource *manager, uint32_t) {
                auto *owner = static_cast<QuietOutputServer *>(wl_resource_get_user_data(manager));
                ++owner->configurationRequests;
                wl_resource_post_error(manager, 0, "startup must not submit a configuration");
            }, nullptr};
        wl_resource_set_implementation(resource, &implementation, data, nullptr);
    }

    static void bindDevice(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *server = static_cast<QuietOutputServer *>(data);
        ++server->deviceBinds;
        auto *resource = wl_resource_create(client, &kde_output_device_v2_interface,
                                             static_cast<int>(version), id);
        const QByteArray name = "WL-" + QByteArray::number(server->deviceBinds);
        const QByteArray uuid = "private-output-" + QByteArray::number(server->deviceBinds);
        kde_output_device_v2_send_name(resource, name.constData());
        kde_output_device_v2_send_uuid(resource, uuid.constData());
        kde_output_device_v2_send_done(resource);
    }

    wl_display *m_display = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
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

} // namespace

class DisplayWriterWaylandStartupTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void discoversAndRebindsWithoutSubmission()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray socket = directory.filePath(QStringLiteral("display-socket")).toUtf8();
        QuietOutputServer server;
        QVERIFY(server.start(socket));
        ScopedWaylandDisplay environment(socket);
        WriterTransactionPort writer(makeProductionOutputManagementPort(),
            std::make_unique<QindaQt::DisplayWriter::TestSupport::FakeJournalStore>());
        QSignalSpy availability(&writer, &WriterTransactionPort::mutationAuthorityChanged);
        QCOMPARE(writer.start(), PortStartStatus::Started);
        QCOMPARE(writer.compositorProcessId(), QCoreApplication::applicationPid());
        QTRY_VERIFY_WITH_TIMEOUT(writer.isOutputManagementAvailable(), 1500);
        QCOMPARE(server.managementBinds, 1);
        QCOMPARE(server.deviceBinds, 1);
        QCOMPARE(server.configurationRequests, 0);
        QCOMPARE(availability.count(), 1);
        QVERIFY(availability.at(0).at(0).toBool());

        QVERIFY(server.addDevice());
        QTRY_COMPARE_WITH_TIMEOUT(server.deviceBinds, 2, 1500);
        QTRY_VERIFY_WITH_TIMEOUT(writer.isOutputManagementAvailable(), 1500);
        QCOMPARE(availability.count(), 3);
        QVERIFY(!availability.at(1).at(0).toBool());
        QVERIFY(availability.at(2).at(0).toBool());
        QCOMPARE(server.configurationRequests, 0);
        QVERIFY(!server.dispatchFailed);
        writer.stop();
        QVERIFY(!writer.isOutputManagementAvailable());
    }
};

QTEST_GUILESS_MAIN(DisplayWriterWaylandStartupTests)
#include "tst_display_writer_wayland_startup.moc"
