// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace QindaQt::Apps::SettingsInput {

// One selectable keyboard layout option from the xkb rules catalog
// (evdev.xml). `code` values are lowercase xkb layout/variant identifiers.
struct EvdevVariantOption {
    QString code;
    QString description;
};

struct EvdevLayoutOption {
    QString code;
    QString description;
    QList<EvdevVariantOption> variants;
};

// Pure bounded parser for the installed xkb rules catalog. Hostile input
// fails closed: oversized files, DTD/entity tricks, runaway nesting or
// element counts, and absurd identifiers abort with an error and no partial
// catalog. The catalog is display data only; writing layouts never trusts
// values that did not come from a successfully parsed catalog.
namespace EvdevLayoutCatalog {

[[nodiscard]] QList<EvdevLayoutOption>
parse(const QString &xmlPath, QString *error);

} // namespace EvdevLayoutCatalog

} // namespace QindaQt::Apps::SettingsInput
