// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_network_manager_types.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>

#include <NetworkManager.h>

using QindaQt::Network::NetworkManager::TestSupport::Interfaces;
using QindaQt::Network::NetworkManager::TestSupport::ObjectTree;
using QindaQt::Network::NetworkManager::TestSupport::SettingsMap;

namespace {

constexpr auto kService = "org.freedesktop.NetworkManager";
constexpr auto kManagerPath = "/org/freedesktop/NetworkManager";
constexpr auto kManagerInterface = "org.freedesktop.NetworkManager";
constexpr auto kDevicePath = "/org/freedesktop/NetworkManager/Devices/1";
constexpr auto kAccessPointPath =
    "/org/freedesktop/NetworkManager/AccessPoint/1";
constexpr auto kControlInterface = "org.qindaqt.tests.FakeNetworkManager";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

using ObjectPaths = QList<QDBusObjectPath>;

class FakeNetworkManager final : public QDBusVirtualObject {
  Q_OBJECT

public:
  explicit FakeNetworkManager(QString security, QObject *parent = nullptr)
      : QDBusVirtualObject(parent), m_security(std::move(security)) {}

  [[nodiscard]] QString introspect(const QString &path) const override {
    Q_UNUSED(path)
    return QStringLiteral("<node/>");
  }

  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &connection) override {
    if (message.type() != QDBusMessage::MethodCallMessage) {
      return false;
    }
    if (message.path() == QLatin1String("/org/freedesktop") &&
        message.interface() ==
            QLatin1String("org.freedesktop.DBus.ObjectManager") &&
        message.member() == QLatin1String("GetManagedObjects")) {
      sendReply(connection, message, QVariant::fromValue(objectTree()));
      return true;
    }
    if (message.interface() == QLatin1String(kPropertiesInterface)) {
      return handleProperties(message, connection);
    }
    if (message.path() == QLatin1String(kManagerPath) &&
        message.interface() == QLatin1String(kManagerInterface)) {
      return handleManager(message, connection);
    }
    if (message.path() == QLatin1String(kDevicePath) &&
        message.interface() ==
            QLatin1String("org.freedesktop.NetworkManager.Device.Wireless") &&
        (message.member() == QLatin1String("GetAccessPoints") ||
         message.member() == QLatin1String("GetAllAccessPoints"))) {
      sendReply(connection, message,
                QVariant::fromValue(ObjectPaths{
                    QDBusObjectPath(QString::fromLatin1(kAccessPointPath))}));
      return true;
    }
    if (message.path() == QLatin1String(kManagerPath) &&
        message.interface() == QLatin1String(kControlInterface)) {
      return handleControl(message, connection);
    }
    sendError(connection, message);
    return true;
  }

private:
  [[nodiscard]] ObjectTree objectTree() const {
    ObjectTree tree;
    tree.insert(
        QDBusObjectPath(QString::fromLatin1(kManagerPath)),
        {{QString::fromLatin1(kManagerInterface), managerProperties()}});
    tree.insert(
        QDBusObjectPath(
            QStringLiteral("/org/freedesktop/NetworkManager/Settings")),
        {{QStringLiteral("org.freedesktop.NetworkManager.Settings"),
          properties(
              QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
              QStringLiteral("org.freedesktop.NetworkManager.Settings"))}});
    tree.insert(
        QDBusObjectPath(QString::fromLatin1(kDevicePath)),
        {{QStringLiteral("org.freedesktop.NetworkManager.Device"),
          deviceProperties()},
         {QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless"),
          wirelessProperties()}});
    tree.insert(QDBusObjectPath(QString::fromLatin1(kAccessPointPath)),
                {{QStringLiteral("org.freedesktop.NetworkManager.AccessPoint"),
                  accessPointProperties()}});
    return tree;
  }

  [[nodiscard]] QVariantMap properties(const QString &path,
                                       const QString &interface) const {
    if (path == QLatin1String(kManagerPath) &&
        interface == QLatin1String(kManagerInterface)) {
      return managerProperties();
    }
    if (path == QLatin1String(kDevicePath) &&
        interface == QLatin1String("org.freedesktop.NetworkManager.Device")) {
      return deviceProperties();
    }
    if (path == QLatin1String(kDevicePath) &&
        interface ==
            QLatin1String("org.freedesktop.NetworkManager.Device.Wireless")) {
      return wirelessProperties();
    }
    if (path == QLatin1String(kAccessPointPath) &&
        interface ==
            QLatin1String("org.freedesktop.NetworkManager.AccessPoint")) {
      return accessPointProperties();
    }
    if (path == QLatin1String("/org/freedesktop/NetworkManager/Settings") &&
        interface == QLatin1String("org.freedesktop.NetworkManager.Settings")) {
      return {
          {QStringLiteral("Connections"), QVariant::fromValue(ObjectPaths{})},
          {QStringLiteral("Hostname"), QString{}},
          {QStringLiteral("CanModify"), true}};
    }
    return {};
  }

  [[nodiscard]] QVariantMap managerProperties() const {
    const QDBusObjectPath root(QStringLiteral("/"));
    const ObjectPaths devices{
        QDBusObjectPath(QString::fromLatin1(kDevicePath))};
    return {
        {QStringLiteral("Devices"), QVariant::fromValue(devices)},
        {QStringLiteral("AllDevices"), QVariant::fromValue(devices)},
        {QStringLiteral("NetworkingEnabled"), true},
        {QStringLiteral("WirelessEnabled"), true},
        {QStringLiteral("WirelessHardwareEnabled"), true},
        {QStringLiteral("WwanEnabled"), false},
        {QStringLiteral("WwanHardwareEnabled"), true},
        {QStringLiteral("ActiveConnections"),
         QVariant::fromValue(ObjectPaths{})},
        {QStringLiteral("PrimaryConnection"), QVariant::fromValue(root)},
        {QStringLiteral("ActivatingConnection"), QVariant::fromValue(root)},
        {QStringLiteral("Metered"), quint32(NM_METERED_NO)},
        {QStringLiteral("Version"), QStringLiteral("1.56.0")},
        {QStringLiteral("VersionInfo"), QVariant::fromValue(QList<quint32>{})},
        {QStringLiteral("State"), quint32(NM_STATE_DISCONNECTED)},
        {QStringLiteral("Startup"), false},
        {QStringLiteral("Connectivity"), quint32(NM_CONNECTIVITY_NONE)},
        {QStringLiteral("Capabilities"), QVariant::fromValue(QList<quint32>{})},
        {QStringLiteral("RadioFlags"), quint32(0)}};
  }

  [[nodiscard]] QVariantMap deviceProperties() const {
    const QDBusObjectPath root(QStringLiteral("/"));
    return {{QStringLiteral("Interface"), QStringLiteral("wlan0")},
            {QStringLiteral("IpInterface"), QStringLiteral("wlan0")},
            {QStringLiteral("DeviceType"), quint32(NM_DEVICE_TYPE_WIFI)},
            {QStringLiteral("State"), quint32(NM_DEVICE_STATE_DISCONNECTED)},
            {QStringLiteral("Managed"), true},
            {QStringLiteral("Autoconnect"), true},
            {QStringLiteral("FirmwareMissing"), false},
            {QStringLiteral("NmPluginMissing"), false},
            {QStringLiteral("ActiveConnection"), QVariant::fromValue(root)},
            {QStringLiteral("AvailableConnections"),
             QVariant::fromValue(ObjectPaths{})}};
  }

  [[nodiscard]] QVariantMap wirelessProperties() const {
    const ObjectPaths points{
        QDBusObjectPath(QString::fromLatin1(kAccessPointPath))};
    return {{QStringLiteral("AccessPoints"), QVariant::fromValue(points)},
            {QStringLiteral("ActiveAccessPoint"),
             QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))},
            {QStringLiteral("Mode"), quint32(NM_802_11_MODE_INFRA)},
            {QStringLiteral("WirelessCapabilities"), quint32(0)},
            {QStringLiteral("LastScan"), qint64(-1)}};
  }

  [[nodiscard]] QVariantMap accessPointProperties() const {
    const bool hidden = m_security == QLatin1String("hidden");
    const bool wep = m_security == QLatin1String("wep");
    const bool personal = m_security == QLatin1String("wpa2") ||
                          m_security == QLatin1String("wpa3");
    const bool enterprise = m_security == QLatin1String("enterprise");
    quint32 rsnFlags = 0;
    if (m_security == QLatin1String("wpa3")) {
      rsnFlags = NM_802_11_AP_SEC_KEY_MGMT_SAE;
    } else if (personal) {
      rsnFlags = NM_802_11_AP_SEC_KEY_MGMT_PSK;
    } else if (enterprise) {
      rsnFlags = NM_802_11_AP_SEC_KEY_MGMT_802_1X;
    }
    return {{QStringLiteral("Flags"), quint32((wep || personal || enterprise)
                                                  ? NM_802_11_AP_FLAGS_PRIVACY
                                                  : NM_802_11_AP_FLAGS_NONE)},
            {QStringLiteral("WpaFlags"), quint32(0)},
            {QStringLiteral("RsnFlags"), rsnFlags},
            {QStringLiteral("Ssid"),
             hidden ? QByteArray{} : QByteArray("Adapter network")},
            {QStringLiteral("Frequency"), quint32(5'180)},
            {QStringLiteral("HwAddress"), QStringLiteral("02:11:22:33:44:55")},
            {QStringLiteral("Mode"), quint32(NM_802_11_MODE_INFRA)},
            {QStringLiteral("MaxBitrate"), quint32(54'000)},
            {QStringLiteral("Strength"), quint8(70)},
            {QStringLiteral("LastSeen"), qint32(1)}};
  }

  bool handleProperties(const QDBusMessage &message,
                        const QDBusConnection &connection) {
    const QString interface = message.arguments().value(0).toString();
    const QVariantMap values = properties(message.path(), interface);
    if (message.member() == QLatin1String("GetAll")) {
      sendReply(connection, message, values);
      return true;
    }
    if (message.member() == QLatin1String("Get")) {
      const QString name = message.arguments().value(1).toString();
      if (values.contains(name)) {
        sendReply(connection, message,
                  QVariant::fromValue(QDBusVariant(values.value(name))));
      } else {
        sendError(connection, message);
      }
      return true;
    }
    sendError(connection, message);
    return true;
  }

  bool handleManager(const QDBusMessage &message,
                     const QDBusConnection &connection) {
    if (message.member() == QLatin1String("GetPermissions")) {
      sendReply(connection, message,
                QVariant::fromValue(QMap<QString, QString>{}));
      return true;
    }
    if (message.member() == QLatin1String("GetDevices") ||
        message.member() == QLatin1String("GetAllDevices")) {
      sendReply(connection, message,
                QVariant::fromValue(ObjectPaths{
                    QDBusObjectPath(QString::fromLatin1(kDevicePath))}));
      return true;
    }
    if (message.member() == QLatin1String("AddAndActivateConnection")) {
      const QDBusArgument argument =
          message.arguments().value(0).value<QDBusArgument>();
      argument >> m_captured;
      ++m_captureCount;
      QDBusMessage reply = message.createReply();
      reply.setArguments(
          {QVariant::fromValue(QDBusObjectPath(
               QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"))),
           QVariant::fromValue(QDBusObjectPath(QStringLiteral(
               "/org/freedesktop/NetworkManager/ActiveConnection/1")))});
      connection.send(reply);
      return true;
    }
    sendError(connection, message);
    return true;
  }

  bool handleControl(const QDBusMessage &message,
                     const QDBusConnection &connection) {
    if (message.member() == QLatin1String("CaptureCount")) {
      sendReply(connection, message, m_captureCount);
      return true;
    }
    if (message.member() == QLatin1String("CapturedSettings")) {
      sendReply(connection, message, QVariant::fromValue(m_captured));
      return true;
    }
    sendError(connection, message);
    return true;
  }

  static void sendReply(const QDBusConnection &connection,
                        const QDBusMessage &message, const QVariant &value) {
    connection.send(message.createReply({value}));
  }

  static void sendError(const QDBusConnection &connection,
                        const QDBusMessage &message) {
    connection.send(message.createErrorReply(
        QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
        QStringLiteral("Unsupported fake NetworkManager request")));
  }

  QString m_security;
  SettingsMap m_captured;
  quint32 m_captureCount = 0;
};

} // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  if (application.arguments().size() != 2) {
    return 2;
  }
  QindaQt::Network::NetworkManager::TestSupport::registerSettingsMap();
  qDBusRegisterMetaType<ObjectPaths>();
  qDBusRegisterMetaType<QList<quint32>>();
  qDBusRegisterMetaType<QMap<QString, QString>>();
  QDBusConnection bus = QDBusConnection::systemBus();
  if (!bus.isConnected() ||
      !bus.registerService(QString::fromLatin1(kService))) {
    return 3;
  }
  FakeNetworkManager fake(application.arguments().at(1));
  if (!bus.registerVirtualObject(QStringLiteral("/org/freedesktop"), &fake,
                                 QDBusConnection::SubPath)) {
    return 4;
  }
  QTextStream(stdout) << "READY\n" << Qt::flush;
  return application.exec();
}

#include "fake_network_manager.moc"
