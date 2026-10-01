// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dpms-server-protocol.h"

#include <QHash>
#include <QSocketNotifier>
#include <QThread>
#include <QVector>
#include <wayland-server.h>

#include <atomic>
#include <functional>
#include <memory>

class DpmsWaylandFixture final {
public:
    DpmsWaylandFixture();
    ~DpmsWaylandFixture();
    DpmsWaylandFixture(const DpmsWaylandFixture &) = delete;
    DpmsWaylandFixture &operator=(const DpmsWaylandFixture &) = delete;

    int openClientFd();
    quint32 addOutput();
    void removeOutput(quint32 name);
    // Hold actual socket reads to expose flush-versus-peer-close scheduling.
    void pauseDispatch(int milliseconds);
    void disconnectClients();
    int setRequestCount() const { return m_setRequestCount.load(); }
    quint32 lastRequestedMode() const { return m_lastRequestedMode.load(); }

    static void bindOutput(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void getDpms(wl_client *client, wl_resource *resource, uint32_t id,
                        wl_resource *output);
    static void setMode(wl_client *client, wl_resource *resource, uint32_t mode);
    static void releaseDpms(wl_client *client, wl_resource *resource);
    static void destroyDpmsResource(wl_resource *resource);
    static void destroyOutputResource(wl_resource *resource);

private:
    struct OutputBinding { DpmsWaylandFixture *fixture; quint32 name; };
    struct DpmsBinding { DpmsWaylandFixture *fixture; quint32 outputName; };
    void runOnServerThread(std::function<void()> action);
    void dispatch();

    QThread m_serverThread;
    QObject *m_worker = nullptr;
    wl_display *m_display = nullptr;
    wl_event_loop *m_loop = nullptr;
    wl_global *m_managerGlobal = nullptr;
    QHash<quint32, wl_global *> m_outputs;
    QHash<quint32, OutputBinding *> m_outputBindings;
    QHash<quint32, QVector<wl_resource *>> m_outputResources;
    quint32 m_nextOutput = 1;
    QVector<wl_resource *> m_dpmsResources;
    std::atomic<int> m_setRequestCount{0};
    std::atomic<quint32> m_lastRequestedMode{UINT32_MAX};
    std::unique_ptr<QSocketNotifier> m_watcher;
};
