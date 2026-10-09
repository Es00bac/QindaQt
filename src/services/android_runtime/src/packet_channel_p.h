// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/protocol.h"
#include <QString>
#include <sys/stat.h>
namespace QindaQt::Services::AndroidRuntime {
struct ReadPacket { enum State { Empty, Received, Lost, Rejected } state=Empty; QByteArray bytes; };
// Owns all three descriptors; never stops/kills the producer. Expected identity came
// from trusted supervised bootstrap. This verifies transport/lifetime only,
// not executable, installed recipe, guest identity or mapped-window authority.
class PacketChannel final {
public:
    PacketChannel(int socketFd,int lifetimeFd,int executableFd,int expectedPid,unsigned expectedUid);
    ~PacketChannel();
    PacketChannel(const PacketChannel &)=delete;
    PacketChannel &operator=(const PacketChannel &)=delete;
    bool live() const;
    int socketFd() const { return m_socket; }
    int lifetimeFd() const { return m_lifetime; }
    int peerPid() const { return m_pid; }
    unsigned peerUid() const { return m_uid; }
    ReadPacket read();
    bool write(const QByteArray &bytes);
    void close();
private:
    int m_socket=-1;
    int m_lifetime=-1;
    int m_executable=-1;
    struct stat m_pin{};
    int m_pid=0;
    unsigned m_uid=0;
    bool m_valid=false;
};
} // namespace QindaQt::Services::AndroidRuntime
