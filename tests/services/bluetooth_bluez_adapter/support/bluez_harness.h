// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fake_bluez.h"
#include "private_bus.h"

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h>
#include <qindaqt/services/bluetooth_model/bluetooth_model.h>

#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Tests
{

// One isolated bus, one fake org.bluez, one backend/model pair per row.
// Nothing here contacts the ambient session bus or the host system bus.
class BluezHarness final
{
public:
    explicit BluezHarness(const quint64 epochSeed)
    {
        m_ready = bus.start();
        if (!m_ready) {
            return;
        }
        fake = std::make_unique<FakeBluez>(bus.address);
        clientName = QStringLiteral("bluez-client-%1")
                         .arg(QUuid::createUuid().toString(QUuid::Id128));
        client = QDBusConnection::connectToBus(bus.address, clientName);
        m_ready = client.isConnected();
        if (!m_ready) {
            return;
        }
        backend = std::make_unique<Bluetooth::BluezAdapterBackend>(client);
        model = std::make_unique<Bluetooth::BluetoothModel>(backend.get(), epochSeed);
    }

    ~BluezHarness()
    {
        model.reset();
        backend.reset();
        if (!clientName.isEmpty()) {
            QDBusConnection::disconnectFromBus(clientName);
        }
    }

    BluezHarness(const BluezHarness &) = delete;
    BluezHarness &operator=(const BluezHarness &) = delete;

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

    [[nodiscard]] bool waitUntil(const std::function<bool()> &predicate,
                                 const int timeout = 5000)
    {
        return QTest::qWaitFor(predicate, timeout);
    }

    [[nodiscard]] bool waitReady()
    {
        return waitUntil(
            [this] { return model->snapshot().availability == Bluetooth::Availability::Ready; });
    }

    [[nodiscard]] bool waitUnavailable()
    {
        return waitUntil([this] {
            return model->snapshot().availability == Bluetooth::Availability::Unavailable;
        });
    }

    [[nodiscard]] std::optional<Bluetooth::OperationResult> awaitResult(
        QSignalSpy &spy, const quint64 operationId, const int timeout = 5000)
    {
        const auto take = [&spy, operationId]() -> std::optional<Bluetooth::OperationResult> {
            return takeResult(spy, operationId);
        };
        const bool arrived = QTest::qWaitFor([&take] { return take().has_value(); }, timeout);
        return arrived ? take() : std::nullopt;
    }

    [[nodiscard]] static std::optional<Bluetooth::OperationResult> takeResult(
        QSignalSpy &spy, const quint64 operationId)
    {
        for (const QList<QVariant> &arguments : spy) {
            if (arguments.value(0).toULongLong() == operationId) {
                return arguments.value(1).value<Bluetooth::OperationResult>();
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] const Bluetooth::Adapter &snapshotAdapter() const
    {
        return model->snapshot().adapters.constFirst();
    }

    PrivateBus bus;
    std::unique_ptr<FakeBluez> fake;
    QString clientName;
    QDBusConnection client{QStringLiteral("invalid")};
    std::unique_ptr<Bluetooth::BluezAdapterBackend> backend;
    std::unique_ptr<Bluetooth::BluetoothModel> model;

private:
    bool m_ready = false;
};

} // namespace QindaQt::Tests
