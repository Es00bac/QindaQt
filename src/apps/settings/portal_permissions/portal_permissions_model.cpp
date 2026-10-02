// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_portal_permissions/portal_permissions_model.h>
#include <algorithm>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Apps::SettingsPortalPermissions {
namespace {
const QString service = QStringLiteral("org.freedesktop.impl.portal.PermissionStore");
const QString path = QStringLiteral("/org/freedesktop/impl/portal/PermissionStore");
const QStringList tables{QStringLiteral("screencast"), QStringLiteral("remote-desktop")};
bool validId(const QString &id) {
  static const QRegularExpression uuid(QStringLiteral(
      "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
  return uuid.match(id).hasMatch();
}
}
PortalPermissionsModel::PortalPermissionsModel(const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_bus(bus) {
  auto *watcher = new QDBusServiceWatcher(service, m_bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &) {
    invalidate(); refresh();
  });
  // Changed's opaque data is intentionally not decoded or retained.
  m_bus.connect(service, path, service, QStringLiteral("Changed"), this, SLOT(storeChanged()));
  QTimer::singleShot(0, this, &PortalPermissionsModel::refresh);
}
void PortalPermissionsModel::storeChanged() {
  if (m_busy) m_reload = true;
  else refresh();
}
void PortalPermissionsModel::invalidate() {
  ++m_generation; m_owner.clear(); m_rows.clear(); m_entries.clear();
  m_pending.clear(); m_available = false; m_busy = false; m_reload = false;
  m_error = tr("Portal permissions are unavailable."); emit changed();
}
void PortalPermissionsModel::fail(const QString &text) {
  ++m_generation; m_rows.clear(); m_entries.clear(); m_pending.clear(); m_available = false;
  m_busy = false; m_reload = false; m_error = text; emit changed();
}
void PortalPermissionsModel::refresh() {
  if (m_busy) { m_reload = true; return; }
  ++m_generation; m_busy = true; m_reload = false; m_error.clear();
  m_rows.clear(); m_entries.clear(); m_pending.clear(); emit changed();
  const auto generation = m_generation;
  m_seen = 0;
  QTimer::singleShot(10000, this, [this, generation] {
    if (generation == m_generation && m_busy) fail(tr("Reading portal permissions timed out."));
  });
  auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("GetNameOwner"));
  message << service;
  auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 2000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this,
          [this, generation](QDBusPendingCallWatcher *done) {
    const QDBusMessage reply = done->reply(); done->deleteLater();
    if (generation != m_generation) return;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1
        || !reply.arguments().first().toString().startsWith(QLatin1Char(':'))) {
      if (reply.errorName() != QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner")) {
        fail(tr("Portal permissions are unavailable.")); return;
      }
      // The existing store is D-Bus activatable even when the frontend has
      // not used it yet. Request only its standard activation, never create
      // a second database or service owned by Settings.
      auto start = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
          QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
          QStringLiteral("StartServiceByName"));
      start << service << quint32(0);
      auto *activation = new QDBusPendingCallWatcher(m_bus.asyncCall(start, 2000), this);
      connect(activation, &QDBusPendingCallWatcher::finished, this,
              [this, generation](QDBusPendingCallWatcher *activated) {
        const auto result = activated->reply(); activated->deleteLater();
        if (generation != m_generation) return;
        if (result.type() != QDBusMessage::ReplyMessage) {
          fail(tr("Portal permissions are unavailable.")); return;
        }
        m_busy = false; refresh();
      });
      return;
    }
    m_owner = reply.arguments().first().toString(); loadTable(0);
  });
}
void PortalPermissionsModel::call(const QString &method, const QVariantList &arguments, Reply callback) {
  auto message = QDBusMessage::createMethodCall(m_owner, path, service, method);
  message.setArguments(arguments);
  const auto generation = m_generation;
  auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 2000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this,
          [this, generation, callback = std::move(callback)](QDBusPendingCallWatcher *done) {
    const auto reply = done->reply(); done->deleteLater();
    if (generation == m_generation) callback(reply);
  });
}
void PortalPermissionsModel::loadTable(int index) {
  m_tableIndex = index;
  if (index == tables.size()) { finish(); return; }
  call(QStringLiteral("List"), {tables.at(index)}, [this](const QDBusMessage &reply) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1
        || reply.signature() != QStringLiteral("as")) {
      // PermissionStore reports NotFound for a table with no saved grants.
      if (reply.errorName() == QStringLiteral("org.freedesktop.portal.Error.NotFound")) {
        loadTable(m_tableIndex + 1); return;
      }
      fail(tr("Could not read remembered portal permissions.")); return;
    }
    const auto ids = qdbus_cast<QStringList>(reply.arguments().first());
    if (ids.size() > 512 || m_seen + ids.size() > 512) {
      fail(tr("The portal permission list exceeds the supported limit.")); return;
    }
    m_seen += static_cast<int>(ids.size());
    for (const auto &id : ids) if (validId(id)) m_pending.append({tables.at(m_tableIndex), id, {}});
    loadNext();
  });
}
QString PortalPermissionsModel::singletonApp(const QDBusMessage &reply, bool *valid) {
  *valid = false;
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2
      || reply.signature() != QStringLiteral("a{sas}v")) return {};
  const auto permissions = qdbus_cast<QMap<QString, QStringList>>(reply.arguments().first());
  // AGENT-GUARD: xdp 1.20.4 session-persistence stores one app/yes per UUID.
  // Never Delete a foreign or shared entry, which would revoke another app.
  if (permissions.size() != 1 || permissions.first() != QStringList{QStringLiteral("yes")}) return {};
  const QString app = permissions.firstKey();
  if (app.size() > 256 || app.contains(QChar::Null) || app.contains(QLatin1Char('\n'))) return {};
  *valid = true; return app;
}
void PortalPermissionsModel::loadNext() {
  if (m_pending.isEmpty()) { loadTable(m_tableIndex + 1); return; }
  const Entry entry = m_pending.takeFirst();
  call(QStringLiteral("Lookup"), {entry.table, entry.id}, [this, entry](const QDBusMessage &reply) {
    bool valid = false; const auto app = singletonApp(reply, &valid);
    if (reply.type() == QDBusMessage::ErrorMessage
        && reply.errorName() != QStringLiteral("org.freedesktop.portal.Error.NotFound")) {
      fail(tr("Could not read remembered portal permissions.")); return;
    }
    if (valid) m_entries.append({entry.table, entry.id, app});
    loadNext();
  });
}
void PortalPermissionsModel::finish() {
  ++m_generation; // Retire the whole-refresh deadline before a later revoke.
  for (const auto &entry : m_entries) m_rows.append(QVariantMap{
      {QStringLiteral("key"), entry.table + QLatin1Char('/') + entry.id},
      {QStringLiteral("app"), entry.app.isEmpty() ? tr("Unsandboxed application") : entry.app},
      {QStringLiteral("family"), entry.table == tables.first() ? tr("Screen sharing") : tr("Remote desktop")}});
  m_available = true; m_busy = false; emit changed();
  if (m_reload) { m_reload = false; QTimer::singleShot(0, this, &PortalPermissionsModel::refresh); }
}
bool PortalPermissionsModel::revoke(const QString &key) {
  if (!m_available || m_busy || m_owner.isEmpty()) return false;
  const auto found = std::find_if(m_entries.cbegin(), m_entries.cend(), [&key](const Entry &entry) {
    return entry.table + QLatin1Char('/') + entry.id == key;
  });
  if (found == m_entries.cend()) return false;
  const Entry entry = *found; m_busy = true; m_error.clear(); emit changed();
  call(QStringLiteral("Lookup"), {entry.table, entry.id}, [this, entry](const QDBusMessage &reply) {
    bool valid = false; const auto app = singletonApp(reply, &valid);
    if (!valid || app != entry.app) { fail(tr("The permission changed. Refresh before revoking.")); return; }
    call(QStringLiteral("Delete"), {entry.table, entry.id}, [this](const QDBusMessage &deleted) {
      if (deleted.type() != QDBusMessage::ReplyMessage || !deleted.arguments().isEmpty()) {
        fail(tr("Could not revoke the remembered permission.")); return;
      }
      m_busy = false; refresh();
    });
  });
  return true;
}
} // namespace QindaQt::Apps::SettingsPortalPermissions
