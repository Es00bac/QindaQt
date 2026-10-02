// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <functional>

namespace QindaQt::Apps::SettingsPortalPermissions {
// GUI-thread, route-owned projection of the existing frontend PermissionStore.
// The injected connection is borrowed for this object's lifetime. No restore
// data is exposed to QML; failed/owner-lost operations are never replayed.
class PortalPermissionsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
  explicit PortalPermissionsModel(const QDBusConnection &bus, QObject *parent = nullptr);
  QVariantList rows() const { return m_rows; }
  bool available() const { return m_available; }
  bool busy() const { return m_busy; }
  QString errorText() const { return m_error; }
  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool revoke(const QString &key);
Q_SIGNALS:
  void changed();
private Q_SLOTS:
  void storeChanged();
private:
  using Reply = std::function<void(const QDBusMessage &)>;
  void call(const QString &method, const QVariantList &arguments, Reply reply);
  void loadTable(int index);
  void loadNext();
  void finish();
  void fail(const QString &text);
  void invalidate();
  struct Entry { QString table; QString id; QString app; };
  static QString singletonApp(const QDBusMessage &reply, bool *valid);
  QDBusConnection m_bus;
  QString m_owner;
  quint64 m_generation = 0;
  bool m_available = false;
  bool m_busy = false;
  bool m_reload = false;
  QString m_error;
  QVariantList m_rows;
  QList<Entry> m_entries;
  QList<Entry> m_pending;
  int m_tableIndex = 0;
  int m_seen = 0;
};
} // namespace QindaQt::Apps::SettingsPortalPermissions
