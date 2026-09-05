// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}

namespace QindaQt::Shell::DesktopControls {

// Pinned-application strip (Windows classic "quick launch", XFCE launchers).
// The rows ARE the launcher's pinned set: this controller reads the launcher
// facade's query-independent browse projection and re-enters the same facade
// for activation and pin edits, so there is exactly one pinned-application
// authority and one execution path (ADR-0062).
//
// AGENT-CONTRACT: the borrowed launcher may be null and must otherwise
// outlive this controller on the GUI thread.
class QuickLaunchController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
  Q_PROPERTY(int count READ count NOTIFY stateChanged)
  Q_PROPERTY(bool available READ available NOTIFY stateChanged)
  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
  Q_PROPERTY(bool launchGranted READ launchGranted CONSTANT)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  QuickLaunchController(Launcher::LauncherAppletController *launcher,
                        bool applicationsLaunchGranted, QObject *parent = nullptr);

  // Rows: {entryId, displayText, iconName, accessibleName,
  // accessibleDescription, index}.
  [[nodiscard]] QVariantList rows() const { return m_rows; }
  [[nodiscard]] int count() const noexcept
  {
    return static_cast<int>(m_rows.size());
  }
  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] bool launchGranted() const noexcept { return m_launchGranted; }
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  Q_INVOKABLE bool activate(const QString &entryId);
  Q_INVOKABLE bool unpin(const QString &entryId);
  Q_INVOKABLE bool moveUp(const QString &entryId);
  Q_INVOKABLE bool moveDown(const QString &entryId);
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void feedbackChanged();

private:
  void rebuild();
  void publishFeedback(const QString &message);
  [[nodiscard]] bool knownEntry(const QString &entryId) const;

  Launcher::LauncherAppletController *m_launcher = nullptr;
  bool m_launchGranted = false;
  QVariantList m_rows;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
