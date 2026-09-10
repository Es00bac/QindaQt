// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_layout_adoption.h"

#include <QVariant>

namespace QindaQt::Shell::RuntimeLayoutAdoption {

Outcome reloadAndSelect(Profiles::ProfileCatalog &catalog,
                        const QStringList &profileDirectories,
                        const QString &preferredProfileId,
                        bool lockedByCommandLine, QString *diagnostic)
{
    if (lockedByCommandLine) {
        if (diagnostic != nullptr) {
            *diagnostic = QStringLiteral(
                "an explicit --profile selection outranks the saved layout");
        }
        return Outcome::LockedByCommandLine;
    }

    const QString priorId = catalog.current()
                                .value(QStringLiteral("id")).toString();
    QString loadError;
    // A failed reload leaves the catalog contents untouched: the loader only
    // swaps the profile list after every directory parsed successfully.
    if (!catalog.loadDirectories(profileDirectories, &loadError)) {
        if (diagnostic != nullptr) {
            *diagnostic = loadError;
        }
        return Outcome::Failed;
    }

    const QString target = preferredProfileId.isEmpty() ? priorId
                                                         : preferredProfileId;
    if (catalog.selectById(target)) {
        return target == priorId ? Outcome::AdoptedContent
                                 : Outcome::AdoptedSelection;
    }

    if (diagnostic != nullptr) {
        *diagnostic = QStringLiteral(
                          "saved layout profile '%1' is unknown; keeping '%2'")
                          .arg(target, priorId);
    }
    if (catalog.selectById(priorId)
        || catalog.selectById(QStringLiteral("qindaqt"))) {
        return Outcome::KeptPriorSelection;
    }
    if (diagnostic != nullptr) {
        *diagnostic = QStringLiteral(
                          "neither '%1' nor the default profile survived the "
                          "catalog reload").arg(priorId);
    }
    return Outcome::Failed;
}

} // namespace QindaQt::Shell::RuntimeLayoutAdoption
