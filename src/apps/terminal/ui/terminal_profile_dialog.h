// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: TerminalProfileDialog is the bounded, keyboard-accessible
// editor for the user profile list and the two persisted policy values
// (default profile, restore-tabs flag). It owns no transport: the caller
// passes the last confirmed values at construction and reads the edited
// draft back after exec() returns Accepted, then drives the Settings1
// commit. The built-in default profile is listed for reference and as the
// default-profile target but is immutable here; every accepted profile
// passes validateTerminalProfile, so a rejected draft never leaves the
// dialog.
class TerminalProfileDialog final : public QDialog {
  Q_OBJECT

public:
  TerminalProfileDialog(const QList<TerminalProfile> &userProfiles,
                        const QString &defaultProfileId, bool restoreTabs,
                        const QStringList &themeIds,
                        QWidget *parent = nullptr);

  // Valid only after Accepted; the caller commits these through the
  // Settings1 controller (which re-validates before encoding).
  [[nodiscard]] QList<TerminalProfile> userProfiles() const {
    return m_userProfiles;
  }
  [[nodiscard]] QString defaultProfileId() const {
    return m_defaultProfileId;
  }
  [[nodiscard]] bool restoreTabs() const { return m_restoreTabs; }

  // Completes the pending caller-owned Settings1 operation. Success closes
  // the dialog only after the asynchronous result is known; every other
  // outcome remains visible and re-enables the unchanged draft for explicit
  // correction or re-apply.
  void finishApply(bool allApplied, const QString &accessibleStatus);

signals:
  void applyRequested();

protected:
  void accept() override;

private:
  void buildUi(const QStringList &themeIds);
  void refreshList();
  void loadSelectedIntoFields();
  void writeFieldsToSelected();
  [[nodiscard]] bool selectedIsBuiltin() const;
  [[nodiscard]] TerminalProfile *selectedProfile();

  QList<TerminalProfile> m_userProfiles;
  QString m_defaultProfileId;
  bool m_restoreTabs = false;

  QListWidget *m_list = nullptr;
  QLineEdit *m_name = nullptr;
  QLineEdit *m_shellProgram = nullptr;
  QPlainTextEdit *m_shellArguments = nullptr;
  QLineEdit *m_fontFamily = nullptr;
  QSpinBox *m_fontSize = nullptr;
  QComboBox *m_colorScheme = nullptr;
  QSpinBox *m_scrollback = nullptr;
  QComboBox *m_bellPolicy = nullptr;
  QCheckBox *m_restoreTabsCheck = nullptr;
  QWidget *m_editingSurface = nullptr;
  QLabel *m_applyStatus = nullptr;
  QDialogButtonBox *m_buttons = nullptr;
  bool m_loadingFields = false;
  bool m_applyInFlight = false;
};

} // namespace QindaQt::Apps::Terminal
