// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_item.h>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QUuid>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// One exported menu snapshot and its ownership lineage. `ownerWindowId` names
// the window whose menu this is; it is opaque compositor-authenticated
// identity, not a wire key by itself. `epoch` changes only when the owning
// window identity changes (a fresh window, not merely a content refresh);
// `revision` advances on every accepted content change within one epoch. This
// mirrors the owner/epoch/revision lineage used by Display1, Audio1, and
// Settings1 so a stale snapshot can always be detected without a live
// connection. See ActiveProviderSelector and MenuExporter for who assigns
// these fields; MenuItem itself never carries lineage.
struct MenuTree final {
    QUuid ownerWindowId;
    QUuid epoch;
    quint64 revision = 0;
    QList<MenuItem> items;

    bool operator==(const MenuTree &) const = default;
};

} // namespace QindaQt::Shell::GlobalMenu::Protocol
