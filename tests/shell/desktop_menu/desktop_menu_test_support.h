// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixtures for the desktop menu rows (ADR-0260): a complete set of
// facts, and a recording targets port that stands in for the shell's real
// controllers (the runtime row proves those separately).

#include "qindaqt/shell/desktop_menu/desktop_menu_targets.h"
#include "qindaqt/shell/desktop_menu/desktop_menu_types.h"

#include <qindaqt/shell/global_menu/protocol/menu_item.h>

#include <QList>
#include <QString>

namespace QindaQt::Tests::DesktopMenu {

using Shell::DesktopMenu::Capability;
using Shell::DesktopMenu::DesktopCommand;
using Shell::DesktopMenu::DesktopMenuCommand;
using Shell::DesktopMenu::DesktopMenuFacts;
using Shell::DesktopMenu::DesktopPlace;
using Shell::DesktopMenu::DesktopWorkspace;

inline DesktopMenuFacts fullFacts()
{
    DesktopMenuFacts facts;
    const Capability on = Capability::available();
    facts.aboutComputer = on;
    facts.systemSettings = on;
    facts.keyboardShortcuts = on;
    facts.lockScreen = on;
    facts.logOut = on;
    facts.suspend = on;
    facts.restart = on;
    facts.shutDown = on;
    facts.newFileManagerWindow = on;
    facts.find = on;
    facts.help = on;
    facts.newFolder = on;
    facts.paste = on;
    facts.selectAll = on;
    facts.cleanUp = on;
    facts.clipboardHistory = on;
    facts.showDesktop = on;
    facts.gatherOverview = on;
    facts.places = on;
    facts.placeList = {{QStringLiteral("home"), QStringLiteral("Home")},
                       {QStringLiteral("desktop"), QStringLiteral("Desktop")},
                       {QStringLiteral("documents"), QStringLiteral("Documents")},
                       {QStringLiteral("downloads"), QStringLiteral("Downloads")},
                       {QStringLiteral("music"), QStringLiteral("Music")},
                       {QStringLiteral("pictures"), QStringLiteral("Pictures")},
                       {QStringLiteral("videos"), QStringLiteral("Videos")},
                       {QStringLiteral("computer"), QStringLiteral("Computer")}};
    facts.workspaces = on;
    facts.workspaceList = {{QStringLiteral("ws-1"), QStringLiteral("Main"), true},
                           {QStringLiteral("ws-2"), QStringLiteral("Games"), false}};
    facts.workspaceRevision = 7;
    facts.shortcutNote = on;
    return facts;
}

inline const Shell::GlobalMenu::Protocol::MenuItem *
findItem(const QList<Shell::GlobalMenu::Protocol::MenuItem> &items, const QString &id)
{
    for (const auto &item : items) {
        if (item.id == id) {
            return &item;
        }
        if (const auto *child = findItem(item.children, id)) {
            return child;
        }
    }
    return nullptr;
}

class RecordingTargets final : public Shell::DesktopMenu::DesktopMenuTargets {
public:
    DesktopMenuFacts current = fullFacts();
    QList<DesktopMenuCommand> performed;
    bool accept = true;

    [[nodiscard]] DesktopMenuFacts facts() const override { return current; }
    bool perform(const DesktopMenuCommand &command) override
    {
        performed.append(command);
        return accept;
    }
    [[nodiscard]] QString lastFailure() const override
    {
        return accept ? QString{} : QStringLiteral("owner-refused");
    }
    void change(DesktopMenuFacts facts)
    {
        current = std::move(facts);
        Q_EMIT factsChanged();
    }
};

} // namespace QindaQt::Tests::DesktopMenu
