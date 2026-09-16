// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>

#include <atomic>
#include <memory>

struct pw_context;

namespace QindaQt::Audio
{

// VBAN on the graph (ADR-0185). Both directions follow the recorder's rule:
// the realtime callback touches a lock-free ring and nothing else. The
// sockets live on their own threads, since a send or a receive may block.
//
// A sender captures a bus's device from its monitor and packetises it as
// 48 kHz stereo PCM16, 256 frames per packet, the reference console's
// default. A receiver takes packets for one stream name on one port and
// presents them as a virtual source other applications and the console's
// strips can read; a late or lost packet is silence, never a stall.
class VbanSender final
{
public:
    VbanSender();
    ~VbanSender();
    VbanSender(const VbanSender &) = delete;
    VbanSender &operator=(const VbanSender &) = delete;

    [[nodiscard]] bool start(pw_context *context, const QString &nodeName,
                             const QString &streamName, const QString &host, quint16 port);
    void stop();
    [[nodiscard]] bool running() const noexcept { return m_running.load(); }

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
    std::atomic_bool m_running{false};
};

class VbanReceiver final
{
public:
    VbanReceiver();
    ~VbanReceiver();
    VbanReceiver(const VbanReceiver &) = delete;
    VbanReceiver &operator=(const VbanReceiver &) = delete;

    [[nodiscard]] bool start(pw_context *context, const QString &streamName, quint16 port);
    void stop();
    [[nodiscard]] bool running() const noexcept { return m_running.load(); }

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
    std::atomic_bool m_running{false};
};

// The virtual source a received stream is presented as.
[[nodiscard]] QString vbanSourceNodeName(const QString &streamName);

} // namespace QindaQt::Audio
