// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::Apps::FileManager {

// One server found on the local network, already reduced to something the
// file manager can actually open. Discovery never publishes a service whose
// scheme the ADR-0137 allowlist would refuse.
struct DiscoveredService final {
  // scheme://host[:port] -- the identity two advertisements of one machine on
  // two interfaces collapse to. One laptop advertising over wlan0, tailscale0
  // and lo is one row, not six.
  QString key;
  // The advertised service name, e.g. "qinda-14".
  QString name;
  // The resolved host name, e.g. "qinda-14.local". Preferred over the numeric
  // address so ~/.ssh/config and known_hosts behave as the user expects.
  QString host;
  // "sftp" or "smb".
  QString scheme;
  // 0 means "the scheme's default port", which is what keeps the published
  // address free of a redundant ":22".
  int port = 0;
  // The resolved numeric address. Shown as a subtitle; never used to connect.
  QString address;

  [[nodiscard]] bool operator==(const DiscoveredService &) const = default;
};

// AGENT-CONTRACT: browses the local network for servers the file manager can
// open (ADR-0200). An implementation must:
//  - publish only canonical, userinfo-free sftp/smb identities;
//  - emit serviceFound() at most once per distinct key until the matching
//    serviceLost(), so the model never has to deduplicate a second time;
//  - report an unreachable or failed browser through unavailable() rather
//    than staying silent, because silence is indistinguishable from "no
//    servers here";
//  - do nothing at all until start(), and hold no subscription after stop()
//    or destruction.
//
// AGENT-GUARD: discovery is advisory. Nothing it publishes may be treated as
// reachable, authenticated, or trusted; opening a discovered address goes
// through exactly the same navigation and credential path as a typed one.
class ServiceDiscovery : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~ServiceDiscovery() override = default;

  virtual void start() = 0;
  virtual void stop() = 0;

signals:
  void serviceFound(const DiscoveredService &service);
  void serviceLost(const QString &key);
  // Bounded, human-readable. An empty string means "the previous problem is
  // over"; discovery uses it when a retry succeeds.
  void unavailable(const QString &diagnostic);
};

using ServiceDiscoveryPtr = std::unique_ptr<ServiceDiscovery>;

} // namespace QindaQt::Apps::FileManager

// DiscoveredService crosses a signal, so it must be a registered metatype.
Q_DECLARE_METATYPE(QindaQt::Apps::FileManager::DiscoveredService)
