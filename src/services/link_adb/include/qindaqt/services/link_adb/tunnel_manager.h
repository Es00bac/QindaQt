// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_adb/adb_runner.h>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QTimer>

namespace QindaQt::Link {

struct AdbDevice {
    QString serial;
    QString state;  // device, offline, unauthorized, ...
};

// Incremental parser for the `adb track-devices` stream.
class TrackDevicesParser {
public:
    // Returns every complete device list received so far (one per update).
    [[nodiscard]] QList<QList<AdbDevice>> append(const QByteArray &bytes);

private:
    QByteArray m_buffer;
};

struct TunnelOptions {
    QString package = QStringLiteral("org.qindaqt.link");
    quint16 port = 47160;
    int restartDelayMs = 2000;   // after the tracker exits
    int recheckMs = 15000;       // retry devices where the app is not installed yet
};

struct TunnelStatus {
    QString serial;
    bool appInstalled = false;
    bool tunneled = false;
};

// Keeps `adb -s <serial> reverse tcp:PORT tcp:PORT` in place for every device
// in state `device` that has the QindaLink app installed.
//
// Ownership: borrows the runner. Threading: one thread. Error behavior: adb
// failures are retried on the next device update or recheck; nothing throws.
// AGENT-GUARD: only `pm path <package>` and `reverse` are ever issued; other
// devices' packages and settings are never touched.
class TunnelManager final : public QObject {
    Q_OBJECT
public:
    TunnelManager(AdbRunner &runner, TunnelOptions options = {}, QObject *parent = nullptr);
    void start();
    void stop();
    [[nodiscard]] QList<TunnelStatus> devices() const;

Q_SIGNALS:
    void devicesChanged();

private:
    void onSnapshot(const QList<AdbDevice> &devices);
    void probe(const QString &serial);
    void recheck();

    AdbRunner &m_runner;
    TunnelOptions m_options;
    TrackDevicesParser m_parser;
    QHash<QString, TunnelStatus> m_devices;
    QHash<QString, bool> m_busy;
    QTimer m_restart;
    QTimer m_recheck;
    bool m_running = false;
};

} // namespace QindaQt::Link
