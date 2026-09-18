// SPDX-License-Identifier: GPL-3.0-or-later
#include "avahi_service_discovery.h"

#include "network_location.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto avahiService = "org.freedesktop.Avahi";
constexpr auto avahiServerPath = "/";
constexpr auto avahiServerInterface = "org.freedesktop.Avahi.Server";
constexpr auto avahiBrowserInterface = "org.freedesktop.Avahi.ServiceBrowser";
// AVAHI_IF_UNSPEC / AVAHI_PROTO_UNSPEC: browse every interface and protocol,
// then collapse the duplicates ourselves by resolved identity.
constexpr int avahiUnspecified = -1;

// The identity of one advertisement, before it is resolved. Avahi reports a
// removal with the same tuple it reported the arrival with.
[[nodiscard]] QString announcementKey(int interfaceIndex, int protocol,
                                      const QString &name, const QString &type,
                                      const QString &domain) {
  return QStringLiteral("%1/%2/%3/%4/%5")
      .arg(interfaceIndex)
      .arg(protocol)
      .arg(name, type, domain);
}

// Avahi reports a trailing-dot FQDN; a location never carries one.
[[nodiscard]] QString trimmedHost(const QString &host) {
  QString trimmed = host.trimmed();
  while (trimmed.endsWith(QLatin1Char('.'))) {
    trimmed.chop(1);
  }
  return trimmed.toLower();
}

} // namespace

AvahiServiceDiscovery::AvahiServiceDiscovery(QDBusConnection bus, QObject *parent)
    : ServiceDiscovery(parent), m_bus(std::move(bus)) {}

AvahiServiceDiscovery::~AvahiServiceDiscovery() { retire(); }

QStringList AvahiServiceDiscovery::browsedServiceTypes() {
  return {QStringLiteral("_sftp-ssh._tcp"), QStringLiteral("_ssh._tcp"),
          QStringLiteral("_smb._tcp")};
}

QString AvahiServiceDiscovery::schemeForServiceType(const QString &serviceType) {
  if (serviceType == QLatin1String("_sftp-ssh._tcp") ||
      serviceType == QLatin1String("_ssh._tcp")) {
    return QStringLiteral("sftp");
  }
  if (serviceType == QLatin1String("_smb._tcp")) {
    return QStringLiteral("smb");
  }
  return {};
}

int AvahiServiceDiscovery::defaultPortFor(const QString &scheme) {
  if (scheme == QLatin1String("sftp")) {
    return 22;
  }
  if (scheme == QLatin1String("smb")) {
    return 445;
  }
  return 0;
}

QString AvahiServiceDiscovery::keyFor(const QString &scheme, const QString &host,
                                      const int port) {
  const QString cleanedHost = trimmedHost(host);
  if (scheme.isEmpty() || cleanedHost.isEmpty()) {
    return {};
  }
  const QString address =
      port > 0 && port != defaultPortFor(scheme)
          ? QStringLiteral("%1://%2:%3").arg(scheme, cleanedHost).arg(port)
          : QStringLiteral("%1://%2").arg(scheme, cleanedHost);
  // AGENT-GUARD: the identity is whatever the ADR-0137 allowlist accepts and
  // nothing else, so a hostile advertisement cannot introduce an address the
  // rest of the file manager would refuse to canonicalize later.
  const auto canonical = NetworkLocation::canonicalize(address);
  return canonical.has_value() ? canonical->toString() : QString();
}

bool AvahiServiceDiscovery::busAvailable() const { return m_bus.isConnected(); }

QDBusPendingCall
AvahiServiceDiscovery::createBrowserCall(const QString &serviceType) const {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(avahiService), QString::fromLatin1(avahiServerPath),
      QString::fromLatin1(avahiServerInterface), QStringLiteral("ServiceBrowserNew"));
  call.setArguments({avahiUnspecified, avahiUnspecified, serviceType, QString(),
                     static_cast<uint>(0)});
  return m_bus.asyncCall(call);
}

QDBusPendingCall AvahiServiceDiscovery::createResolveCall(
    const int interfaceIndex, const int protocol, const QString &name,
    const QString &type, const QString &domain) const {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(avahiService), QString::fromLatin1(avahiServerPath),
      QString::fromLatin1(avahiServerInterface), QStringLiteral("ResolveService"));
  call.setArguments({interfaceIndex, protocol, name, type, domain, avahiUnspecified,
                     static_cast<uint>(0)});
  return m_bus.asyncCall(call);
}

void AvahiServiceDiscovery::subscribeToBrowser(const QString &objectPath) {
  m_bus.connect(QString::fromLatin1(avahiService), objectPath,
                QString::fromLatin1(avahiBrowserInterface), QStringLiteral("ItemNew"),
                this, SLOT(onItemNew(int, int, QString, QString, QString, uint)));
  m_bus.connect(QString::fromLatin1(avahiService), objectPath,
                QString::fromLatin1(avahiBrowserInterface), QStringLiteral("ItemRemove"),
                this, SLOT(onItemRemove(int, int, QString, QString, QString, uint)));
  m_bus.connect(QString::fromLatin1(avahiService), objectPath,
                QString::fromLatin1(avahiBrowserInterface), QStringLiteral("Failure"),
                this, SLOT(onFailure(QString)));
}

void AvahiServiceDiscovery::releaseBrowser(const QString &objectPath) {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(avahiService), objectPath,
      QString::fromLatin1(avahiBrowserInterface), QStringLiteral("Free"));
  m_bus.asyncCall(call);
}

void AvahiServiceDiscovery::start() {
  if (m_running) {
    return;
  }
  m_running = true;
  if (!busAvailable()) {
    // Avahi is a platform service; if its bus is not there, say so rather
    // than showing an empty list that looks like "no servers on this network".
    Q_EMIT unavailable(QStringLiteral(
        "Nearby servers are unavailable: the system bus could not be reached."));
    return;
  }
  Q_EMIT unavailable(QString());
  const QStringList types = browsedServiceTypes();
  for (const QString &serviceType : types) {
    dispatchBrowser(serviceType);
  }
}

void AvahiServiceDiscovery::dispatchBrowser(const QString &serviceType) {
  auto *watcher = new QDBusPendingCallWatcher(createBrowserCall(serviceType), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *finished) {
            finished->deleteLater();
            if (!m_running) {
              return;
            }
            // AGENT-NOTE: the raw reply message is read rather than a typed
            // QDBusPendingReply, for the same reason handleResolved() does:
            // one extraction path, and no template signature check to get
            // right twice.
            const QDBusMessage reply = finished->reply();
            if (reply.type() != QDBusMessage::ReplyMessage) {
              Q_EMIT unavailable(NetworkLocation::boundedDiagnostic(
                  QStringLiteral("Nearby servers are unavailable: %1")
                      .arg(reply.errorMessage())));
              return;
            }
            const QList<QVariant> values = reply.arguments();
            const QString objectPath =
                values.isEmpty()
                    ? QString()
                    : (values.constFirst().canConvert<QDBusObjectPath>()
                           ? values.constFirst().value<QDBusObjectPath>().path()
                           : values.constFirst().toString());
            if (objectPath.isEmpty() || m_browserPaths.contains(objectPath)) {
              return;
            }
            m_browserPaths.append(objectPath);
            subscribeToBrowser(objectPath);
          });
}

void AvahiServiceDiscovery::handleItemNew(const int interfaceIndex, const int protocol,
                                          const QString &name, const QString &type,
                                          const QString &domain) {
  if (!m_running || schemeForServiceType(type).isEmpty()) {
    return;
  }
  const QString announcement =
      announcementKey(interfaceIndex, protocol, name, type, domain);
  if (m_announcementKeys.contains(announcement)) {
    return;
  }
  // Claim the announcement before the reply lands so two arrivals of the same
  // tuple cannot both resolve.
  m_announcementKeys.insert(announcement, QString());
  auto *watcher = new QDBusPendingCallWatcher(
      createResolveCall(interfaceIndex, protocol, name, type, domain), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, announcement](QDBusPendingCallWatcher *finished) {
            finished->deleteLater();
            handleResolved(announcement, finished->reply());
          });
}

void AvahiServiceDiscovery::handleResolved(const QString &announcement,
                                           const QDBusMessage &reply) {
  const auto claimed = m_announcementKeys.constFind(announcement);
  // The announcement was withdrawn, or discovery stopped, while the resolve
  // was in flight: publishing now would add a row nothing can retire.
  if (!m_running || claimed == m_announcementKeys.constEnd() ||
      !claimed.value().isEmpty()) {
    return;
  }
  const QList<QVariant> values = reply.arguments();
  // iissssisqaayu: name at 2, type at 3, host at 5, address at 7, port at 8.
  if (reply.type() != QDBusMessage::ReplyMessage || values.size() < 9) {
    m_announcementKeys.remove(announcement);
    return;
  }
  const QString scheme = schemeForServiceType(values.at(3).toString());
  const QString host = values.at(5).toString();
  const int port = values.at(8).toInt();
  const QString key = keyFor(scheme, host, port);
  if (key.isEmpty()) {
    m_announcementKeys.remove(announcement);
    return;
  }
  m_announcementKeys[announcement] = key;
  const int references = ++m_keyReferences[key];
  // The same machine seen on a second interface is the same row.
  if (references > 1) {
    return;
  }
  if (m_keyReferences.size() > maximumServices) {
    return;
  }
  DiscoveredService service;
  service.key = key;
  service.name = values.at(2).toString();
  service.host = trimmedHost(host);
  service.scheme = scheme;
  service.port = port == defaultPortFor(scheme) ? 0 : port;
  service.address = values.at(7).toString();
  Q_EMIT serviceFound(service);
}

void AvahiServiceDiscovery::handleItemRemove(const int interfaceIndex,
                                             const int protocol, const QString &name,
                                             const QString &type,
                                             const QString &domain) {
  const QString announcement =
      announcementKey(interfaceIndex, protocol, name, type, domain);
  const auto claimed = m_announcementKeys.constFind(announcement);
  if (claimed == m_announcementKeys.constEnd()) {
    return;
  }
  const QString key = claimed.value();
  m_announcementKeys.erase(claimed);
  if (key.isEmpty()) {
    return;
  }
  const auto references = m_keyReferences.find(key);
  if (references == m_keyReferences.end()) {
    return;
  }
  if (--references.value() > 0) {
    return;
  }
  m_keyReferences.erase(references);
  Q_EMIT serviceLost(key);
}

void AvahiServiceDiscovery::onItemNew(const int interfaceIndex, const int protocol,
                                      const QString &name, const QString &type,
                                      const QString &domain, uint) {
  handleItemNew(interfaceIndex, protocol, name, type, domain);
}

void AvahiServiceDiscovery::onItemRemove(const int interfaceIndex, const int protocol,
                                         const QString &name, const QString &type,
                                         const QString &domain, uint) {
  handleItemRemove(interfaceIndex, protocol, name, type, domain);
}

void AvahiServiceDiscovery::onFailure(const QString &diagnostic) {
  handleBrowserFailure(diagnostic);
}

void AvahiServiceDiscovery::handleBrowserFailure(const QString &diagnostic) {
  if (!m_running) {
    return;
  }
  Q_EMIT unavailable(NetworkLocation::boundedDiagnostic(
      QStringLiteral("Nearby servers are unavailable: %1").arg(diagnostic)));
}

void AvahiServiceDiscovery::stop() {
  if (!m_running) {
    return;
  }
  const QStringList keys = m_keyReferences.keys();
  retire();
  for (const QString &key : keys) {
    Q_EMIT serviceLost(key);
  }
}

void AvahiServiceDiscovery::retire() {
  m_running = false;
  const QStringList paths = m_browserPaths;
  m_browserPaths.clear();
  m_announcementKeys.clear();
  m_keyReferences.clear();
  for (const QString &objectPath : paths) {
    releaseBrowser(objectPath);
  }
}

} // namespace QindaQt::Apps::FileManager
