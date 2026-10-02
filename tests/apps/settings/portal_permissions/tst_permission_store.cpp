// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_portal_permissions/portal_permissions_model.h>
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusVariant>
#include <QtCore/QTimer>
#include <QtTest/QTest>
using namespace QindaQt::Apps::SettingsPortalPermissions;
using Permissions = QMap<QString, QStringList>;
namespace {
const QString service = QStringLiteral("org.freedesktop.impl.portal.PermissionStore");
const QString path = QStringLiteral("/org/freedesktop/impl/portal/PermissionStore");
const QString first = QStringLiteral("11111111-1111-1111-1111-111111111111");
const QString second = QStringLiteral("22222222-2222-2222-2222-222222222222");
}
class Store final : public QDBusVirtualObject {
  Q_OBJECT
public:
  QMap<QString, Permissions> entries;
  QStringList deleted;
  bool refuse = false;
  bool delay = false;
  bool oversized = false;
  bool malformed = false;
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
    const auto args = message.arguments();
    if (message.member() == QStringLiteral("List")) {
      QStringList ids;
      const QString prefix = args.first().toString() + QLatin1Char('/');
      for (auto it = entries.cbegin(); it != entries.cend(); ++it)
        if (it.key().startsWith(prefix)) ids.append(it.key().mid(prefix.size()));
      if (oversized) for (int i = 0; i < 513; ++i) ids.append(QString::number(i));
      const auto reply = message.createReply({ids});
      if (delay) QTimer::singleShot(120, this, [connection, reply] { connection.send(reply); });
      else connection.send(reply);
    } else if (message.member() == QStringLiteral("Lookup")) {
      const auto key = args.at(0).toString() + QLatin1Char('/') + args.at(1).toString();
      if (malformed) connection.send(message.createReply({QStringLiteral("wrong signature")}));
      else if (entries.contains(key)) connection.send(message.createReply({
          QVariant::fromValue(entries.value(key)), QVariant::fromValue(QDBusVariant(QStringLiteral("opaque")))}));
      else connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotFound"), QStringLiteral("gone")));
    } else if (message.member() == QStringLiteral("Delete")) {
      const auto key = args.at(0).toString() + QLatin1Char('/') + args.at(1).toString();
      if (refuse) connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotAllowed"), QStringLiteral("refused")));
      else { deleted.append(key); entries.remove(key); connection.send(message.createReply()); }
    } else return false;
    return true;
  }
};
class PermissionStoreTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase() { qDBusRegisterMetaType<Permissions>(); }
  void listDeletePreservesOtherEntries() {
    auto bus = QDBusConnection::sessionBus(); Store store;
    const auto one = QStringLiteral("screencast/") + first;
    const auto two = QStringLiteral("remote-desktop/") + second;
    store.entries[one] = {{QStringLiteral("org.example.One"), {QStringLiteral("yes")}}};
    store.entries[two] = {{QStringLiteral("org.example.Two"), {QStringLiteral("yes")}}};
    store.entries[QStringLiteral("screencast/shared")] = {{QStringLiteral("foreign"), {QStringLiteral("yes")}}};
    store.entries[QStringLiteral("documents/other")] = {{QStringLiteral("foreign"), {QStringLiteral("yes")}}};
    QVERIFY(bus.registerVirtualObject(path, &store)); QVERIFY(bus.registerService(service));
    {
      PortalPermissionsModel model(bus); QTRY_VERIFY_WITH_TIMEOUT(model.available() && !model.busy(), 1500);
      QCOMPARE(model.rows().size(), 2); QVERIFY(!model.revoke(QStringLiteral("documents/other")));
      QVERIFY(model.revoke(one)); QVERIFY(!model.revoke(two));
      QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
      QCOMPARE(store.deleted, QStringList{one}); QCOMPARE(model.rows().size(), 1);
      QVERIFY(store.entries.contains(two)); QVERIFY(store.entries.contains(QStringLiteral("documents/other")));
      QVERIFY(store.entries.contains(QStringLiteral("screencast/shared")));
    }
    bus.unregisterService(service); bus.unregisterObject(path);
  }
  void refusalAndChangedGrantDoNotDelete() {
    auto bus = QDBusConnection::sessionBus(); Store store;
    const auto key = QStringLiteral("screencast/") + first;
    store.entries[key] = {{QStringLiteral("org.example.One"), {QStringLiteral("yes")}}};
    QVERIFY(bus.registerVirtualObject(path, &store)); QVERIFY(bus.registerService(service));
    {
      PortalPermissionsModel model(bus); QTRY_VERIFY_WITH_TIMEOUT(model.available(), 1500);
      store.refuse = true; QVERIFY(model.revoke(key));
      QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500); QVERIFY(!model.available()); QVERIFY(!model.errorText().isEmpty());
      QVERIFY(store.deleted.isEmpty()); QVERIFY(store.entries.contains(key));
      store.refuse = false; model.refresh(); QTRY_VERIFY_WITH_TIMEOUT(model.available(), 1500);
      store.entries[key].insert(QStringLiteral("other-app"), {QStringLiteral("yes")});
      QVERIFY(model.revoke(key)); QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
      QVERIFY(store.deleted.isEmpty()); QVERIFY(!model.available());
    }
    bus.unregisterService(service); bus.unregisterObject(path);
  }
  void boundedListAndMalformedReplyRefuse() {
    auto bus = QDBusConnection::sessionBus(); Store store;
    store.oversized = true;
    store.entries[QStringLiteral("screencast/") + first] = {{QStringLiteral("org.example.One"), {QStringLiteral("yes")}}};
    QVERIFY(bus.registerVirtualObject(path, &store)); QVERIFY(bus.registerService(service));
    {
      PortalPermissionsModel model(bus); QTRY_VERIFY_WITH_TIMEOUT(!model.busy() && !model.errorText().isEmpty(), 1500);
      QVERIFY(!model.available()); QVERIFY(model.rows().isEmpty()); QVERIFY(store.deleted.isEmpty());
      store.oversized = false; store.malformed = true; model.refresh();
      QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
      QVERIFY(!model.available()); QVERIFY(model.rows().isEmpty()); QVERIFY(store.deleted.isEmpty());
    }
    bus.unregisterService(service); bus.unregisterObject(path);
  }
  void ownerLossDropsLateListAndUnavailable() {
    auto bus = QDBusConnection::sessionBus(); Store store; store.delay = true;
    store.entries[QStringLiteral("screencast/") + first] = {{QStringLiteral("org.example.One"), {QStringLiteral("yes")}}};
    QVERIFY(bus.registerVirtualObject(path, &store)); QVERIFY(bus.registerService(service));
    {
      PortalPermissionsModel model(bus); QTest::qWait(30);
      QVERIFY(bus.unregisterService(service)); QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
      QTest::qWait(180); QVERIFY(!model.available()); QVERIFY(model.rows().isEmpty());
      QVERIFY(!model.revoke(QStringLiteral("screencast/") + first)); QVERIFY(store.deleted.isEmpty());
    }
    bus.unregisterObject(path);
  }
};
QTEST_GUILESS_MAIN(PermissionStoreTest)
#include "tst_permission_store.moc"
