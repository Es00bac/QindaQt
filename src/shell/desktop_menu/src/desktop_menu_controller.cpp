// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_menu/desktop_menu_controller.h"

#include "qindaqt/shell/desktop_menu/desktop_menu_targets.h"

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

namespace QindaQt::Shell::DesktopMenu {

DesktopMenuController::DesktopMenuController(GlobalMenu::GlobalMenuAppletAccess &facade,
                                             DesktopMenuTargets &targets, QObject *parent)
    : QObject(parent)
    , m_facade(facade)
    , m_targets(targets)
{
  m_retireTimer.setSingleShot(true);
  m_retireTimer.setInterval(kRetireGraceMilliseconds);
  connect(&m_retireTimer, &QTimer::timeout, this, &DesktopMenuController::retire);
  connect(&m_targets, &DesktopMenuTargets::factsChanged, this,
          &DesktopMenuController::refresh);
  connect(&m_facade, &GlobalMenu::GlobalMenuAppletAccess::desktopActivationRequested,
          this, &DesktopMenuController::activate);
  connect(&m_facade, &GlobalMenu::GlobalMenuAppletAccess::confirmationResolved, this,
          &DesktopMenuController::resolveConfirmation);
}

DesktopMenuController::~DesktopMenuController()
{
  m_enabled = false;
  evaluate();
}

void DesktopMenuController::setEnabled(bool enabled)
{
  if (m_enabled == enabled) {
    return;
  }
  m_enabled = enabled;
  evaluate();
}

void DesktopMenuController::setPresence(Presence presence)
{
  m_presence = presence;
  evaluate();
}

void DesktopMenuController::refresh()
{
  // Inert and absent menus are rebuilt when they next become actionable, so
  // an activation can only ever map through the table of the tree the facade
  // is presenting.
  if (m_enabled && m_shown == Shown::Active) {
    publish();
  }
}

void DesktopMenuController::evaluate()
{
  if (!m_enabled) {
    withdraw();
    // AGENT-GUARD: a question asked by a menu that no longer exists must not
    // survive it; an answer arriving later then matches no token.
    if (!m_pendingToken.isEmpty()) {
      m_facade.withdrawConfirmation(m_pendingToken);
      m_pendingToken.clear();
    }
    return;
  }
  switch (m_presence) {
  case Presence::NoApplication:
    m_retireTimer.stop();
    publish();
    return;
  case Presence::ApplicationActive:
    // AGENT-GUARD: never actionable for the new focus. The application's
    // first tree replaces this inert one in the facade the moment it lands;
    // the grace only bounds how long a menu-less application keeps it.
    if (m_shown == Shown::Active) {
      m_shown = Shown::Inert;
      m_facade.retainDesktopTreeInert();
    }
    if (m_shown == Shown::Inert && !m_retireTimer.isActive()) {
      m_retireTimer.start();
    }
    return;
  case Presence::Unknown:
    // AGENT-NOTE: the identity channel withdraws and rereads on every
    // visibility change, including this menu's own popup appearing. Keeping
    // the menu actionable across that sub-second gap is what keeps an open
    // popup alive; the timer (never re-armed while running) bounds it.
    if (m_shown != Shown::None && !m_retireTimer.isActive()) {
      m_retireTimer.start();
    }
    return;
  }
}

void DesktopMenuController::publish()
{
  m_menu = buildDesktopMenu(m_targets.facts());
  if (!GlobalMenu::Protocol::validateMenuTree(m_menu.tree).accepted) {
    // The builder guarantees validity; a defect here must fail closed, never
    // present part of a menu.
    m_lastFailure = QStringLiteral("invalid-desktop-menu");
    m_menu = {};
    withdraw();
    return;
  }
  m_facade.publishDesktopTree(m_menu.tree, desktopMenuTitle(), desktopMenuIconName());
  m_shown = Shown::Active;
}

void DesktopMenuController::withdraw()
{
  m_retireTimer.stop();
  if (m_shown != Shown::None) {
    m_shown = Shown::None;
    m_facade.withdrawDesktopTree();
  }
}

void DesktopMenuController::retire()
{
  if (m_enabled && m_presence == Presence::NoApplication) {
    return;
  }
  withdraw();
}

void DesktopMenuController::activate(const QString &actionId)
{
  // The facade admits only enabled actions of the tree it presents; this
  // table is that tree's own, so an unknown id is simply not ours.
  if (!m_enabled || m_shown != Shown::Active) {
    return;
  }
  const auto found = m_menu.commands.constFind(actionId);
  if (found == m_menu.commands.cend()) {
    return;
  }
  const DesktopMenuCommand command = *found;
  if (commandNeedsConfirmation(command.kind)) {
    // The token is set before asking, so the facade declining an older
    // unanswered question cannot be mistaken for this one's answer.
    m_pendingToken = QStringLiteral("desktop-menu-%1").arg(++m_confirmationSerial);
    m_pendingCommand = command;
    m_facade.requestConfirmation(m_pendingToken, confirmationTitle(command.kind),
                                 confirmationText(command.kind));
    return;
  }
  perform(command);
}

void DesktopMenuController::resolveConfirmation(const QString &token, bool accepted)
{
  if (token.isEmpty() || token != m_pendingToken) {
    return;
  }
  const DesktopMenuCommand command = m_pendingCommand;
  m_pendingToken.clear();
  m_pendingCommand = {};
  if (!accepted) {
    return;
  }
  // AGENT-GUARD: the answer can arrive long after the question. Re-check the
  // owner's admission now so an action withdrawn meanwhile (session service
  // gone, layout switched away) is refused instead of dispatched.
  if (!m_enabled || !stillAdmitted(command)) {
    m_lastFailure = QStringLiteral("no-longer-available");
    Q_EMIT commandPerformed(command, false);
    return;
  }
  perform(command);
}

bool DesktopMenuController::stillAdmitted(const DesktopMenuCommand &command) const
{
  const DesktopMenuFacts facts = m_targets.facts();
  const auto admitted = [](const Capability &capability) {
    return capability.present && capability.enabled;
  };
  switch (command.kind) {
  case DesktopCommand::LogOut:
    return admitted(facts.logOut);
  case DesktopCommand::Restart:
    return admitted(facts.restart);
  case DesktopCommand::ShutDown:
    return admitted(facts.shutDown);
  default:
    return true;
  }
}

void DesktopMenuController::perform(const DesktopMenuCommand &command)
{
  const bool accepted = m_targets.perform(command);
  m_lastFailure = accepted ? QString{} : m_targets.lastFailure();
  Q_EMIT commandPerformed(command, accepted);
}

} // namespace QindaQt::Shell::DesktopMenu
