// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QDBusMessage>
#include <QJsonArray>
#include <QJsonDocument>
#include <qindaqt/window_management/qt_command_endpoint.h>
namespace QindaQt::WindowManagement {
QtCommandEndpoint::QtCommandEndpoint(Authority &authority, Scene &scene,
                                     Executor &executor, QObject *parent)
    : QObject(parent), m_controller(authority, scene, executor) {}
QString QtCommandEndpoint::caller() const {
  return calledFromDBus() ? message().service() : QString{};
}
QByteArray QtCommandEndpoint::BeginCommand() {
  const auto result = m_controller.begin(caller());
  auto wire = QJsonDocument::fromJson(encodeResult(result)).object();
  if (result.status == Status::Accepted)
    wire.insert(QStringLiteral("capabilities"),
                QJsonArray::fromStringList(m_controller.capabilities()));
  return QJsonDocument(wire).toJson(QJsonDocument::Compact);
}
QByteArray QtCommandEndpoint::ExecuteCommand(const QString &contextId,
                                             const QByteArray &request) {
  return encodeResult(m_controller.submit(caller(), contextId, request));
}
QByteArray QtCommandEndpoint::CancelCommand(const QString &contextId) {
  return encodeResult(m_controller.cancel(caller(), contextId));
}
void QtCommandEndpoint::invalidate() noexcept { m_controller.invalidate(); }
} // namespace QindaQt::WindowManagement
