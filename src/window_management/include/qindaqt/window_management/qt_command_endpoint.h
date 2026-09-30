// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusContext>
#include <QObject>
#include <qindaqt/window_management/controller.h>
namespace QindaQt::WindowManagement {
// Same-thread transport over borrowed policy ports. The caller registers only
// ExportScriptableSlots at the documented object path, after owning its bus
// name, and unregisters before destruction. No provider or launch is activated.
class QtCommandEndpoint final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.WindowManagement1")
public:
  QtCommandEndpoint(Authority &authority, Scene &scene, Executor &executor,
                    QObject *parent = nullptr);
  void invalidate() noexcept;
public Q_SLOTS:
  Q_SCRIPTABLE QByteArray BeginCommand();
  Q_SCRIPTABLE QByteArray ExecuteCommand(const QString &contextId,
                                         const QByteArray &request);
  Q_SCRIPTABLE QByteArray CancelCommand(const QString &contextId);

private:
  QString caller() const;
  Controller m_controller;
};
inline constexpr char CommandObjectPath[] = "/org/qindaqt/WindowManagement";
inline constexpr char CommandInterface[] = "org.qindaqt.WindowManagement1";
} // namespace QindaQt::WindowManagement
