// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "service_discovery.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QHash>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// Production ServiceDiscovery (ADR-0197) on the platform's Avahi daemon,
// which is already installed and running on both machines. It speaks
// `org.freedesktop.Avahi.Server` on the injected bus: one ServiceBrowser per
// advertised type, then one ResolveService per announcement to turn it into a
// host name and port.
//
// AGENT-CONTRACT: this object browses and resolves. It never connects to a
// discovered server, never authenticates, never writes anything, and never
// starts, stops, or configures the Avahi daemon. The bus connection is
// injected (production passes the system bus, where Avahi lives) so a test
// can hand it a private one.
//
// AGENT-GUARD: `_ssh._tcp` and `_sftp-ssh._tcp` both become `sftp`, because
// KIO's sftp worker speaks SSH; `_smb._tcp` becomes `smb`. Nothing else is
// browsed. NFS is deliberately absent: the ADR-0137 allowlist cannot open it,
// and offering a row that cannot be opened is worse than offering none.
//
// Not final: tst_avahi_service_discovery.cpp subclasses this to answer the
// D-Bus seams with canned replies and to drive the browser signals directly,
// proving the whole state machine without a bus.
class AvahiServiceDiscovery : public ServiceDiscovery {
  Q_OBJECT

public:
  // Beyond this many distinct servers the browser stops publishing, so a
  // hostile or misconfigured network cannot grow the model without bound.
  static constexpr int maximumServices = 128;

  explicit AvahiServiceDiscovery(QDBusConnection bus, QObject *parent = nullptr);
  ~AvahiServiceDiscovery() override;

  void start() override;
  void stop() override;

  // The Avahi service types browsed, and the scheme each maps to.
  [[nodiscard]] static QStringList browsedServiceTypes();
  [[nodiscard]] static QString schemeForServiceType(const QString &serviceType);
  // The canonical identity for one resolved announcement, or an empty string
  // when the announcement cannot be expressed as a browsable location.
  [[nodiscard]] static QString keyFor(const QString &scheme, const QString &host,
                                      int port);
  [[nodiscard]] static int defaultPortFor(const QString &scheme);

protected:
  // AGENT-NOTE: the four D-Bus edges, isolated so a test subclass can answer
  // them with canned replies and never touch a bus.
  [[nodiscard]] virtual bool busAvailable() const;
  [[nodiscard]] virtual QDBusPendingCall createBrowserCall(const QString &serviceType) const;
  [[nodiscard]] virtual QDBusPendingCall
  createResolveCall(int interfaceIndex, int protocol, const QString &name,
                    const QString &type, const QString &domain) const;
  virtual void subscribeToBrowser(const QString &objectPath);
  virtual void releaseBrowser(const QString &objectPath);

  // The browser signal handlers, reachable by a test without a bus.
  void handleItemNew(int interfaceIndex, int protocol, const QString &name,
                     const QString &type, const QString &domain);
  void handleItemRemove(int interfaceIndex, int protocol, const QString &name,
                        const QString &type, const QString &domain);
  void handleBrowserFailure(const QString &diagnostic);
  // Called with a completed ResolveService reply for one announcement.
  void handleResolved(const QString &announcement, const QDBusMessage &reply);

private slots:
  // AGENT-NOTE: the three Avahi ServiceBrowser signals, connected by name
  // through QDBusConnection::connect(), so their signatures must stay exactly
  // what Avahi emits. They only forward to the handlers above, which is what
  // lets a test drive the state machine without a bus.
  void onItemNew(int interfaceIndex, int protocol, const QString &name,
                 const QString &type, const QString &domain, uint flags);
  void onItemRemove(int interfaceIndex, int protocol, const QString &name,
                    const QString &type, const QString &domain, uint flags);
  void onFailure(const QString &diagnostic);

private:
  void dispatchBrowser(const QString &serviceType);
  void retire();

  QDBusConnection m_bus;
  bool m_running = false;
  QStringList m_browserPaths;
  // announcement identity -> the published key it resolved to, so a removal
  // can retire the right row.
  QHash<QString, QString> m_announcementKeys;
  // published key -> how many announcements still back it, so one machine on
  // three interfaces disappears only when the last one does.
  QHash<QString, int> m_keyReferences;
};

} // namespace QindaQt::Apps::FileManager
