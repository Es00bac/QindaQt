// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_adb/tunnel_manager.h>

namespace QindaQt::Link {

QList<QList<AdbDevice>> TrackDevicesParser::append(const QByteArray &bytes)
{
    m_buffer.append(bytes);
    QList<QList<AdbDevice>> out;
    for (;;) {
        if (m_buffer.size() < 4) {
            break;
        }
        bool ok = false;
        const int length = m_buffer.left(4).toInt(&ok, 16);
        if (!ok || length < 0) {
            m_buffer.clear();  // garbage: resynchronise on the next update
            break;
        }
        if (m_buffer.size() < 4 + length) {
            break;
        }
        const QByteArray payload = m_buffer.mid(4, length);
        m_buffer.remove(0, 4 + length);
        QList<AdbDevice> list;
        for (const QByteArray &line : payload.split('\n')) {
            const QList<QByteArray> parts = line.split('\t');
            if (parts.size() >= 2 && !parts[0].isEmpty()) {
                list.append({QString::fromLatin1(parts[0]), QString::fromLatin1(parts[1]).trimmed()});
            }
        }
        out.append(list);
    }
    return out;
}

TunnelManager::TunnelManager(AdbRunner &runner, TunnelOptions options, QObject *parent)
    : QObject(parent), m_runner(runner), m_options(std::move(options))
{
    m_restart.setSingleShot(true);
    m_restart.setInterval(m_options.restartDelayMs);
    m_recheck.setInterval(m_options.recheckMs);
    connect(&m_restart, &QTimer::timeout, this, [this] {
        if (m_running) {
            m_runner.startTracking();
        }
    });
    connect(&m_recheck, &QTimer::timeout, this, &TunnelManager::recheck);
    connect(&m_runner, &AdbRunner::trackData, this, [this](const QByteArray &bytes) {
        for (const QList<AdbDevice> &list : m_parser.append(bytes)) {
            onSnapshot(list);
        }
    });
    connect(&m_runner, &AdbRunner::trackEnded, this, [this] {
        // The adb server may have restarted: forget all state so tunnels are
        // re-applied from the next device list.
        m_parser = {};
        if (!m_devices.isEmpty()) {
            m_devices.clear();
            Q_EMIT devicesChanged();
        }
        if (m_running) {
            m_restart.start();
        }
    });
}

void TunnelManager::start()
{
    m_running = true;
    m_recheck.start();
    m_runner.startTracking();
}

void TunnelManager::stop()
{
    m_running = false;
    m_restart.stop();
    m_recheck.stop();
    m_runner.stopTracking();
}

QList<TunnelStatus> TunnelManager::devices() const { return m_devices.values(); }

void TunnelManager::onSnapshot(const QList<AdbDevice> &devices)
{
    QSet<QString> present;
    for (const AdbDevice &d : devices) {
        if (d.state == QLatin1String("device")) {
            present.insert(d.serial);
        }
    }
    bool changed = false;
    for (auto it = m_devices.begin(); it != m_devices.end();) {
        if (!present.contains(it.key())) {
            it = m_devices.erase(it);  // gone: re-apply when it returns
            changed = true;
        } else {
            ++it;
        }
    }
    for (const QString &serial : present) {
        if (!m_devices.contains(serial)) {
            m_devices.insert(serial, TunnelStatus{serial, false, false});
            changed = true;
            probe(serial);
        }
    }
    if (changed) {
        Q_EMIT devicesChanged();
    }
}

void TunnelManager::probe(const QString &serial)
{
    if (m_busy.value(serial)) {
        return;
    }
    m_busy.insert(serial, true);
    m_runner.run({QStringLiteral("-s"), serial, QStringLiteral("shell"), QStringLiteral("pm"),
                  QStringLiteral("path"), m_options.package},
                 [this, serial](int code, const QByteArray &out) {
                     if (!m_devices.contains(serial)) {
                         m_busy.remove(serial);
                         return;
                     }
                     if (code != 0 || !out.contains("package:")) {
                         m_busy.remove(serial);
                         return;
                     }
                     m_devices[serial].appInstalled = true;
                     const QString spec = QStringLiteral("tcp:%1").arg(m_options.port);
                     m_runner.run({QStringLiteral("-s"), serial, QStringLiteral("reverse"), spec, spec},
                                  [this, serial](int rc, const QByteArray &) {
                                      m_busy.remove(serial);
                                      if (auto it = m_devices.find(serial); it != m_devices.end()) {
                                          it->tunneled = rc == 0;
                                      }
                                      Q_EMIT devicesChanged();
                                  });
                 });
}

void TunnelManager::recheck()
{
    // Retry devices whose app was missing or whose reverse failed.
    for (const TunnelStatus &s : std::as_const(m_devices)) {
        if (!s.tunneled) {
            probe(s.serial);
        }
    }
}

} // namespace QindaQt::Link
