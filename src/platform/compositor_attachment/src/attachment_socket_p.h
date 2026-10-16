// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace QindaQt::Platform::Compositor::Private {
bool nativeName(const QString &name);
// Unavailable is transient (missing socket, refused or full backlog, exited
// peer); Mismatch means the pathname no longer leads to the proven compositor
// or its directory/socket protections changed.
enum class PeerStatus { Connected, Unavailable, Mismatch };
struct PeerConnection final {
  PeerStatus status = PeerStatus::Unavailable;
  int fd = -1;    // owned connected CLOEXEC socket when Connected
  int pidfd = -1; // owned peer PIDFD when requested and Connected
};
PeerConnection connectPeer(const QString &runtime, const QString &name,
                           qint64 expectedPid, bool wantPidfd);
} // namespace QindaQt::Platform::Compositor::Private
