// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/workspaces/qt_workspace_transport.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::Workspaces {
namespace {

constexpr auto KWinService = "org.kde.KWin";
constexpr auto KWinPath = "/KWin";
constexpr auto KWinInterface = "org.kde.KWin";
constexpr auto DesktopsPath = "/VirtualDesktopManager";
constexpr auto DesktopsInterface = "org.kde.KWin.VirtualDesktopManager";
constexpr auto PropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto BusService = "org.freedesktop.DBus";
constexpr auto BusPath = "/org/freedesktop/DBus";
constexpr auto BusInterface = "org.freedesktop.DBus";

QDBusMessage methodCall(const QString &owner, const char *path,
                        const char *interface, const QString &member)
{
  QDBusMessage message = QDBusMessage::createMethodCall(
      owner, QString::fromLatin1(path), QString::fromLatin1(interface), member);
  // AGENT-GUARD: never auto-start a service. A request to a unique owner
  // cannot activate anything, but the bus-daemon lookups below name the
  // well-known service; activation would let an unrelated peer become "KWin".
  message.setAutoStartService(false);
  return message;
}

// Decodes KWin's `a(iss)` desktops property. Bounded and total: a malformed or
// oversized array yields nullopt instead of a partial list.
std::optional<QList<WorkspaceRow>> decodeDesktops(const QVariant &value)
{
  if (value.userType() != qMetaTypeId<QDBusArgument>()) {
    return std::nullopt;
  }
  const QDBusArgument argument = value.value<QDBusArgument>();
  if (argument.currentType() != QDBusArgument::ArrayType) {
    return std::nullopt;
  }
  QList<WorkspaceRow> rows;
  argument.beginArray();
  while (!argument.atEnd()) {
    if (argument.currentType() != QDBusArgument::StructureType
        || rows.size() >= Bounds::maxWorkspaces) {
      return std::nullopt;
    }
    WorkspaceRow row;
    argument.beginStructure();
    argument >> row.position >> row.id >> row.name;
    argument.endStructure();
    rows.append(std::move(row));
  }
  argument.endArray();
  return rows;
}

} // namespace

QtWorkspaceTransport::QtWorkspaceTransport(QDBusConnection connection,
                                           WorkspaceAuthority authority,
                                           QObject *parent)
    : WorkspaceTransport(parent)
    , m_connection(std::move(connection))
    , m_authority(std::move(authority))
{
}

QtWorkspaceTransport::~QtWorkspaceTransport()
{
  stop();
}

bool QtWorkspaceTransport::start(QString *error)
{
  if (m_started) {
    return true;
  }
  if (!m_connection.isConnected()) {
    if (error != nullptr) {
      *error = QStringLiteral("workspace transport has no session bus");
    }
    return false;
  }
  m_started = true;
  m_serviceWatcher = new QDBusServiceWatcher(this);
  m_serviceWatcher->setConnection(m_connection);
  m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
  m_serviceWatcher->addWatchedService(QString::fromLatin1(KWinService));
  if (!m_authority.compositorProcessId.has_value()
      && !m_authority.peerServiceName.isEmpty()) {
    m_serviceWatcher->addWatchedService(m_authority.peerServiceName);
  }
  connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          &QtWorkspaceTransport::handleServiceOwnerChanged);
  resolveOwner();
  return true;
}

void QtWorkspaceTransport::stop()
{
  if (!m_started) {
    return;
  }
  m_started = false;
  ++m_resolveGeneration;
  for (const QPointer<QDBusPendingCallWatcher> &watcher : m_pendingCalls) {
    if (watcher) {
      watcher->deleteLater();
    }
  }
  m_pendingCalls.clear();
  unbindOwner(QStringLiteral("workspace-transport-stopped"));
  delete m_serviceWatcher;
  m_serviceWatcher = nullptr;
}

void QtWorkspaceTransport::handleServiceOwnerChanged(const QString &service,
                                                     const QString &,
                                                     const QString &)
{
  Q_UNUSED(service)
  // Either KWin or the peer authority moved: forget everything, then resolve
  // again. The controller sees an explicit unavailable state in between.
  ++m_resolveGeneration;
  unbindOwner(QStringLiteral("compositor-owner-changed"));
  resolveOwner();
}

void QtWorkspaceTransport::resolveOwner()
{
  if (!m_started) {
    return;
  }
  const quint64 generation = ++m_resolveGeneration;
  requestNameOwner(generation, QString::fromLatin1(KWinService),
                   &QtWorkspaceTransport::handleKWinOwner,
                   QStringLiteral("compositor"));
}

void QtWorkspaceTransport::requestNameOwner(
    quint64 generation, const QString &service,
    void (QtWorkspaceTransport::*next)(quint64, const QString &, const QString &),
    const QString &context)
{
  QDBusMessage message = methodCall(QString::fromLatin1(BusService), BusPath,
                                    BusInterface, QStringLiteral("GetNameOwner"));
  message << service;
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation, next, context] {
            untrack(watcher);
            if (generation != m_resolveGeneration) {
              return;
            }
            const QDBusPendingReply<QString> reply = *watcher;
            if (reply.isError()) {
              unbindOwner(context + QStringLiteral("-unavailable"));
              return;
            }
            (this->*next)(generation, context, reply.value());
          });
}

void QtWorkspaceTransport::requestProcessId(
    quint64 generation, const QString &uniqueOwner,
    void (QtWorkspaceTransport::*next)(quint64, const QString &, qint64),
    const QString &context)
{
  QDBusMessage message =
      methodCall(QString::fromLatin1(BusService), BusPath, BusInterface,
                 QStringLiteral("GetConnectionUnixProcessID"));
  message << uniqueOwner;
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation, next, context] {
            untrack(watcher);
            if (generation != m_resolveGeneration) {
              return;
            }
            const QDBusPendingReply<quint32> reply = *watcher;
            if (reply.isError()) {
              unbindOwner(context + QStringLiteral("-identity-unknown"));
              return;
            }
            (this->*next)(generation, context, static_cast<qint64>(reply.value()));
          });
}

void QtWorkspaceTransport::handleKWinOwner(quint64 generation,
                                           const QString &context,
                                           const QString &owner)
{
  if (owner.isEmpty()) {
    unbindOwner(QStringLiteral("compositor-unavailable"));
    return;
  }
  m_pendingKWinOwner = owner;
  requestProcessId(generation, owner, &QtWorkspaceTransport::handleKWinProcessId,
                   context);
}

void QtWorkspaceTransport::handleKWinProcessId(quint64 generation,
                                               const QString &,
                                               qint64 processId)
{
  m_pendingKWinProcessId = processId;
  if (m_authority.compositorProcessId.has_value()) {
    authenticate(generation, m_pendingKWinOwner, processId,
                 *m_authority.compositorProcessId);
    return;
  }
  if (m_authority.peerServiceName.isEmpty()) {
    unbindOwner(QStringLiteral("compositor-identity-unknown"));
    return;
  }
  requestNameOwner(generation, m_authority.peerServiceName,
                   &QtWorkspaceTransport::handlePeerOwner,
                   QStringLiteral("compositor-peer"));
}

void QtWorkspaceTransport::handlePeerOwner(quint64 generation,
                                           const QString &context,
                                           const QString &owner)
{
  if (owner.isEmpty()) {
    unbindOwner(QStringLiteral("compositor-peer-unavailable"));
    return;
  }
  requestProcessId(generation, owner, &QtWorkspaceTransport::handlePeerProcessId,
                   context);
}

void QtWorkspaceTransport::handlePeerProcessId(quint64 generation, const QString &,
                                               qint64 processId)
{
  authenticate(generation, m_pendingKWinOwner, m_pendingKWinProcessId, processId);
}

void QtWorkspaceTransport::authenticate(quint64 generation, const QString &kwinOwner,
                                        qint64 kwinProcessId,
                                        qint64 expectedProcessId)
{
  if (generation != m_resolveGeneration) {
    return;
  }
  if (kwinProcessId <= 1 || kwinProcessId != expectedProcessId) {
    unbindOwner(QStringLiteral("compositor-identity-mismatch"));
    return;
  }
  bindOwner(kwinOwner);
}

void QtWorkspaceTransport::bindOwner(const QString &owner)
{
  if (owner == m_owner) {
    return;
  }
  if (m_signalsBound) {
    unbindOwner(QStringLiteral("compositor-owner-changed"));
  }
  m_owner = owner;
  const QString desktopsInterface = QString::fromLatin1(DesktopsInterface);
  const QString desktopsPath = QString::fromLatin1(DesktopsPath);
  bool bound = true;
  for (const char *signal : {"currentChanged", "countChanged", "rowsChanged",
                             "desktopCreated", "desktopRemoved",
                             "desktopDataChanged"}) {
    bound = m_connection.connect(owner, desktopsPath, desktopsInterface,
                                 QString::fromLatin1(signal), this,
                                 SLOT(handleWorkspaceSignal()))
        && bound;
  }
  bound = m_connection.connect(owner, QString::fromLatin1(KWinPath),
                               QString::fromLatin1(KWinInterface),
                               QStringLiteral("showingDesktopChanged"), this,
                               SLOT(handleWorkspaceSignal()))
      && bound;
  m_signalsBound = true;
  if (!bound) {
    unbindOwner(QStringLiteral("compositor-signal-subscription-failed"));
    return;
  }
  Q_EMIT ownerChanged(m_owner, QString{});
}

void QtWorkspaceTransport::unbindOwner(const QString &reasonCode)
{
  if (m_signalsBound && !m_owner.isEmpty()) {
    const QString desktopsInterface = QString::fromLatin1(DesktopsInterface);
    const QString desktopsPath = QString::fromLatin1(DesktopsPath);
    for (const char *signal : {"currentChanged", "countChanged", "rowsChanged",
                               "desktopCreated", "desktopRemoved",
                               "desktopDataChanged"}) {
      m_connection.disconnect(m_owner, desktopsPath, desktopsInterface,
                              QString::fromLatin1(signal), this,
                              SLOT(handleWorkspaceSignal()));
    }
    m_connection.disconnect(m_owner, QString::fromLatin1(KWinPath),
                            QString::fromLatin1(KWinInterface),
                            QStringLiteral("showingDesktopChanged"), this,
                            SLOT(handleWorkspaceSignal()));
  }
  m_signalsBound = false;
  m_owner.clear();
  m_pendingKWinOwner.clear();
  m_pendingKWinProcessId = 0;
  Q_EMIT ownerChanged(QString{}, reasonCode);
}

void QtWorkspaceTransport::handleWorkspaceSignal()
{
  if (!m_started || m_owner.isEmpty()) {
    return;
  }
  Q_EMIT changed(m_owner);
}

void QtWorkspaceTransport::requestSnapshot(quint64 token, const QString &uniqueOwner)
{
  if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_owner) {
    Q_EMIT snapshotFailed(token, uniqueOwner, QStringLiteral("owner-not-bound"));
    return;
  }
  QDBusMessage message = methodCall(uniqueOwner, DesktopsPath, PropertiesInterface,
                                    QStringLiteral("GetAll"));
  message << QString::fromLatin1(DesktopsInterface);
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner] {
            untrack(watcher);
            if (!m_started) {
              return;
            }
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            if (reply.isError()) {
              Q_EMIT snapshotFailed(token, uniqueOwner,
                                    QStringLiteral("desktops-read-failed"));
              return;
            }
            const QVariantMap properties = reply.value();
            const auto desktops =
                decodeDesktops(properties.value(QStringLiteral("desktops")));
            const QVariant current = properties.value(QStringLiteral("current"));
            const QVariant rows = properties.value(QStringLiteral("rows"));
            if (!desktops.has_value() || current.userType() != QMetaType::QString
                || !rows.canConvert<quint32>()) {
              Q_EMIT snapshotFailed(token, uniqueOwner,
                                    QStringLiteral("desktops-malformed"));
              return;
            }
            WorkspaceSnapshot snapshot;
            snapshot.desktops = *desktops;
            snapshot.currentId = current.toString();
            snapshot.rows = rows.value<quint32>();
            finishSnapshot(token, uniqueOwner, std::move(snapshot));
          });
}

void QtWorkspaceTransport::finishSnapshot(quint64 token, const QString &owner,
                                          WorkspaceSnapshot snapshot)
{
  QDBusMessage message =
      methodCall(owner, KWinPath, PropertiesInterface, QStringLiteral("Get"));
  message << QString::fromLatin1(KWinInterface) << QStringLiteral("showingDesktop");
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, owner, snapshot = std::move(snapshot)]() mutable {
            untrack(watcher);
            if (!m_started) {
              return;
            }
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            if (reply.isError()) {
              Q_EMIT snapshotFailed(token, owner,
                                    QStringLiteral("showing-desktop-read-failed"));
              return;
            }
            const QVariant value = reply.value().variant();
            if (value.userType() != QMetaType::Bool) {
              Q_EMIT snapshotFailed(token, owner,
                                    QStringLiteral("showing-desktop-malformed"));
              return;
            }
            snapshot.showingDesktop = value.toBool();
            Q_EMIT snapshotReceived(token, owner, snapshot);
          });
}

void QtWorkspaceTransport::requestSwitch(quint64 token, const QString &uniqueOwner,
                                         const QString &desktopId)
{
  if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_owner) {
    Q_EMIT operationFinished(token, uniqueOwner, false,
                             QStringLiteral("owner-not-bound"));
    return;
  }
  QDBusMessage message = methodCall(uniqueOwner, DesktopsPath, PropertiesInterface,
                                    QStringLiteral("Set"));
  message << QString::fromLatin1(DesktopsInterface) << QStringLiteral("current")
          << QVariant::fromValue(QDBusVariant(desktopId));
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner] {
            untrack(watcher);
            if (!m_started) {
              return;
            }
            const QDBusPendingReply<> reply = *watcher;
            if (reply.isError()) {
              Q_EMIT operationFinished(token, uniqueOwner, false,
                                       reply.error().name());
              return;
            }
            Q_EMIT operationFinished(token, uniqueOwner, true, QString{});
          });
}

void QtWorkspaceTransport::requestShowDesktop(quint64 token,
                                              const QString &uniqueOwner,
                                              bool showing)
{
  if (!m_started || uniqueOwner.isEmpty() || uniqueOwner != m_owner) {
    Q_EMIT operationFinished(token, uniqueOwner, false,
                             QStringLiteral("owner-not-bound"));
    return;
  }
  QDBusMessage message = methodCall(uniqueOwner, KWinPath, KWinInterface,
                                    QStringLiteral("showDesktop"));
  message << showing;
  QDBusPendingCallWatcher *watcher = track(message);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, token, uniqueOwner] {
            untrack(watcher);
            if (!m_started) {
              return;
            }
            const QDBusPendingReply<> reply = *watcher;
            // AGENT-NOTE: the XML marks showDesktop NoReply, but QtDBus
            // adaptors still answer a reply-expecting call. A timeout is the
            // only uncertain outcome; the controller then re-reads truth from
            // the showingDesktop property instead of assuming success.
            if (reply.isError()) {
              Q_EMIT operationFinished(token, uniqueOwner, false,
                                       reply.error().name());
              return;
            }
            Q_EMIT operationFinished(token, uniqueOwner, true, QString{});
          });
}

QDBusPendingCallWatcher *QtWorkspaceTransport::track(const QDBusMessage &message)
{
  auto *watcher = new QDBusPendingCallWatcher(
      m_connection.asyncCall(message, RequestTimeoutMilliseconds), this);
  m_pendingCalls.append(watcher);
  return watcher;
}

void QtWorkspaceTransport::untrack(QDBusPendingCallWatcher *watcher)
{
  m_pendingCalls.removeAll(QPointer<QDBusPendingCallWatcher>(watcher));
  watcher->deleteLater();
}

} // namespace QindaQt::Shell::Workspaces
