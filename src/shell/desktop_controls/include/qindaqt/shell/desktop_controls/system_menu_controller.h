// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}

namespace QindaQt::Shell::DesktopControls {

// The "system" menu found in the macOS- and MATE-inspired presets: About,
// System Settings, and the session actions. Session buttons re-enter the same
// shell-owned session-actions facade the Power applet borrows (ADR-0070);
// opening Settings re-enters the launcher facade so execution stays behind
// the launcher's bounded seams (ADR-0062).
//
// AGENT-CONTRACT: both borrowed objects may be null and must otherwise outlive
// this controller on the GUI thread. The controller never starts, stops, or
// owns them and never spawns a process itself.
class SystemMenuController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *sessionActions READ sessionActions CONSTANT)
  Q_PROPERTY(bool sessionActionsAvailable READ sessionActionsAvailable CONSTANT)
  Q_PROPERTY(bool canOpenSettings READ canOpenSettings NOTIFY stateChanged)
  Q_PROPERTY(QString productName READ productName CONSTANT)
  Q_PROPERTY(QString versionText READ versionText CONSTANT)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  static QString defaultSettingsEntryId();

  SystemMenuController(QObject *sessionActions,
                       Launcher::LauncherAppletController *launcher,
                       bool applicationsLaunchGranted, QString settingsEntryId,
                       QString versionText, QObject *parent = nullptr);

  [[nodiscard]] QObject *sessionActions() const noexcept { return m_sessionActions; }
  [[nodiscard]] bool sessionActionsAvailable() const noexcept
  {
    return m_sessionActions != nullptr;
  }
  [[nodiscard]] bool canOpenSettings() const;
  [[nodiscard]] QString productName() const { return QStringLiteral("QindaQt"); }
  [[nodiscard]] QString versionText() const { return m_versionText; }
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  // Activates the Settings desktop entry through the launcher facade. Returns
  // false with feedback when the grant, the launcher, or the entry is missing.
  Q_INVOKABLE bool openSettings();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void feedbackChanged();

private:
  void publishFeedback(const QString &message);

  QObject *m_sessionActions = nullptr;
  Launcher::LauncherAppletController *m_launcher = nullptr;
  bool m_launchGranted = false;
  QString m_settingsEntryId;
  QString m_versionText;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
