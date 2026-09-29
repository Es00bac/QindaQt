// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Session {

// Carries a user's KDE KWin settings into qindaqt-kwin's own files once
// (ADR-0289): monitor layout, window rules, input and keyboard settings, and
// KWin's state. For each pair, the KDE file is copied only while the
// qindaqt-kwin file does not exist yet; a present qindaqt-kwin file is never
// read from or written to here, so KDE and QindaQt settings diverge freely
// afterwards and a stock Plasma session never sees QindaQt's changes.
//
// Runs in qindaqt-wm before SessionDefaults and before the compositor starts.
// Not thread-safe; call it from one process per session start.
class CompositorConfigImport final
{
public:
    struct Pair
    {
        QString kdeFile;   // relative to its home, e.g. "kcminputrc"
        QString qindaqtFile; // relative to its home, e.g. "qindaqt/kwininputrc"
        bool state = false; // $XDG_STATE_HOME instead of $XDG_CONFIG_HOME
    };

    [[nodiscard]] static QList<Pair> pairs();

    // Copies each missing qindaqt-kwin file from its KDE counterpart. Returns
    // false on the first copy that fails, with a readable error; files copied
    // before it stay imported. `imported` lists the qindaqt-kwin files created.
    [[nodiscard]] static bool run(const QString &configHome,
                                  const QString &stateHome,
                                  QStringList *imported = nullptr,
                                  QString *error = nullptr);
};

} // namespace QindaQt::Session
