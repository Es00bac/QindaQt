// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_catalog.h"

#include <QString>
#include <QStringList>

namespace QindaQt::Shell::RuntimeLayoutAdoption {

// What a single adoption attempt did to the catalog.
enum class Outcome {
    // A different saved profile id was selected from the reloaded catalog.
    AdoptedSelection,
    // The same profile was reloaded from disk (the Customize route rewrote
    // the user-store copy of the current layout).
    AdoptedContent,
    // The saved id is unknown after reload; the prior selection was
    // restored. The running layout is unchanged.
    KeptPriorSelection,
    // An explicit --profile lock outranks the saved selection for this
    // process lifetime; nothing was reloaded.
    LockedByCommandLine,
    // The catalog reload itself failed; the catalog is unchanged and the
    // running layout stays as it is.
    Failed
};

// Reloads the profile catalog from the remembered directories and selects
// the preferred layout, following the startup precedence rules: an explicit
// command-line selection always wins, and an unknown saved id fails closed
// to the prior selection instead of stranding the shell without a layout.
// An empty preferred id adopts content only (keep the current selection).
// The catalog's loadDirectories resets the current index, so this function
// always re-selects explicitly before returning a non-failing outcome.
Outcome reloadAndSelect(Profiles::ProfileCatalog &catalog,
                        const QStringList &profileDirectories,
                        const QString &preferredProfileId,
                        bool lockedByCommandLine, QString *diagnostic);

} // namespace QindaQt::Shell::RuntimeLayoutAdoption
