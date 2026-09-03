// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>
#include <qindaqt/shell/global_menu/exporter/menu_source.h>

#include <QtCore/QUuid>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

inline constexpr qsizetype kMaxIconNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxIconDataBytes = 256 * 1024;
inline constexpr qsizetype kMaxPropertiesPerItem = 32;
inline constexpr qsizetype kMaxPropertyEntries = 1024;
inline constexpr qsizetype kMaxRemovedPropertyNames = 4096;
inline constexpr qsizetype kMaxShortcutSequences = 4;
inline constexpr qsizetype kMaxShortcutTokens = 8;

struct DecodeResult final {
    bool accepted = false;
    QString reasonCode;
    Exporter::MenuSnapshot snapshot;
};

// Strictly converts one complete standard layout into the canonical model.
// Known properties are type/bounds checked, unknown properties are ignored,
// and any malformed or over-limit subtree rejects the entire snapshot. The
// returned snapshot owns its canonical value tree and is safe to retain.
[[nodiscard]] DecodeResult decodeLayout(const QUuid &ownerWindowId, quint32 remoteRevision,
                                        const LayoutItem &root);
[[nodiscard]] bool validatePropertyUpdates(const PropertyEntryList &updated,
                                           const RemovedPropertyEntryList &removed,
                                           QString *reasonCode = nullptr);

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
