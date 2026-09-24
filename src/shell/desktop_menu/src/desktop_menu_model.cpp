// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_menu/desktop_menu_model.h"

#include "file_manager_menu_catalog.h"

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

#include <QSet>

#include <utility>

namespace QindaQt::Shell::DesktopMenu {
namespace {

namespace Catalog = Apps::FileManager::MenuCatalog;
using GlobalMenu::Protocol::MenuItem;
using GlobalMenu::Protocol::MenuItemKind;

// Presentation caps, well inside the canonical per-item child limit (128).
constexpr qsizetype kMaxWorkspaces = 64;
constexpr qsizetype kMaxPlaces = 32;
// Workspace names are compositor-supplied; a readable menu never needs more.
constexpr qsizetype kMaxDynamicTextCharacters = 96;

const QString kWorkspaceRadioGroup = QStringLiteral("desktop.workspaces");

[[nodiscard]] bool wellFormed(const QString &text)
{
  for (qsizetype i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c.unicode() == 0) {
      return false;
    }
    if (c.isHighSurrogate()) {
      if (i + 1 >= text.size() || !text.at(i + 1).isLowSurrogate()) {
        return false;
      }
      ++i;
    } else if (c.isLowSurrogate()) {
      return false;
    }
  }
  return true;
}

// Keeps whole code points only, drops NULs and unpaired surrogates, and caps
// the length, so a hostile or oversized name can never invalidate the tree.
[[nodiscard]] QString boundedText(const QString &text, const QString &fallback)
{
  QString result;
  for (qsizetype i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c.isHighSurrogate()) {
      if (i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
        if (result.size() + 2 > kMaxDynamicTextCharacters) {
          break;
        }
        result.append(c);
        result.append(text.at(i + 1));
        ++i;
      }
      continue;
    }
    if (c.isLowSurrogate() || c.unicode() == 0) {
      continue;
    }
    if (result.size() + 1 > kMaxDynamicTextCharacters) {
      break;
    }
    result.append(c);
  }
  result = result.trimmed();
  return result.isEmpty() ? fallback : result;
}

[[nodiscard]] bool idFits(const QString &id)
{
  return wellFormed(id) && id.toUtf8().size() <= GlobalMenu::Protocol::kMaxIdUtf8Bytes;
}

// One top-level menu assembled from groups; separators appear only between
// two non-empty groups, so an omitted capability never leaves a dangling or
// doubled divider.
class MenuAssembly final {
public:
  MenuAssembly(QString id, QString title, QHash<QString, DesktopMenuCommand> &commands)
      : m_id(std::move(id)), m_title(std::move(title)), m_commands(commands)
  {
    m_groups.append(QList<MenuItem>{});
  }

  void nextGroup()
  {
    if (!m_groups.last().isEmpty()) {
      m_groups.append(QList<MenuItem>{});
    }
  }

  void add(const QString &id, const QString &text, Capability capability,
           DesktopMenuCommand command)
  {
    if (!capability.present || !idFits(id) || m_commands.contains(id)) {
      return;
    }
    MenuItem item;
    item.id = id;
    item.kind = MenuItemKind::Action;
    item.text = text;
    item.enabled = capability.enabled;
    m_groups.last().append(item);
    m_commands.insert(id, std::move(command));
  }

  void addCheckable(const QString &id, const QString &text, Capability capability,
                    DesktopMenuCommand command, bool checked,
                    const QString &radioGroup = {})
  {
    add(id, text, capability, std::move(command));
    if (!m_groups.last().isEmpty() && m_groups.last().last().id == id) {
      MenuItem &item = m_groups.last().last();
      item.checkable = true;
      item.checked = checked;
      item.radioGroup = radioGroup;
    }
  }

  // Appends the assembled submenu unless it would be empty.
  void appendTo(QList<MenuItem> &topLevel) const
  {
    MenuItem menu;
    menu.id = m_id;
    menu.kind = MenuItemKind::Submenu;
    menu.text = m_title;
    int separators = 0;
    for (const QList<MenuItem> &group : m_groups) {
      if (group.isEmpty()) {
        continue;
      }
      if (!menu.children.isEmpty()) {
        MenuItem separator;
        separator.id = QStringLiteral("%1.separator-%2").arg(m_id).arg(++separators);
        separator.kind = MenuItemKind::Separator;
        menu.children.append(separator);
      }
      menu.children.append(group);
    }
    if (!menu.children.isEmpty()) {
      topLevel.append(menu);
    }
  }

private:
  QString m_id;
  QString m_title;
  QHash<QString, DesktopMenuCommand> &m_commands;
  QList<QList<MenuItem>> m_groups;
};

[[nodiscard]] QString catalogLabel(const char *actionId)
{
  const std::optional<Catalog::ActionDefinition> definition =
      Catalog::findAction(QString::fromLatin1(actionId));
  return definition ? definition->label : QString{};
}

[[nodiscard]] QString catalogMenuTitle(const char *menuId)
{
  const std::optional<Catalog::MenuDefinition> menu =
      Catalog::findMenu(QString::fromLatin1(menuId));
  return menu ? menu->label : QString{};
}

[[nodiscard]] DesktopMenuCommand command(DesktopCommand kind)
{
  return {.kind = kind, .argument = {}, .revision = 0};
}

void addApplicationMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                        QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.application"), desktopMenuTitle(),
                    commands);
  menu.add(QStringLiteral("desktop.about-computer"), QStringLiteral("About This Computer"),
           facts.aboutComputer, command(DesktopCommand::AboutComputer));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.system-settings"), QStringLiteral("System Settings…"),
           facts.systemSettings, command(DesktopCommand::SystemSettings));
  menu.add(QStringLiteral("desktop.keyboard-shortcuts"),
           QStringLiteral("Keyboard Shortcuts…"), facts.keyboardShortcuts,
           command(DesktopCommand::KeyboardShortcuts));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.lock-screen"), QStringLiteral("Lock Screen"),
           facts.lockScreen, command(DesktopCommand::LockScreen));
  menu.add(QStringLiteral("desktop.log-out"), QStringLiteral("Log Out…"), facts.logOut,
           command(DesktopCommand::LogOut));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.suspend"), QStringLiteral("Suspend"), facts.suspend,
           command(DesktopCommand::Suspend));
  menu.add(QStringLiteral("desktop.restart"), QStringLiteral("Restart…"), facts.restart,
           command(DesktopCommand::Restart));
  menu.add(QStringLiteral("desktop.shut-down"), QStringLiteral("Shut Down…"),
           facts.shutDown, command(DesktopCommand::ShutDown));
  menu.appendTo(topLevel);
}

// AGENT-GUARD (ADR-0260): catalog-backed entries take their id, label, and
// menu title from File Manager's public catalog. They carry NO shortcut text:
// the desktop surface is a focus-less layer (KeyboardInteractivityNone), so
// a File Manager window shortcut shown here would advertise a key that does
// nothing. Show a catalog shortcut only once the desktop context honours it,
// and then only the catalog's own sequence.
void addFileMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                 QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.file"), catalogMenuTitle("file"), commands);
  menu.add(QStringLiteral("file.new-window"), catalogLabel("file.new-window"),
           facts.newFileManagerWindow, command(DesktopCommand::NewFileManagerWindow));
  menu.add(QStringLiteral("file.new-folder"), catalogLabel("file.new-folder"),
           facts.newFolder, command(DesktopCommand::NewFolder));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.find"), QStringLiteral("Find…"), facts.find,
           command(DesktopCommand::Find));
  menu.appendTo(topLevel);
}

void addEditMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                 QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.edit"), catalogMenuTitle("edit"), commands);
  menu.add(QStringLiteral("edit.paste"), catalogLabel("edit.paste"), facts.paste,
           command(DesktopCommand::Paste));
  menu.add(QStringLiteral("edit.select-all"), catalogLabel("edit.select-all"),
           facts.selectAll, command(DesktopCommand::SelectAll));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.clipboard-history"),
           QStringLiteral("Show Clipboard History"), facts.clipboardHistory,
           command(DesktopCommand::ClipboardHistory));
  menu.appendTo(topLevel);
}

void addViewMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                 QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.view"), catalogMenuTitle("view"), commands);
  menu.addCheckable(QStringLiteral("desktop.show-desktop"), QStringLiteral("Show Desktop"),
                    facts.showDesktop, command(DesktopCommand::ShowDesktop),
                    facts.showingDesktop);
  menu.add(QStringLiteral("desktop.gather-overview"), QStringLiteral("Gather Overview"),
           facts.gatherOverview, command(DesktopCommand::GatherOverview));
  menu.nextGroup();
  menu.add(QStringLiteral("desktop.clean-up"), QStringLiteral("Clean Up"), facts.cleanUp,
           command(DesktopCommand::CleanUp));
  menu.appendTo(topLevel);
}

void addGoMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
               QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.go"), catalogMenuTitle("go"), commands);
  const auto placeCommand = [](const QString &placeId) {
    return DesktopMenuCommand{
        .kind = DesktopCommand::OpenPlace, .argument = placeId, .revision = 0};
  };
  const Capability places = facts.places;
  const QString computer = QStringLiteral("computer");
  qsizetype count = 0;
  for (const DesktopPlace &place : facts.placeList) {
    if (count++ >= kMaxPlaces) {
      break;
    }
    if (place.id == computer) {
      continue;
    }
    if (place.id == QLatin1StringView("home")) {
      // The home place is File Manager's own Go ▸ Home Folder.
      menu.add(QStringLiteral("go.home"), catalogLabel("go.home"), places,
               placeCommand(place.id));
      continue;
    }
    menu.add(QStringLiteral("desktop.place.") + place.id,
             boundedText(place.label, place.id), places, placeCommand(place.id));
  }
  menu.nextGroup();
  for (const DesktopPlace &place : facts.placeList) {
    if (place.id == computer) {
      menu.add(QStringLiteral("desktop.place.computer"), boundedText(place.label, computer),
               places, placeCommand(place.id));
      break;
    }
  }
  menu.appendTo(topLevel);
}

void addWindowMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                   QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.window"), QStringLiteral("Window"),
                    commands);
  bool currentSeen = false;
  qsizetype count = 0;
  for (const DesktopWorkspace &workspace : facts.workspaceList) {
    if (count >= kMaxWorkspaces) {
      break;
    }
    const QString id = QStringLiteral("desktop.workspace.") + workspace.id;
    if (workspace.id.isEmpty() || !idFits(id) || commands.contains(id)) {
      continue;
    }
    ++count;
    // A radio group admits one checked member; a hostile snapshot naming two
    // current workspaces checks only the first.
    const bool checked = workspace.current && !currentSeen;
    currentSeen = currentSeen || checked;
    menu.addCheckable(id,
                      boundedText(workspace.name, QStringLiteral("Workspace %1").arg(count)),
                      facts.workspaces,
                      {.kind = DesktopCommand::SwitchWorkspace,
                       .argument = workspace.id,
                       .revision = facts.workspaceRevision},
                      checked, kWorkspaceRadioGroup);
  }
  menu.appendTo(topLevel);
}

void addHelpMenu(const DesktopMenuFacts &facts, QList<MenuItem> &topLevel,
                 QHash<QString, DesktopMenuCommand> &commands)
{
  MenuAssembly menu(QStringLiteral("desktop.menu.help"), QStringLiteral("Help"), commands);
  menu.add(QStringLiteral("desktop.help"), QStringLiteral("QindaQt Help"), facts.help,
           command(DesktopCommand::Help));
  menu.addCheckable(QStringLiteral("desktop.shortcut-note"),
                    QStringLiteral("Keyboard Shortcuts"), facts.shortcutNote,
                    command(DesktopCommand::ShortcutNote), facts.shortcutNoteVisible);
  menu.appendTo(topLevel);
}

} // namespace

BuiltDesktopMenu buildDesktopMenu(const DesktopMenuFacts &facts)
{
  BuiltDesktopMenu built;
  built.tree.revision = 1;
  addApplicationMenu(facts, built.tree.items, built.commands);
  addFileMenu(facts, built.tree.items, built.commands);
  addEditMenu(facts, built.tree.items, built.commands);
  addViewMenu(facts, built.tree.items, built.commands);
  addGoMenu(facts, built.tree.items, built.commands);
  addWindowMenu(facts, built.tree.items, built.commands);
  addHelpMenu(facts, built.tree.items, built.commands);
  return built;
}

QString desktopMenuTitle()
{
  return Catalog::applicationTitle();
}

QString desktopMenuIconName()
{
  return Catalog::applicationIconName();
}

QString fileManagerDesktopEntryId()
{
  return Catalog::desktopEntryId();
}

bool commandNeedsConfirmation(DesktopCommand kind)
{
  return kind == DesktopCommand::LogOut || kind == DesktopCommand::Restart
      || kind == DesktopCommand::ShutDown;
}

// AGENT-NOTE: the same questions the system menu applet asks
// (SystemMenuApplet.qml); a session-ending action reads identically wherever
// it starts.
QString confirmationTitle(DesktopCommand kind)
{
  switch (kind) {
  case DesktopCommand::LogOut:
    return QStringLiteral("Log out?");
  case DesktopCommand::Restart:
    return QStringLiteral("Restart?");
  case DesktopCommand::ShutDown:
    return QStringLiteral("Shut down?");
  default:
    return {};
  }
}

QString confirmationText(DesktopCommand kind)
{
  switch (kind) {
  case DesktopCommand::LogOut:
    return QStringLiteral("Open applications will be asked to close before this session ends.");
  case DesktopCommand::Restart:
    return QStringLiteral("The computer will restart now.");
  case DesktopCommand::ShutDown:
    return QStringLiteral("The computer will shut down now.");
  default:
    return {};
  }
}

} // namespace QindaQt::Shell::DesktopMenu
