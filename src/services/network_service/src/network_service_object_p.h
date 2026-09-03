// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_service/network_service_coordinator.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Network::Service {

class NetworkServiceObject final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Network1")
  Q_CLASSINFO(
      "D-Bus Introspection",
      "<interface name=\"org.qindaqt.Network1\">"
      "<method name=\"GetSnapshot\"><arg name=\"payload\" type=\"ay\" "
      "direction=\"out\"/></method>"
      "<method name=\"RequestScan\"><arg name=\"epoch\" type=\"t\" "
      "direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/>"
      "<arg name=\"deadlineMs\" type=\"x\" direction=\"in\"/><arg "
      "name=\"payload\" type=\"ay\" direction=\"out\"/></method>"
      "<method name=\"ConnectKnownNetwork\"><arg name=\"epoch\" type=\"t\" "
      "direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/>"
      "<arg name=\"knownNetworkId\" type=\"s\" direction=\"in\"/><arg "
      "name=\"payload\" type=\"ay\" direction=\"out\"/></method>"
      "<method name=\"ConnectVisibleNetwork\"><arg name=\"epoch\" type=\"t\" "
      "direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/>"
      "<arg name=\"accessPointId\" type=\"s\" direction=\"in\"/><arg "
      "name=\"payload\" type=\"ay\" direction=\"out\"/></method>"
      "<method name=\"DisconnectActive\"><arg name=\"epoch\" type=\"t\" "
      "direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/>"
      "<arg name=\"deviceInterface\" type=\"s\" direction=\"in\"/><arg "
      "name=\"payload\" type=\"ay\" direction=\"out\"/></method>"
      "<method name=\"SetRadio\"><arg name=\"epoch\" type=\"t\" "
      "direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/>"
      "<arg name=\"radioKind\" type=\"u\" direction=\"in\"/><arg "
      "name=\"enable\" "
      "type=\"b\" direction=\"in\"/><arg name=\"payload\" type=\"ay\" "
      "direction=\"out\"/></method>"
      "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
      "name=\"revision\" type=\"t\"/></signal></interface>")

public:
  explicit NetworkServiceObject(NetworkServiceCoordinator *coordinator,
                                const QDBusConnection &connection,
                                QObject *parent = nullptr);

public Q_SLOTS:
  Q_SCRIPTABLE QByteArray GetSnapshot() const;
  Q_SCRIPTABLE void RequestScan(quint64 epoch, quint64 revision,
                                qint64 deadlineMs);
  Q_SCRIPTABLE void ConnectKnownNetwork(quint64 epoch, quint64 revision,
                                        const QString &knownNetworkId);
  Q_SCRIPTABLE void ConnectVisibleNetwork(quint64 epoch, quint64 revision,
                                          const QString &accessPointId);
  Q_SCRIPTABLE void DisconnectActive(quint64 epoch, quint64 revision,
                                     const QString &deviceInterface);
  Q_SCRIPTABLE void SetRadio(quint64 epoch, quint64 revision, quint32 radioKind,
                             bool enable);

Q_SIGNALS:
  Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);

private:
  void beginOperation(const NetworkServiceRequest &request);
  void finishOperation(quint64 operationId, const OperationResult &result);

  NetworkServiceCoordinator *m_coordinator = nullptr;
  QDBusConnection m_connection;
  QHash<quint64, QDBusMessage> m_pendingReplies;
};

} // namespace QindaQt::Network::Service
