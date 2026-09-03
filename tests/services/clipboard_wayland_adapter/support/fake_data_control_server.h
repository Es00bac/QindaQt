// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <memory>
#include <optional>
#include <vector>

struct wl_client;
struct wl_display;
struct wl_global;
struct wl_resource;

namespace QindaQt::Tests {

class FakeDataControlServer final : public QObject {
    Q_OBJECT
public:
    explicit FakeDataControlServer(QObject *parent = nullptr);
    ~FakeDataControlServer() override;
    [[nodiscard]] bool start();
    [[nodiscard]] QString socketName() const { return m_socketName; }
    [[nodiscard]] bool hasDevice() const noexcept { return m_device != nullptr; }
    void sendOffer(const QHash<QString, QByteArray> &payloads, bool primary);
    void sendUnselectedOffer(const QHash<QString, QByteArray> &payloads);
    void removeManagerGlobal();
    void disconnectClient();
    [[nodiscard]] QStringList receivedTypes() const { return m_receivedTypes; }

public:
    struct OfferState;
    struct PendingWrite;
    static void bindSeat(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void getDataDevice(wl_client *client, wl_resource *resource,
                              uint32_t id, wl_resource *seat);
    static void createDataSource(wl_client *client, wl_resource *resource, uint32_t id);
    static void destroyResource(wl_client *client, wl_resource *resource);
    static void receiveOffer(wl_client *client, wl_resource *resource,
                             const char *mediaType, int32_t fd);
    static void setSelection(wl_client *client, wl_resource *resource,
                             wl_resource *source);
    static void sourceOffer(wl_client *client, wl_resource *resource,
                            const char *mediaType);
private:
    void publishOffer(const QHash<QString, QByteArray> &payloads,
                      std::optional<bool> primary);
    void dispatch();
    void queueWrite(int fd, QByteArray payload);
    void pumpWrites();

    wl_display *m_display = nullptr;
    wl_global *m_seatGlobal = nullptr;
    wl_global *m_managerGlobal = nullptr;
    wl_resource *m_device = nullptr;
    QString m_socketName;
    QTimer m_dispatchTimer;
    QTimer m_writeTimer;
    std::vector<std::unique_ptr<OfferState>> m_offers;
    std::vector<std::unique_ptr<PendingWrite>> m_writes;
    QStringList m_receivedTypes;
};

} // namespace QindaQt::Tests
