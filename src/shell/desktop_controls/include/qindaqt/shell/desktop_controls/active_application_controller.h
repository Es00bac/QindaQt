// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}

namespace QindaQt::Shell::DesktopControls {

struct ActiveApplicationGrants {
  bool windowsRead = false;
  bool windowsManage = false;

  friend bool operator==(const ActiveApplicationGrants &,
                         const ActiveApplicationGrants &) = default;
};

// The GNOME-style "active application" indicator: the focused task-list row's
// application name and icon, with a popup offering Minimize and Close. Truth
// and every intent come from the borrowed task-list facade, which applies its
// own grants, generation fence, and pending marker before any dispatch.
//
// AGENT-CONTRACT: the borrowed facade may be null and must otherwise outlive
// this controller on the GUI thread. This controller never sees compositor
// objects or window identifiers other than the facade's opaque task id.
class ActiveApplicationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available NOTIFY stateChanged)
  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
  Q_PROPERTY(bool hasActiveWindow READ hasActiveWindow NOTIFY stateChanged)
  Q_PROPERTY(QString title READ title NOTIFY stateChanged)
  Q_PROPERTY(QString applicationName READ applicationName NOTIFY stateChanged)
  Q_PROPERTY(QString applicationId READ applicationId NOTIFY stateChanged)
  Q_PROPERTY(QString iconName READ iconName NOTIFY stateChanged)
  Q_PROPERTY(bool iconResolved READ iconResolved NOTIFY stateChanged)
  Q_PROPERTY(QString iconText READ iconText NOTIFY stateChanged)
  Q_PROPERTY(int windowCount READ windowCount NOTIFY stateChanged)
  Q_PROPERTY(bool minimized READ minimized NOTIFY stateChanged)
  Q_PROPERTY(bool pending READ pending NOTIFY stateChanged)
  Q_PROPERTY(QString taskId READ taskId NOTIFY stateChanged)
  Q_PROPERTY(quint64 revision READ revision NOTIFY stateChanged)
  Q_PROPERTY(bool canManage READ canManage NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  ActiveApplicationController(ShellTaskListApplet::TaskListAppletController *taskList,
                              ActiveApplicationGrants grants,
                              QObject *parent = nullptr);

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] bool hasActiveWindow() const noexcept { return !m_row.isEmpty(); }
  [[nodiscard]] QString title() const;
  [[nodiscard]] QString applicationName() const;
  [[nodiscard]] QString applicationId() const;
  [[nodiscard]] QString iconName() const;
  [[nodiscard]] bool iconResolved() const;
  [[nodiscard]] QString iconText() const;
  [[nodiscard]] int windowCount() const;
  [[nodiscard]] bool minimized() const;
  [[nodiscard]] bool pending() const;
  [[nodiscard]] QString taskId() const;
  [[nodiscard]] quint64 revision() const;
  [[nodiscard]] bool canManage() const;
  [[nodiscard]] QString accessibleName() const;
  [[nodiscard]] QString accessibleDescription() const;
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  Q_INVOKABLE bool minimize();
  Q_INVOKABLE bool close();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void feedbackChanged();

private:
  void reproject();
  [[nodiscard]] bool dispatchAllowed(QString *message) const;
  void publishFeedback(const QString &message);

  ShellTaskListApplet::TaskListAppletController *m_taskList = nullptr;
  ActiveApplicationGrants m_grants;
  QVariantMap m_row;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
