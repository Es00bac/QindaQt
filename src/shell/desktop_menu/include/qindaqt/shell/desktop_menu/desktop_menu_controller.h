// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_menu/desktop_menu_model.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace QindaQt::Shell::GlobalMenu {
class GlobalMenuAppletAccess;
}

namespace QindaQt::Shell::DesktopMenu {

class DesktopMenuTargets;

// The shell-owned desktop menu provider (ADR-0260): publishes the File
// Manager's menu into the global menu facade's DESKTOP channel while no
// application is active, and turns each admitted activation into exactly one
// command on the borrowed targets port.
//
// Selection follows one observation, `Presence`, which shell composition
// derives from the compositor-authenticated active-window identity (the same
// exact-owner client the global menu already uses; the compositor reports the
// desktop surface itself as "no active window"):
// - NoApplication: publish (actionable).
// - ApplicationActive: an application's menu takes over. A shown desktop menu
//   turns inert at once (never actionable for the new focus) and is withdrawn
//   after kRetireGraceMilliseconds unless the facade already presents the
//   application's first tree (it always wins while it exists).
// - Unknown (the identity channel is rereading after an invalidation): a
//   shown menu stays actionable, because opening its own popup invalidates
//   the identity; it is withdrawn if nothing re-proves the empty state within
//   the same grace.
// Nothing here touches application-menu ownership, authentication, or the
// local/global hosting acknowledgment (ADR-0033/0056/0077): the desktop
// channel is a separate facade input the transport coordinator never reads.
//
// AGENT-CONTRACT: GUI-thread only. The facade and targets are borrowed and
// must outlive this controller; destruction and setEnabled(false) withdraw
// the desktop channel and any outstanding confirmation.
class DesktopMenuController final : public QObject {
  Q_OBJECT

public:
  enum class Presence {
    Unknown,
    NoApplication,
    ApplicationActive,
  };

  // Matches the transport coordinator's presentation grace, so the bar hands
  // over in either direction on the same clock.
  static constexpr int kRetireGraceMilliseconds = 500;

  DesktopMenuController(GlobalMenu::GlobalMenuAppletAccess &facade,
                        DesktopMenuTargets &targets, QObject *parent = nullptr);
  ~DesktopMenuController() override;

  DesktopMenuController(const DesktopMenuController &) = delete;
  DesktopMenuController &operator=(const DesktopMenuController &) = delete;

  // True while the adopted layout hosts a granted global menu (ADR-0130
  // layouts without one never show a desktop menu).
  void setEnabled(bool enabled);
  void setPresence(Presence presence);

  [[nodiscard]] bool enabled() const noexcept { return m_enabled; }
  [[nodiscard]] Presence presence() const noexcept { return m_presence; }
  // True while the desktop channel holds a tree (actionable or inert).
  [[nodiscard]] bool published() const noexcept { return m_shown != Shown::None; }
  [[nodiscard]] bool actionable() const noexcept { return m_shown == Shown::Active; }
  // The last built menu (tree and id -> command table).
  [[nodiscard]] const BuiltDesktopMenu &menu() const noexcept { return m_menu; }
  [[nodiscard]] QString lastFailure() const { return m_lastFailure; }

public Q_SLOTS:
  // Rebuilds from the targets' current facts and republishes when actionable.
  void refresh();

Q_SIGNALS:
  // One per dispatched command, after the owning controller answered.
  void commandPerformed(QindaQt::Shell::DesktopMenu::DesktopMenuCommand command,
                        bool accepted);

private:
  enum class Shown {
    None,
    Active,
    Inert,
  };

  void evaluate();
  void publish();
  void withdraw();
  void retire();
  void activate(const QString &actionId);
  void resolveConfirmation(const QString &token, bool accepted);
  void perform(const DesktopMenuCommand &command);
  [[nodiscard]] bool stillAdmitted(const DesktopMenuCommand &command) const;

  GlobalMenu::GlobalMenuAppletAccess &m_facade;
  DesktopMenuTargets &m_targets;
  BuiltDesktopMenu m_menu;
  QTimer m_retireTimer;
  bool m_enabled = false;
  Presence m_presence = Presence::Unknown;
  Shown m_shown = Shown::None;
  quint64 m_confirmationSerial = 0;
  QString m_pendingToken;
  DesktopMenuCommand m_pendingCommand;
  QString m_lastFailure;
};

} // namespace QindaQt::Shell::DesktopMenu
